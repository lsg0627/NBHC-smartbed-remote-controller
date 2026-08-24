//////////////////////////////////////////////////////////////////////////////////////////////////
//
// --------- System Timer ----------
// system clock : PCLK(system clock / 2) = 50MHz
// pre-scale : 8192
// time count interval : 8192/50MHz = 164uSec
// 1msec = 164uSec * 6 = 1mSec
// 한글도 되는데
//
//////////////////////////////////////////////////////////////////////////////////////////////////
#include "sdk.h"
#include "../app/main.h"
#include "../drive/smart_bed_remocon.h"


EGL_FONT* g_pFont48;
EGL_FONT* g_pFont40;
EGL_FONT* g_pFont32;
EGL_FONT* g_pFont28;
EGL_FONT* g_pFont16;
EGL_FONT* g_pFontKor = NULL;
EGL_FONT* g_pFontKor16 = NULL;

// 전원 ON 직후 로딩 화면 (boot.suf) 표시 — PDF 슬라이드 3 → 4 전환
// (process_target_time_handler / home_draw / remocon_power_ctrl 모두 참조)
bool show_loading_screen = false;
U32 loading_remain = 0;
U32 loading_anim_phase = 0;	// 스피너 회전 위상 (0~7)

// 5초 무입력 자동 홈 복귀 타이머
U32 auto_home_timer = 0;

// 로컬 상태 잠금 — 사용자 키 동작 후 ESP32 패킷이 current_mode/run_state를 덮어쓰지 못하게 함
// 100ms 틱 단위 (30 = 3초)
U32 bed_state_lock_remain = 0;

// 볼륨 잠금 — 음량 조절 직후 ESP32 상태 패킷이 LED/카운터를 되돌리지 못하게 함 (100ms 틱)
U32 vol_lock_remain = 0;

// stdby 시작 시점의 mode 기억 — "종료중" vs "초기화 중" 일관성 유지
// stdby_initial_mode != 0 → "종료중" (모드 종료 중)
// stdby_initial_mode == 0 → "초기화 중" (idle 상태에서 전원 누름)
U8 stdby_initial_mode = 0;

// 외부(태블릿) 종료 감지 — ESP32 BED_STATUS의 state 전환으로 detect
// state 1/2 → state 0/3 전환 시 set, mode=0/state=0 안정 시 clear
bool external_stopping = false;
U32 external_stopping_timeout = 0;		// 100ms 틱 단위 (200 = 20초 안전 timeout)
U32 external_stopping_min_remain = 0;	// 최소 표시 시간 (clear 방지)

// 모드 시작 ACK 직후 → state=3 transient를 stopping으로 잘못 감지하는 것 방지
bool pending_mode_start = false;
U32 pending_mode_start_timeout = 0;	// 100ms 틱 단위

// 모드 전환 시 ESP32 홈(init) 진행 중 표시 — 경과시간 정지 + "초기화 중" 텍스트용.
// CONFORM 시점 세팅, ESP32의 2번째 state=1 수신 시 클리어. 안전 timeout 60초.
bool init_home_pending = false;
U32 init_home_pending_timeout = 0;	// 100ms 틱 단위 (600 = 60초)
U8 init_home_state1_count = 0;		// 이번 사이클의 state=1 packet 개수 (2번째에 clear)

// 리모컨 전원 ON 직후 → 첫 BED_STATUS로 침대 상태 확인 후 CMD2_PWR_ON 송신 여부 결정
// idle(state=0)이면 CMD2_PWR_ON 송신 (ESP32에서 1.mp3 + 홈 복귀 루틴 수행)
// running/paused이면 송신 안 함 (태블릿 동작 모드 유지)
bool pending_pwr_on_check = false;
U32 pending_pwr_on_check_timeout = 0;	// 100ms 틱 단위 (30 = 3초)

// CMD2_PWR_ON 송신 후 ESP32의 state=3 (호밍 시작) BED_STATUS 도착 대기
// 도착 전까지 로딩 스피너 유지 → 빈 home 화면 노출 방지
bool awaiting_homing_start = false;
U32 awaiting_homing_timeout = 0;	// 100ms 틱 단위 (50 = 5초)

// --- 부팅 확인창 (docs/remote_firmware_spec.md §5) ---
// startup_pending=1이면 확인 없이 홈을 돌리지 않는다. LCD가 꺼진 부팅 시점이 아니라
// 사용자가 전원키를 눌러 power=true가 되는 시점에 확인창을 띄운다.
bool startup_confirm_active = false;
bool conform_key_held = false;
U32  conform_hold_cnt = 0;		// 100ms 틱 단위 (CONFORM_HOLD_TICKS = 30 = 3초)

// --- 부팅 시 GetBedStatus(0xF1) 요청 (§3) ---
// LCD가 꺼진 상태에서도 송신한다. 3회 실패하면 브로드캐스트 대기로 fallback.
bool boot_status_done = false;
U8   boot_status_retry = 3;
U32  boot_status_wait = 0;
bool master_booted = false;	// 마스터의 첫 BedStatus를 실제로 수신했는지 (부팅 완료 판정)
							// boot_status_done은 fallback(~600ms)으로도 켜지므로 구분 불가 → 별도 플래그

// --- 통신 끊김 감지 (§7) ---
U32  bed_status_silence = 0;
bool link_lost = false;

// --- 전이 감지용 이전 값 (Path B / §5.4) ---
U8 prev_startup_pending = 0;

// 마사지 모드 사이클 타이머 (1회 측정 시간 기준, 한 사이클 끝나면 wrap)
U32 massage_timer_elapsed_ms = 0;	// 누적 ms (100ms 틱마다 +100)
U8 massage_timer_mode = 0xFF;		// 0~11: 마사지 인덱스, 0xFF: 비활성

// VAIRANCE/LEVITATE 모드 경과 시간 (HOME 시간 박스에 카운트업 표시)
// 새 시작(state=0→1 또는 mode 변경) 시 리셋, state=1 동안만 누적
U32 mode_timer_elapsed_ms = 0;
U8  mode_timer_mode = 0;	// 현재 추적 중인 mode 코드 (0 = 비활성)

// 1회 사이클 시간 (ms) — 실측치
const U32 massage_durations_ms[12] = {
	779000,	// 0: 파도타기 (12:59)
	766572,	// 1: 지압 (12:46)
	991288,	// 2: 집중 (16:31)
	1129528,	// 3: 추나 (18:49)
	717511,	// 4: 스트레칭 (11:57)
	675432,	// 5: 트위스트 (11:15)
	336304,	// 6: 트렌델렌버그 (5:36)
	1093265,	// 7: 롤링 (18:13)
	488301,	// 8: 호흡 (8:08)
	1014757,	// 9: 시소 (16:54)
	421573,	// 10: 무중력 (7:01)
	565385,	// 11: 수면 (9:25)
};

// 모드명 마퀴 스크롤 상태 (홈 화면 좌측 박스, 동작 중 모드명이 길 때)
U32 mode_text_offset = 0;	// 현재 스크롤 오프셋 (픽셀)
U32 mode_text_pause = 0;	// 끝 도달 후 일시정지 카운트 (1초 = 10틱)
U32 mode_text_width = 0;	// 현재 모드명 폭 (캐시)
U8  mode_text_last_mode = 0xFF;	// 모드 변경 감지용
U8  mode_text_last_state = 0xFF;	// 상태 변경 감지용 (동작중/일시정지/종료 전환)
bool mode_text_last_stdby = false;	// stdby_in_progress 변경 감지용
bool mode_text_last_external_stopping = false;	// external_stopping 변경 감지용
bool mode_text_scrolling = false;	// 스크롤 필요 여부

// 모드명 텍스트 폭 계산.
// - Pretendard bmpfont: EGL API로 실제 폰트 metric (프로포셔널 대응)
// - SDK bitfont: UTF-8 못 다루므로 기존 고정폭 계산 유지 (한글=28, ASCII=14)
U32 estimate_text_width_28(const char* utf8_str)
{
	if(!utf8_str) return 0;
#if USE_PRETENDARD_FONT
	if(!g_pFontKor) return 0;
	int w = text_width(g_pFontKor, utf8_str);
	return (w < 0) ? 0 : (U32)w;
#else
	U32 width = 0;
	int i = 0;
	while(utf8_str[i]) {
		U8 c = (U8)utf8_str[i];
		if(c < 0x80)            { width += 14; i += 1; }
		else if((c & 0xE0) == 0xC0) { width += 28; i += 2; }
		else if((c & 0xF0) == 0xE0) { width += 28; i += 3; }
		else                    { i += 1; }
	}
	return width;
#endif
}
//VAIRANCE_ST vairance;
//VAIRANCE_ST vairance_temp;

//U8 bar_status[16] = {0, };


//////////////////////////////////////////////////////////////////////////////////////////////////
//
// System Timer
// Timer Channel : SYS_TIMER_CH
// System Time = (8912/ APB CLK)*DIVIDE
// 1msec = (8912/50MHz)*6
//
//////////////////////////////////////////////////////////////////////////////////////////////////
void sys_timer_set(void){
	// Timer mode, 512 pre-scale, timer enable
	*((volatile unsigned int*)TMCTRL_ADDR(SYS_TIMER_CH))= F_TMCTRL_TMOD_TIMER | F_TMCTRL_PFSEL_8192 | F_TMCTRL_TMEN;
	*((volatile unsigned int*)TPCTRL_ADDR(SYS_TIMER_CH)) = F_TPCTRL_CNTCLR | F_TPCTRL_CLKSEL_SCLK ;// Timer counter reset
	*((volatile unsigned int*)TPCTRL_ADDR(SYS_TIMER_CH)) = F_TPCTRL_CLKSEL_SCLK ;// Timer counter run
}

// timer count
// old_time_cnt : before timer count
// return : 

U16 time_1msec_interval_get(U16 old_timer_cnt){
	U16 cur_timer_cnt;
	
	cur_timer_cnt = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
	if(cur_timer_cnt >= old_timer_cnt){
		return ((cur_timer_cnt - old_timer_cnt) / SYS_TICK_1MS_DIV);
	}
	
	U16 tmp = 0xFFFF - old_timer_cnt;
	return ((tmp + cur_timer_cnt) / SYS_TICK_1MS_DIV);
}

U16 time_10msec_interval_get(U16 old_timer_cnt){
	U16 cur_timer_cnt;
	
	cur_timer_cnt = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
	if(cur_timer_cnt >= old_timer_cnt){
		return ((cur_timer_cnt - old_timer_cnt) / SYS_TICK_10MS_DIV);
	}
	
	U16 tmp = 0xFFFF - old_timer_cnt;
	return ((tmp + cur_timer_cnt) / SYS_TICK_10MS_DIV);
	
}

U16 time_100msec_interval_get(U16 old_timer_cnt)
{
	U16 cur_timer_cnt;

	cur_timer_cnt = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
	if(cur_timer_cnt >= old_timer_cnt){
		return ((cur_timer_cnt - old_timer_cnt) / SYS_TICK_100MS_DIV);
	}

	U16 tmp = 0xFFFF - old_timer_cnt;
	return ((tmp + cur_timer_cnt) / SYS_TICK_100MS_DIV);
	
}

//void progress_10ms_condition()
//{
	//get_bar_run_info();
	//PRINTLINE;
//}
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// target time condition
// 10mSec, 100mSec, 1Sec
//////////////////////////////////////////////////////////////////////////////////////////////////
void process_target_time_handler(void){
	// if(time_1msec_interval_get(pDC->target_time.tmout_10msec) > 10){
		// pDC->target_time.tmout_10msec= *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		// progress_10ms_condition();
	// }
	
	if(time_10msec_interval_get(pDC->target_time.tmout_10msec) > 10){
		pDC->target_time.tmout_10msec = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		//progress_10ms_condition();
		if(esp32_get_infomation.get_info_time)
			esp32_get_infomation.get_info_time--;

		// 로딩 화면 카운트다운 + 스피너 회전
		// 다음 중 하나면 스피너 표시:
		//   1) show_loading_screen (초기 3초 로딩)
		//   2) ESP32 호밍 중 (state=3)
		//   3) CMD2_PWR_ON 송신 후 state=3 도착 대기 중 (awaiting_homing_start)
		{
			bool homing_in_progress = (power &&
				bed_status.current_mode == 0 && bed_status.run_state == 3);
			// 스피너 회전은 화면 표시 조건(is_homing_active)과 일치시킨다.
			// (마스터 부팅 대기 !master_booted 포함 — 그래야 대기 중에도 스피너가 돈다)
			bool spinner_shown = is_homing_active();
			if(show_loading_screen && loading_remain > 0){
				loading_remain--;
				if(loading_remain == 0){
					show_loading_screen = false;
				}
			}
			// awaiting_homing_start 타임아웃 (state=3가 안 와도 5초 후 해제)
			if(awaiting_homing_start){
				if(homing_in_progress){
					// state=3 확인됨 — 이후엔 homing_in_progress 조건으로 유지
					awaiting_homing_start = false;
				} else if(awaiting_homing_timeout > 0){
					awaiting_homing_timeout--;
					if(awaiting_homing_timeout == 0){
						awaiting_homing_start = false;
					}
				}
			}
			if(spinner_shown){
				loading_anim_phase = (loading_anim_phase + 1) & 7;
				smart_bed_display.display_refresh = true;
			}
		}

		// --- 부팅 시 GetBedStatus 요청 (spec §3) ---
		// 200ms(2틱) 대기 × 최대 3회. 실패해도 브로드캐스트로 진입하므로 무해.
		// CMD1은 반드시 CMD1_SEND_RUN_ST(0x10). 0x01이면 Master가 조용히 무시한다.
		if(!boot_status_done){
			if(boot_status_wait > 0){
				boot_status_wait--;
			} else if(boot_status_retry > 0){
				U8 tmp = 0;
				boot_status_retry--;
				debugprintf("\n\r [BOOT] TX GetBedStatus (0x10,0xF1) retry_left=%d", boot_status_retry);
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_GET_BED_STATUS, &tmp, 0);
				boot_status_wait = 2;	// 200ms
			} else {
				debugprintf("\n\r [BOOT] GetBedStatus no reply -> fallback to broadcast");
				boot_status_done = true;
			}
		}

		// --- 부팅 확인창 long-press (spec §6) ---
		// 100ms 틱 기준으로 재야 화면 재그리기 부하와 무관하게 정확히 3초가 된다.
		// 진행 바 칸이 바뀔 때만 화면을 갱신한다. 매 틱 갱신하면 루프가 느려진다.
		if(startup_confirm_active){
			if(conform_key_held){
				if(conform_hold_cnt < CONFORM_HOLD_TICKS){
					U32 seg_before = (conform_hold_cnt * CONFORM_BAR_SEGMENTS) / CONFORM_HOLD_TICKS;
					conform_hold_cnt++;
					if(((conform_hold_cnt * CONFORM_BAR_SEGMENTS) / CONFORM_HOLD_TICKS) != seg_before)
						smart_bed_display.display_refresh = true;
					if(conform_hold_cnt >= CONFORM_HOLD_TICKS){
						debugprintf("\n\r CONFIRM : 3s hold complete");
						startup_confirm_accept();
					}
				}
			} else if(conform_hold_cnt != 0){
				debugprintf("\n\r CONFIRM : released early -> progress reset");
				conform_hold_cnt = 0;
				smart_bed_display.display_refresh = true;
			}
		}

		// --- 통신 끊김 감지 (spec §7) --- [비활성화]
		// ESP32 BedStatus keepalive 주기(10초)와 리모컨 timeout(4.5초) 불일치로
		// 항상 lost 판정되는 문제 → 감지 로직 자체를 끔.
		// 필요 시 나중에 keepalive/timeout 재조정 후 부활.
		link_lost = false;
		bed_status_silence = 0;

		// stdby 타임아웃 카운트다운 (100ms 틱 기준 → 정확한 초 단위)
		if(stdby_in_progress && stdby_timeout > 0){
			stdby_timeout--;
		}

		// 로컬 상태 잠금 카운트다운 (100ms 틱)
		if(bed_state_lock_remain > 0){
			bed_state_lock_remain--;
		}

		// 볼륨 잠금 카운트다운 (100ms 틱)
		if(vol_lock_remain > 0){
			vol_lock_remain--;
		}

		// 외부 종료중 — 최소 표시 시간 decay
		if(external_stopping && external_stopping_min_remain > 0){
			external_stopping_min_remain--;
			if(external_stopping_min_remain == 0 &&
			   bed_status.current_mode == 0 && bed_status.run_state == 0){
				// 최소 시간 경과 + 안정 상태 → clear
				external_stopping = false;
				smart_bed_display.display_refresh = true;
			}
		}
		// 외부 종료중 안전 timeout
		if(external_stopping && external_stopping_timeout > 0){
			external_stopping_timeout--;
			if(external_stopping_timeout == 0){
				external_stopping = false;
				smart_bed_display.display_refresh = true;
			}
		}

		// pending_mode_start 카운트다운 — timeout(15초)까지 유지
		// 자동 settle 감지(mode!=0 && state==1)는 제거 — 모드 시작 후 ESP32가
		// 호밍(state=3)을 거칠 수 있으므로 state=1로 일찍 settle 처리하면
		// 이후 state=3 transition을 "종료중"으로 오감지함
		if(pending_mode_start && pending_mode_start_timeout > 0){
			pending_mode_start_timeout--;
			if(pending_mode_start_timeout == 0){
				pending_mode_start = false;
			}
		}

		// init_home_pending 안전 timeout (60초). 정상 흐름은 BED_STATUS(state=1) 수신 시 클리어.
		if(init_home_pending && init_home_pending_timeout > 0){
			init_home_pending_timeout--;
			if(init_home_pending_timeout == 0){
				init_home_pending = false;
				smart_bed_display.display_refresh = true;
			}
		}

		// pending_pwr_on_check: REMO_PWR_ON에서 즉시 결정하도록 변경됨 (fallback 불필요)

		// VAIRANCE/LEVITATE 모드 경과 시간 타이머
		//  - state=1 (동작 중): 100ms씩 누적. 새 시작이면 0으로 리셋.
		//  - state=2 (일시정지): 값 유지
		//  - state=0/3 (정지/초기화), stdby_in_progress, 비-VAIRANCE/LEVITATE 모드:
		//    → 즉시 00:00으로 리셋
		{
			static U8 prev_mode_for_etimer = 0;
			static U8 prev_state_for_etimer = 0xFF;
			U8 m = bed_status.current_mode;
			U8 s = bed_status.run_state;
			bool is_vair_or_levit = (m == 0x20 || m == 0x10 || m == 0x11 || m == 0x12);
			// init_home_pending 활성 중엔 카운트 정지 (ESP32가 실제 홈 시퀀스 진행 중)
			if(is_vair_or_levit && s == 1 && !stdby_in_progress && !init_home_pending) {
				U32 prev_sec, new_sec;
				// fresh_start: state 0/3(정지/초기화)에서 1로 전환 or 모드 변경 시에만 true.
				// state 2(일시정지)에서 1로 전환(resume)은 fresh_start 아님 → 이어서 카운트.
				bool fresh_start = (prev_state_for_etimer != 1 && prev_state_for_etimer != 2) ||
					(prev_mode_for_etimer != m);
				if(fresh_start || mode_timer_mode != m) {
					mode_timer_mode = m;
					mode_timer_elapsed_ms = 0;
				}
				prev_sec = mode_timer_elapsed_ms / 1000;
				mode_timer_elapsed_ms += 100;
				new_sec = mode_timer_elapsed_ms / 1000;
				if(new_sec != prev_sec) {
					smart_bed_display.display_refresh = true;
				}
			} else if(is_vair_or_levit && s == 2) {
				// 일시정지 — 값 유지 (재시작 시 이어서 카운트)
			} else {
				// 종료(state=0) / 초기화(state=3) / stdby / 다른 모드 → 00:00 리셋
				if(mode_timer_mode != 0 || mode_timer_elapsed_ms != 0) {
					mode_timer_mode = 0;
					mode_timer_elapsed_ms = 0;
					smart_bed_display.display_refresh = true;
				}
			}
			prev_state_for_etimer = s;
			prev_mode_for_etimer = m;
		}

		// 체압 매트리스는 ESP32 sensor_user_task가 항상 송신.
		// 모드/종료/초기화 상태와 무관하게 리모컨은 받은 데이터를 그대로 표시.

		// 마사지 화면 슬라이드 — 휠 회전 느낌
		// 90px → 0, easing처럼 처음엔 빠르고 끝에 느려지는 효과
		// (값이 클수록 큰 step, 작아질수록 작은 step)
		{
			extern int massage_anim_offset;
			int abs_v = massage_anim_offset > 0 ? massage_anim_offset : -massage_anim_offset;
			if(abs_v > 0) {
				int step = abs_v / 3 + 2;	// easing: 30→12→6→4→2→0 같은 감속
				if(massage_anim_offset > 0){
					massage_anim_offset -= step;
					if(massage_anim_offset < 0) massage_anim_offset = 0;
				} else {
					massage_anim_offset += step;
					if(massage_anim_offset > 0) massage_anim_offset = 0;
				}
				smart_bed_display.display_refresh = true;
			}
		}

		// 5초 무입력 자동 홈 복귀 — 동작 중 모드가 있을 때 비-홈 화면이면 카운트다운
		// 종료 / 낙상경고 화면은 제외 (사용자 응답 필요)
		// 돌봄(환자케어) 화면은 제외 — 조그로 수동 조절하는 동안 미조작이 정상이므로
		//   자동 홈 복귀하면 안 됨. 종료는 사용자가 명시적으로 홈 버튼을 눌러야 함.
		if(power && !show_loading_screen &&
		   smart_bed_status.status != MODE_HOME &&
		   smart_bed_status.status != MODE_SHUTDOWN &&
		   smart_bed_status.status != MODE_FALL_ALERT &&
		   smart_bed_status.status != MODE_PATIENT_CARE &&
		   (bed_status.run_state == 1 || bed_status.run_state == 2)) {
			if(auto_home_timer > 0) {
				auto_home_timer--;
				if(auto_home_timer == 0) {
					debugprintf("\n\r AUTO HOME: 5sec idle -> MODE_HOME");
					smart_bed_status.status = MODE_HOME;
					smart_bed_display.display_refresh = true;
				}
			}
		}

		// 모드명/상태 마퀴 스크롤 — status 박스를 가진 화면에서 텍스트가 박스를 초과할 때 스크롤
		// 동작중 / 일시정지 / 종료중 / 초기화중(stdby) 모두 처리
		// HOME 외에 VAIRANCE/LEVITATE/MASSAGE/CARE 화면도 동일 박스를 가지므로 스크롤 유지
		if(power && !show_loading_screen &&
		   (smart_bed_status.status == MODE_HOME ||
		    smart_bed_status.status == MODE_VAIRANCE ||
		    smart_bed_status.status == MODE_LEVITATE ||
		    smart_bed_status.status == MODE_MASSAGE ||
		    smart_bed_status.status == MODE_PATIENT_CARE)) {
			// 모드/상태/stdby 변경 감지 → 폭 재계산 + 스크롤 리셋
			if(mode_text_last_mode != bed_status.current_mode ||
			   mode_text_last_state != bed_status.run_state ||
			   mode_text_last_stdby != stdby_in_progress ||
			   mode_text_last_external_stopping != external_stopping) {
				debugprintf("\n\r [DISP] mode=0x%02x state=%d stdby=%d ext_stop=%d -> \"%s\"",
					bed_status.current_mode, bed_status.run_state,
					stdby_in_progress, external_stopping,
					get_mode_status_text(bed_status.current_mode, bed_status.run_state));
				mode_text_last_mode = bed_status.current_mode;
				mode_text_last_state = bed_status.run_state;
				mode_text_last_stdby = stdby_in_progress;
				mode_text_last_external_stopping = external_stopping;
				mode_text_width = estimate_text_width_28(
					get_mode_status_text(bed_status.current_mode, bed_status.run_state));
				mode_text_offset = 0;
				mode_text_pause = 0;
				// 텍스트 영역(x=32~140, 폭 108)보다 길면 스크롤
				mode_text_scrolling = (mode_text_width > 108);
				smart_bed_display.display_refresh = true;	// 텍스트 변경 → 재그리기
			}
			// 스크롤 진행
			if(mode_text_scrolling) {
				U32 target = (mode_text_width > 108) ? (mode_text_width - 108 + 4) : 0;
				if(mode_text_pause > 0) {
					mode_text_pause--;
					if(mode_text_pause == 0) {
						mode_text_offset = 0;	// 처음으로 리셋
					}
				} else {
					mode_text_offset += 2;	// 2px/tick = ~20px/sec
					if(mode_text_offset >= target) {
						mode_text_offset = target;
						mode_text_pause = 10;	// 1초 (10 × 100ms)
					}
				}
				smart_bed_display.display_refresh = true;	// 매 틱 재그리기
			}
		} else {
			// 비-홈이면 스크롤 상태 리셋
			if(mode_text_last_mode != 0xFF) {
				mode_text_last_mode = 0xFF;
				mode_text_last_state = 0xFF;
				mode_text_last_stdby = false;
				mode_text_offset = 0;
				mode_text_scrolling = false;
			}
		}
	}
	// if(time_100msec_interval_get(pDC->target_time.tmout_100msec) > 1){
		// pDC->target_time.tmout_100msec = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		
		// PRINTLINE;
	// }
}

// void progress_10ms_condition(void)
// {
	// PRINTLINE;
	// // if(key_control->read_time > 0) key_control->read_time--;// key read
		
	
	// // if(slave_control->read_tmout>0) slave_control->read_tmout--;					// SERIAL COMMUNICATION
	// // if(WHM_modbus->read_tmout>0)	WHM_modbus->read_tmout--;					// WATT METER COMMUNICATION

	// // if(master_modbus_rtu->read_tmout>0)	master_modbus_rtu->read_tmout--;
	// // if(user_info->t_key_tmout>0) user_info->t_key_tmout--;

// }


SMART_BED_DISP_STATUS smart_bed_display;
SMART_BED_STATUS smart_bed_status;
SURFACE *home_img;     // boot.suf (부팅/초기화 화면)
SURFACE *main_img;     // main.suf (메인 상태 화면 배경)
SURFACE *btm1_img;
SURFACE *btm2_img;
SURFACE *vaira_title_imag;
SURFACE *vaira_main_icon_imag;
SURFACE *vaira_title_imag;
SURFACE *bar_img;
SURFACE *body_imag;
SURFACE *radio_avail_img;
SURFACE *radio_avail_selt_img;
SURFACE *radio_dis_img;
SURFACE *radio_dis_selt_img;
SURFACE *dip_en_num_img;
SURFACE *dip_ds_num_img;

SURFACE *set_title_img;
SURFACE *set_top_manual_sel_img;
SURFACE *set_top_manual_img;
SURFACE *set_top_selftest_sel_img;
SURFACE *set_top_selftest_img;
SURFACE *set_icon_img;

// 교대부양
SURFACE *levitate_title_img;
SURFACE *levitate_main_icon_img;
SURFACE *levitate_normal_icon_img;
SURFACE *levitate_concent_icon_img;
SURFACE *levitate_sleep_icon_img;
SURFACE *levitate_normal_sel_icon_img;
SURFACE *levitate_concent_sel_icon_img;
SURFACE *levitate_sleep_sel_icon_img;

// MASSAGE
SURFACE *massage_title_img;
SURFACE *massage_main_icon_img;
SURFACE *massage_sub_icon_img[12];

// 환자 케어
SURFACE *patient_title_img;
SURFACE *patient_main_icon_img;
SURFACE *patient_head_img;
SURFACE *patient_head_sel_img;
SURFACE *patient_defec_img;
SURFACE *patient_defec_sel_img;
SURFACE *patient_shift_img;
SURFACE *patient_shift_sel_img;

SURFACE *patient_head_main_img;	// 머리감기 이미지
SURFACE *patient_shift_main_img;	// 이동 이미지

SURFACE *heat_titile_img;
SURFACE *heat_mian_icon_img;
SURFACE *heat_on_img;
SURFACE *heat_off_img;

SURFACE *left_arrow_img;
SURFACE *right_arrow_img;
SURFACE *left_run_arrow_img;
SURFACE *right_run_arrow_img;

SURFACE *left_select_img;
SURFACE *right_select_img;

// 자세제어
SURFACE *posture_bg_img;
SURFACE *posture_back_plate_img;
SURFACE *posture_back_plate_a_img;
SURFACE *posture_back_plate_icon_img;
SURFACE *posture_leg_plate_img;
SURFACE *posture_leg_plate_a_img;
SURFACE *posture_leg_plate_icon_img;
SURFACE *posture_all_plate_img;
SURFACE *posture_all_plate_a_img;
SURFACE *posture_all_plate_icon_img;
SURFACE *posture_height_img;
SURFACE *posture_height_a_img;

const char *massage_file_list[12] = {
	"image/top_nav_select.suf",	// 리듬타입
	"image/top_nav_select2.suf", // 물결
	"image/top_nav_select3.suf", // 맥파
 	
	"image/top_nav_select4.suf",
	"image/top_nav_select5.suf",
	"image/top_nav_select6.suf",
	"image/top_nav_select7.suf",
	"image/top_nav_select8.suf",
	"image/top_nav_select9.suf",
	"image/top_nav_select10.suf",
	"image/top_nav_select11.suf",
	"image/top_nav_select12.suf"
};
void image_load(void){
	int i;
	
	home_img = loadsurf("image/boot.suf");
	main_img = loadsurf("image/main.suf");
	btm1_img = loadsurf("image/bottom1.suf");
	btm2_img = loadsurf("image/bottom2.suf");
	vaira_title_imag = loadsurf("image/rc_title_bar_vairance.suf");
	vaira_main_icon_imag = loadsurf("image/main_icon_vairance.suf");
	bar_img = loadsurf("image/bar_active.suf");
	body_imag = loadsurf("image/human.suf");
	
	radio_avail_img = loadsurf("image/radio_available_selected_none.suf");
	radio_avail_selt_img = loadsurf("image/radio_available_selected_focused.suf");
	radio_dis_img = loadsurf("image/radio_disabled_selected_none.suf");
	radio_dis_selt_img = loadsurf("image/radio_disabled_selected_focused.suf");
	dip_en_num_img = loadsurf("image/disp_cnt_bg_available.suf");
	dip_ds_num_img = loadsurf("image/disp_cnt_bg_focused.suf");
	
	set_title_img = loadsurf("image/rc_title_bar_set.suf");
	set_icon_img = loadsurf("image/main_icon_set.suf");
	set_top_manual_sel_img = loadsurf("image/top_manual_btn_focused.suf");
	set_top_manual_img = loadsurf("image/top_manual_btn_default.suf");
	set_top_selftest_sel_img = loadsurf("image/top_self_test_btn_focused.suf");
	set_top_selftest_img = loadsurf("image/top_selft_test_btn_default.suf");
	
	// 교대부양
	levitate_title_img = loadsurf("image/rc_title_bar_levitate.suf");// title
	levitate_main_icon_img = loadsurf("image/main_icon_levitate.suf");// icon 
	
	levitate_normal_icon_img = loadsurf("image/top_normal_btn_default.suf");// 일반
	levitate_concent_icon_img = loadsurf("image/top_concent_btn_default.suf");// 집중
	levitate_sleep_icon_img = loadsurf("image/top_sleep_btn_default.suf");// 수면
	
	levitate_normal_sel_icon_img = loadsurf("image/top_normal_btn_focused.suf");// 일반
	levitate_concent_sel_icon_img = loadsurf("image/top_concent_btn_focused.suf");// 집중
	levitate_sleep_sel_icon_img = loadsurf("image/top_sleep_btn_focused.suf");// 수면
	
	// MASSAGE
	massage_title_img = loadsurf("image/rc_title_bar_massage.suf");// 마사지
	massage_main_icon_img = loadsurf("image/main_icon_massage.suf");// massage icon
	//massage_sub_icon_img[0] = loadsurf("image/top_nav_select.suf");
	for(i = 0; i<12; i++)
		massage_sub_icon_img[i] = loadsurf((char*)massage_file_list[i]);// 리듬타입
	
	// 환자케어
	patient_title_img = loadsurf("image/rc_title_bar_care.suf");
	patient_main_icon_img = loadsurf("image/main_icon_care.suf");
	patient_head_img = loadsurf("image/top_head_wrap_btn_default.suf");
	patient_head_sel_img = loadsurf("image/top_head_wrap_btn_focused.suf");
	patient_defec_img = loadsurf("image/top_meal_btn_default.suf");
	patient_defec_sel_img = loadsurf("image/top_meal_btn_focused.suf");
	patient_shift_img = loadsurf("image/top_shift_btn_default.suf");
	patient_shift_sel_img = loadsurf("image/top_shift_btn_focused.suf");
	
	patient_head_main_img = loadsurf("image/head_wrap.suf");	// 머리감기 이미지
	patient_shift_main_img = loadsurf("image/shift.suf");	// 이동 이미지
	
	heat_titile_img = loadsurf("image/rc_title_bar_heat.suf");	
	heat_mian_icon_img = loadsurf("image/main_icon_heat.suf");	
	heat_on_img = loadsurf("image/hit_on.suf");	
	heat_off_img = loadsurf("image/hit_off.suf");

	left_arrow_img = loadsurf("image/btn_left.suf");
	right_arrow_img = loadsurf("image/btn_right.suf");
	left_run_arrow_img = loadsurf("image/btn_left_run.suf");
	right_run_arrow_img = loadsurf("image/btn_right_run.suf");

	left_select_img = loadsurf("image/btn_left_select.suf");
	right_select_img = loadsurf("image/btn_right_select.suf");

	// 자세제어
	posture_bg_img = loadsurf("image/bg.suf");
	posture_back_plate_img = loadsurf("image/back_plate.suf");
	posture_back_plate_a_img = loadsurf("image/back_plate_a.suf");
	posture_back_plate_icon_img = loadsurf("image/back_plate_icon.suf");
	posture_leg_plate_img = loadsurf("image/leg_plate.suf");
	posture_leg_plate_a_img = loadsurf("image/leg_plate_a.suf");
	posture_leg_plate_icon_img = loadsurf("image/leg_plate_icon.suf");
	posture_all_plate_img = loadsurf("image/all_plate.suf");
	posture_all_plate_a_img = loadsurf("image/all_plate_a.suf");
	posture_all_plate_icon_img = loadsurf("image/all_plate_icon.suf");
	posture_height_img = loadsurf("image/height.suf");
	posture_height_a_img = loadsurf("image/height_a.suf");
}

// USE_PRETENDARD_FONT는 user_task.h에서 정의 (draw_text_kr에서도 참조)
void load_font(void){
#if USE_PRETENDARD_FONT
	// Pretendard .fnt는 EGLDesigner 대신 gen_pretendard_fnt.py로 생성.
	// 한 파일에 한글/영문 모두 포함되므로 g_pFont/g_pFontKor 양쪽에서 같은 파일 참조.
	// UTF-8 encoding 설정 → draw_text_kr에서 UTF-8 그대로 넘길 수 있음.
	g_pFont28 = create_bmpfont("image/font/pretendard28.fnt");
	bmpfont_setautokerning(g_pFont28, true);
	bmpfont_settextencoding(g_pFont28, UTF8);

	g_pFont16 = create_bmpfont("image/font/pretendard16.fnt");
	bmpfont_setautokerning(g_pFont16, true);
	bmpfont_settextencoding(g_pFont16, UTF8);

	// 40px는 홈 화면 큰 숫자용 (ASCII만). Pretendard 40px 아틀라스는 1MB로 부담 →
	// 기존 ASCII-only font40.fnt 그대로 사용.
	g_pFont40 = create_bmpfont("image/font/font40.fnt");
	bmpfont_setautokerning(g_pFont40, true);

	// 한글 포인터: 같은 .fnt를 재사용 (중복 로드 방지 위해 포인터 공유)
	g_pFontKor   = g_pFont28;
	g_pFontKor16 = g_pFont16;
#else
	// [Fallback] 기존 폰트 (Pretendard .fnt 미준비 시)
	g_pFont28 = create_bmpfont("image/font/font28.fnt");
	bmpfont_setautokerning(g_pFont28, true);

	g_pFont16 = create_bmpfont("image/font/font16.fnt");
	bmpfont_setautokerning(g_pFont16, true);

	g_pFont40 = create_bmpfont("image/font/font40.fnt");
	bmpfont_setautokerning(g_pFont40, true);

	// 한글 폰트 (SDK 내장 bitfont)
	g_pFontKor = create_bitfont();
	set_bitfontsize(g_pFontKor, 14, 28, 28, 28);	// 영문14x28, 한글28x28

	g_pFontKor16 = create_bitfont();
	set_bitfontsize(g_pFontKor16, 8, 16, 16, 16);	// 영문8x16, 한글16x16
#endif
}







// ========================= HOME Start ============================ //
//////////////////////////////////////////////////////////////////////////////////////////////////
// 
// HOME Mode
// 
//////////////////////////////////////////////////////////////////////////////////////////////////
void home_proc(void)
{
	if(smart_bed_display.status != MODE_HOME)
	{
		smart_bed_display.status = MODE_HOME;	// 교대부양 display 갱신
		smart_bed_display.display_refresh = true;// 교대부양 display
		remocon_key.key_val = 0xFF;
		return;
	}

	switch(remocon_key.key_val){
		case CONFORM_KEY:
			// 오버레이 화면에서 확인 버튼 → 동작중↔일시정지 토글
			if(bed_status.run_state == 1) {
				// 동작 중 → 일시정지
				U8 tmp = CMD3_PAUSE;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PAUSE, &tmp, 1);
				bed_status.run_state = 2;	// 즉시 상태 반영
				bed_state_lock_remain = 100;	// 3초 잠금
				smart_bed_display.display_refresh = true;
				conform_key_run = CMD3_RESTART;
				debugprintf("\n\r HOME: CONFORM -> PAUSE");
			} else if(bed_status.run_state == 2) {
				// 일시정지 → 재시작
				U8 tmp = CMD3_RESTART;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_RESTART, &tmp, 1);
				bed_status.run_state = 1;	// 즉시 상태 반영
				bed_state_lock_remain = 100;	// 3초 잠금
				smart_bed_display.display_refresh = true;
				conform_key_run = CMD3_PAUSE;
				debugprintf("\n\r HOME: CONFORM -> RESUME");
			}
			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:// 설정/저장 키를 눌렀다.
			smart_bed_status.status = MODE_SET_SAVE;
			remocon_key.key_val = 0xFF;
			break;
	}
}

void home_draw(void){
	set_draw_target(getbackframe());
	if(!power) {
		// 전원 OFF
		draw_surface(home_img, 0, 0);
	} else {
		// 로딩/호밍 스피너 화면은 progress_lcd_display의 전역 오버레이(homing_draw)가 처리한다.
		// 여기(홈)에 도달하면 항상 대기/상태 화면이다.
		// PDF 슬라이드 4 — 단색 배경 + 4영역 오버레이
		draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(20, 25, 38));
		draw_status_overlay();
	}
	flip();
}
// ========================= HOME End ============================ //











// ========================= Power On/Off ==========================/
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Power ON/OFF
// 전원 Off 후 다시 전원 On이 되면 Home Mode
//
// POWER ON  : FF 81 '9''B''1''C' 10 80 00 xx xx
// POWER OFF : FF 81 '9''B''1''C' 10 81 00 xx xx
// CONFIRM   : FF 81 '9''B''1''C' 10 FF 00 xx xx
// STAND BY  : FF 81 '9''B''1''C' 10 F0 00 xx xx
//
// 전원 최초 입력시 키에서 off상태로 LCD는 안나옴
//
//////////////////////////////////////////////////////////////////////////////////////////////////

U8 remo_pwr_st = 0;	// 0(전원 최초 입력), 1(LCD ON), 2(LCD OFF) 0->1<->2

bool stdby_in_progress = false;	// 초기위치 복귀 중 (키 차단)
bool power_off_pending = false;	// ACK 후 전원 OFF 필요
bool stdby_complete = false;	// ESP32 ACK 수신 플래그
U32 stdby_timeout = 0;			// 타임아웃 카운터
// show_loading_screen / loading_remain 은 파일 상단(g_pFont 영역)에 정의됨

void remocon_power_ctrl(U8 remo_pwr)
{
	U8 tmp = 0;

	switch(remo_pwr)
	{
		case REMO_PWR_ON:	// 전원 ON (클릭)
			// CMD2_PWR_ON은 ESP32의 run_power(PowerOn)을 트리거 → 1.mp3 재생 + 홈 복귀 루틴.
			// 리모컨 OFF 상태에서도 main loop가 BED_STATUS를 계속 수신하여 bed_status는 최신 상태.
			// 침대가 idle(mode=0 && state=0)일 때만 CMD2_PWR_ON 송신.
			// 동작 중(mode≠0, state=1/2) 또는 초기화 중(state=3)이면 송신 안 함 → 동작 유지.
			pending_pwr_on_check = false;
			pending_pwr_on_check_timeout = 0;
			remo_pwr_st = REMO_LCD_ON;
			stdby_in_progress = false;
			power_off_pending = false;

			// startup_pending=1이면 Master가 InitializeAction 외 모든 명령을 거부한다.
			// CMD2_PWR_ON을 보내봐야 무시당하고 5초 스피너 후 무반응이 된다 (spec §5.1).
			// 대신 확인창을 띄우고, 3초 확인 후 CMD2_STDBY를 보낸다.
			if(bed_status.startup_pending){
				debugprintf("\n\r [PWR_ON] startup_pending=1 -> suppress CMD2_PWR_ON, show confirm");
				startup_confirm_active = true;
				conform_hold_cnt = 0;
				show_loading_screen = false;
				awaiting_homing_start = false;
				smart_bed_status.status = MODE_STARTUP_CONFIRM;
				smart_bed_display.status = MODE_STARTUP_CONFIRM;
				smart_bed_display.display_refresh = true;
				LCD_ON();
				break;
			}

			if(bed_status.current_mode == 0 && bed_status.run_state == 0){
				debugprintf("\n\r [PWR_ON] bed idle -> send CMD2_PWR_ON");
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_ON, &tmp, 0);
				awaiting_homing_start = true;
				awaiting_homing_timeout = 50;	// 5초 fallback
			} else {
				debugprintf("\n\r [PWR_ON] bed running (mode=0x%02x state=%d) -> skip PWR_ON",
					bed_status.current_mode, bed_status.run_state);
			}
			smart_bed_status.status = MODE_HOME;
			smart_bed_display.display_refresh = true;
			// PDF 슬라이드 3: 로딩 화면 (boot.suf) 표시 → 약 3초 후 자동 홈 전환
			show_loading_screen = true;
			loading_remain = 30;	// process_target_time_handler 틱 기준 (~3초)
			LCD_ON();
			break;

		case REMO_LCD_OFF:	// 전원 OFF (ACK 수신 후 또는 직접 호출)
			remo_pwr_st = REMO_LCD_OFF;
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_OFF, &tmp, 0);
			smart_bed_status.status = MODE_HOME;
			running_massage_type = -1;
			stdby_in_progress = false;
			show_shutdown_screen();	// "종료합니다." 표시 후 1.5초 대기
			LCD_OFF();
			break;

		case REMO_STDBY_OFF:	// 롱클릭: 초기위치 복귀 + 전원 OFF 대기
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_STDBY, &tmp, 0);
			smart_bed_status.status = MODE_SHUTDOWN;
			smart_bed_display.status = MODE_SHUTDOWN;
			smart_bed_display.display_refresh = true;
			stdby_in_progress = true;
			power_off_pending = true;
			stdby_complete = false;
			stdby_timeout = 6000;	// ~60초 타임아웃 (10ms × 6000)
			break;
	}
}

// 메인 전원 인가 시 리모컨 자동 ON — 전원키 없이 부팅과 함께 켜진다.
// 침대+스피너(초기화) 화면을 즉시 띄우고, 이후 마스터의 BedStatus 브로드캐스트에 따라
// protocol.c가 확인창(startup_pending=1) 또는 홈(idle)으로 전환한다.
// CMD2_PWR_ON은 보내지 않는다 — 마스터가 startup_pending 브로드캐스트로 부팅 흐름을 주도하고,
// 사용자 확인 후 CMD2_STDBY로 홈 시퀀스를 시작한다 (spec §5).
void remocon_boot_power_on(void)
{
	power = true;
	remo_pwr_st = REMO_LCD_ON;
	stdby_in_progress = false;
	power_off_pending = false;
	awaiting_homing_start = false;

	// 스피너는 마스터 부팅 완료(첫 BedStatus 수신)까지 유지된다 — is_homing_active()의 !master_booted 조건.
	// 부팅 완료 시 startup_pending=1이면 protocol.c가 확인창으로, idle이면 홈으로 전환한다.
	smart_bed_status.status = MODE_HOME;
	smart_bed_display.status = MODE_HOME;
	smart_bed_display.display_refresh = true;
	LCD_ON();
	debugprintf("\n\r [BOOT] auto power-on -> spinner (wait master boot)");
}

// 부팅 확인창에서 3초 확인이 끝났을 때 (spec §5.2)
// stdby_timeout과 stdby_in_progress를 반드시 함께 세팅한다.
// stdby_timeout 전역 초기값이 0이라 in_progress만 켜면 다음 틱에 즉시 타임아웃 처리된다.
void startup_confirm_accept(void)
{
	U8 tmp = 0;

	startup_confirm_active = false;
	conform_hold_cnt = 0;
	conform_key_held = false;

	stdby_timeout = STDBY_TIMEOUT_TICKS;
	stdby_in_progress = true;
	stdby_complete = false;
	stdby_initial_mode = 0;		// idle에서 시작 → "초기화 중" 표기
	power_off_pending = false;

	debugprintf("\n\r [CONFIRM] accepted -> TX CMD2_STDBY (0x10,0xF0)");
	esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_STDBY, &tmp, 0);

	smart_bed_status.status = MODE_HOME;
	smart_bed_display.status = MODE_HOME;
	smart_bed_display.display_refresh = true;
}

// 초기위치 복귀 진행 상태 확인 (메인 루프에서 매 사이클 호출)
void check_stdby_progress(void)
{
	if(!stdby_in_progress) return;

	if(stdby_complete){
		debugprintf("\n\r STDBY ACK RECEIVED");
		stdby_in_progress = false;
		stdby_complete = false;
		// 종료 완료 — 즉시 mode/state 클리어 + 잠금으로 ESP32 잔여 패킷 무시
		bed_status.current_mode = 0;
		bed_status.run_state = 0;
		bed_state_lock_remain = 100;	// 2초 잠금 (다음 ESP32 패킷이 정상 0이라 확신될 때까지)
		// 돌봄 틸팅 준비 상태도 리셋 (POWER_KEY로 종료 시 UI가 다시 단일 "틸팅" 박스로)
		running_flag = false;
		tilt_care_ready = false;
		smart_bed_display.display_refresh = true;	// "종료중/초기화중" → "대기중" 전환 표시

		if(power_off_pending){
			// 롱클릭 → 전원 OFF
			debugprintf("\n\r -> POWER OFF");
			remocon_power_ctrl(REMO_LCD_OFF);
			power = false;
			power_off_pending = false;
		}
		return;
	}

	// 타임아웃 (시간 기반 — 100ms 틱 핸들러에서 감소됨)
	if(stdby_timeout == 0){
		debugprintf("\n\r STDBY TIMEOUT");
		stdby_in_progress = false;
		bed_status.current_mode = 0;
		bed_status.run_state = 0;
		bed_state_lock_remain = 100;
		running_flag = false;
		tilt_care_ready = false;
		smart_bed_display.display_refresh = true;
		if(power_off_pending){
			remocon_power_ctrl(REMO_LCD_OFF);
			power = false;
			power_off_pending = false;
		}
	}
}

// bool remocon_power_proc(bool power_on){
	// U8 tmp = 0;
	
	// if(power_on == true){
		// LCD_ON();
		// esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_ON, &tmp, 0);
		// return true;// power on
	// }
	// LCD_OFF();
	// smart_bed_status.status = MODE_HOME;
	// esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PWR_OFF, &tmp, 0);
	// return false;// power off
// }

//



void process_analy_data(){
	check_stdby_progress();

	// 낙상 경고 상태: Pause(해제)와 PowerOff만 허용, 나머지 키 무시
	if(smart_bed_status.status == MODE_FALL_ALERT){
		if(remocon_key.key_val == CONFORM_KEY){
			U8 tmp = 0;
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_FALL_CLEAR, &tmp, 0);
			smart_bed_status.status = MODE_HOME;
			smart_bed_display.status = MODE_HOME;
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			debugprintf("\n\r FALL ALERT DISMISSED by user");
		}
		// PowerOff는 key.c에서 처리됨 (FALL_ALERT 상태에서도 허용)
		remocon_key.key_val = 0xFF;  // 나머지 키 무시
		return;
	}

	switch(smart_bed_status.status){
		case MODE_HOME:
			home_proc();
			break;
		case MODE_SET_SAVE:
			manual_selft_test_proc();
			break;
		case MODE_VAIRANCE:// 체압분산
			dispersion_proc();
			break;
		case MODE_LEVITATE:// 교대부양
			levitate_proc();
			break;
		case MODE_MASSAGE:// 마사지
			massage_proc();
			break;
		case MODE_PATIENT_CARE:// 환자 케어
			patient_care_proc();
			break;
		// 온열/음량은 key.c에서 직접 처리 (화면 전환 없음)
		// case MODE_SET_SAVE:
			// set_proc();
			// break;
		case MODE_INITIAL:
			initial_proc();
			break;
		case MODE_POSTURE:
			posture_proc();
			break;
		case MODE_ULCER_CARE:
			ulcer_care_proc();
			break;
	}
}

// 초기화(호밍) 화면 표시 조건 — 부팅 로딩 / 전원 ON 후 state=3 대기 / 호밍 진행 중.
// 이 동안엔 홈뿐 아니라 모든 화면 위에 스피너 화면을 띄우고 키 입력을 차단한다.
// 초기화가 끝나면(false) 정상적으로 다른 화면으로 전환된다.
bool is_homing_active(void)
{
	return power && (
		!master_booted ||					// 마스터 부팅 완료(첫 BedStatus) 전까지 스피너 유지
		show_loading_screen ||				// 수동 전원 ON 초기 로딩
		awaiting_homing_start ||			// 전원 ON 후 state=3 도착 대기
		(bed_status.current_mode == 0x00 && bed_status.run_state == 3));	// 홈 시퀀스 진행
}

void progress_lcd_display(void){
	// 통신 끊김 오버레이 (spec §7). 낙상 경고와 확인창이 우선.
	// 원래 모드를 잃지 않도록 smart_bed_display.status를 건드리지 않는다.
	static bool link_screen_shown = false;
	if(power && link_lost &&
	   smart_bed_status.status != MODE_FALL_ALERT && !startup_confirm_active){
		if(!link_screen_shown){
			link_check_draw();
			link_screen_shown = true;
		}
		smart_bed_display.display_refresh = false;
		return;
	}
	if(link_screen_shown){
		link_screen_shown = false;
		smart_bed_display.display_refresh = true;	// 원래 화면 복원
	}

	if(smart_bed_display.display_refresh == true){
		// 초기화(호밍) 중: 현재 어떤 모드 화면이든 침대+스피너 화면을 오버레이한다.
		// 낙상 경고·부팅 확인창은 안전상 우선하므로 제외한다.
		if(is_homing_active() &&
		   smart_bed_status.status != MODE_FALL_ALERT &&
		   smart_bed_status.status != MODE_STARTUP_CONFIRM){
			homing_draw();
			smart_bed_display.display_refresh = false;
			return;
		}
		switch(smart_bed_display.status){
			case MODE_HOME: // home
				home_draw();
				break;
			//case MODE_INITIAL:// 초기화
			//	break;
			case MODE_SET_SAVE:
				manual_selft_test_draw();
				break;
			case MODE_VAIRANCE:// 체압분산
				dispersion_draw();
				break;
			case MODE_LEVITATE:// 교대부양
				levitate_draw();
				break;
			case MODE_MASSAGE:
				massage_draw();
				break;
			case MODE_PATIENT_CARE:
				patient_care_draw();
				break;
			// 온열/음량은 화면 없음 (LED만 제어)
			case MODE_INITIAL:
				initial_draw();
				break;
			case MODE_POSTURE:
				posture_draw();
				break;
			case MODE_ULCER_CARE:
				ulcer_care_draw();
				break;
			case MODE_SHUTDOWN:
				shutdown_draw();
				break;
			case MODE_FALL_ALERT:
				fall_alert_draw();
				break;
			case MODE_STARTUP_CONFIRM:
				startup_confirm_draw();
				break;
		}

		smart_bed_display.display_refresh = false;
	}

}

// ========================= Screenshot ============================ //
//
// SET_KEY + POWER_KEY 동시 누름 시 현재 화면을 FAT(0:/screenshot/) 폴더에
// SHOT_NNN.BMP (24-bit BMP) 형식으로 저장.
//
#include "fatfs/ff.h"

static int screenshot_counter = 0;

// RGB565 → 24-bit BMP 저장 (front frame 캡쳐)
void save_screenshot(void){
	FIL fp;
	FRESULT res;
	UINT wb;
	SURFACE *surf;
	int w, h, row_size, pixel_data_size, file_size;
	int x, y;
	U8 hdr[14];
	U8 dib[40];
	static U8 row_buf[320 * 3];	// 320 px × 3 bytes = 960 bytes
	char path[40];

	surf = getfrontframe();
	if(!surf) return;
	w = 320; h = 480;	// 알려진 LCD 해상도
	row_size = w * 3;	// 24-bit, 320*3=960 (4-byte align 됨)
	pixel_data_size = row_size * h;
	file_size = 54 + pixel_data_size;

	// screenshot 폴더 생성 (이미 있으면 무시)
	f_mkdir("0:/screenshot");

	sprintf(path, "0:/screenshot/SHOT_%03d.BMP", screenshot_counter);
	screenshot_counter = (screenshot_counter + 1) % 1000;

	res = f_open(&fp, path, FA_WRITE | FA_CREATE_ALWAYS);
	if(res != FR_OK) {
		debugprintf("\n\r screenshot: f_open failed %d", res);
		return;
	}

	// BMP file header (14 bytes)
	hdr[0]='B'; hdr[1]='M';
	hdr[2] = (U8)(file_size & 0xFF);
	hdr[3] = (U8)((file_size >> 8) & 0xFF);
	hdr[4] = (U8)((file_size >> 16) & 0xFF);
	hdr[5] = (U8)((file_size >> 24) & 0xFF);
	hdr[6]=0; hdr[7]=0; hdr[8]=0; hdr[9]=0;	// reserved
	hdr[10]=54; hdr[11]=0; hdr[12]=0; hdr[13]=0;	// data offset
	f_write(&fp, hdr, 14, &wb);

	// DIB header (BITMAPINFOHEADER, 40 bytes)
	memset(dib, 0, 40);
	dib[0]=40;
	dib[4] = (U8)(w & 0xFF); dib[5] = (U8)((w >> 8) & 0xFF);
	dib[8] = (U8)(h & 0xFF); dib[9] = (U8)((h >> 8) & 0xFF);
	dib[12]=1;	// planes
	dib[14]=24;	// bpp
	dib[20] = (U8)(pixel_data_size & 0xFF);
	dib[21] = (U8)((pixel_data_size >> 8) & 0xFF);
	dib[22] = (U8)((pixel_data_size >> 16) & 0xFF);
	dib[23] = (U8)((pixel_data_size >> 24) & 0xFF);
	f_write(&fp, dib, 40, &wb);

	// Pixel data — BMP는 bottom-up 저장
	for(y = h - 1; y >= 0; y--) {
		for(x = 0; x < w; x++) {
			U16 *p16 = GETPOINT16(surf, x, y);
			U16 p = *p16;
			U8 r = (U8)(((p >> 11) & 0x1F) << 3);
			U8 g = (U8)(((p >> 5) & 0x3F) << 2);
			U8 b = (U8)((p & 0x1F) << 3);
			row_buf[x*3 + 0] = b;	// BMP는 BGR 순서
			row_buf[x*3 + 1] = g;
			row_buf[x*3 + 2] = r;
		}
		f_write(&fp, row_buf, row_size, &wb);
	}
	f_close(&fp);
	debugprintf("\n\r screenshot saved: %s", path);
}
