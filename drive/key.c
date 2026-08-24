#include "../app/main.h"
\
/*******************************************************************
	Smart bed remocon
	Key read
******************************************************************/
extern void spi_wait_finish (int ch);

#define KEY_CS_LOW0() 	*R_GPOLOW(4) = (1<<1)	// P4.1
#define KEY_CS_HIGH0() 	*R_GPOHIGH(4) = (1<<1)	// P4.1
#define LATCH_LOW0() 		*R_GPOLOW(4) = (1<<0)	// P4.0 
#define LATCH_HIGH0()		*R_GPOHIGH(4) = (1<<0)	// P4.0
#define KEY_CS_LOW1()		*R_GPOLOW(3) = (1<<7)	// P3.7
#define KEY_CS_HIGH1()	*R_GPOHIGH(3) = (1<<7)	// P3.7
#define LATCH_LOW1()		*R_GPOLOW(3) = (1<<6)	// P3.6
#define LATCH_HIGH1()		*R_GPOHIGH(3) = (1<<6) 	// P3.6

REMO_KEY	remocon_key;
CURSOR	cursor;
bool power = false;
static bool posture_up_active = false;
static bool posture_down_active = false;
// 돌봄케어 모드 조그 상태
static bool care_up_active = false;
static bool care_down_active = false;
static bool care_left_active = false;
static bool care_right_active = false;
static bool care_tilt_active = false;	// 틸팅 조그 활성 상태 (올림/내림 중이면 true)
static U8   care_tilt_dir = 0;			// 틸팅 조그 방향: 0=정지, 1=올림(UP), 2=내림(DOWN)

void key_init(void){
	
	KEY_CS_HIGH0();
	KEY_CS_HIGH1();
	
	memset(&remocon_key,0xFF, sizeof(REMO_KEY));
	remocon_key.count = KEY_CNT;
}

U16 get_spi_key(void){
	U16 key0 = 0;
	U16 key1 = 0;

	// latch
	LATCH_LOW0();
	delayus(200);
	LATCH_HIGH0();
	delayus(500);
	
	// cs
	KEY_CS_LOW0();
	delayus(500);
	*R_SPI0DATA(1) = 0xFF;
	spi_wait_finish(1);
	delayus(100);
	key0 = *R_SPI0DATA(1);
	KEY_CS_HIGH0();
	delayus(100);
	
	// latch
	LATCH_LOW1();
	delayus(200);
	LATCH_HIGH1();
	delayus(500);
	
	// cs
	KEY_CS_LOW1();
	delayus(500);
	*R_SPI0DATA(1) = 0xFF;
	spi_wait_finish(1);
	delayus(100);
	key1 = *R_SPI0DATA(1);
	KEY_CS_HIGH1();
	delayus(100);
	return ((key1<<8) | key0);
}


void key_proc(void){
	remocon_key.current = get_spi_key() | ~KEY_MASK;
	if(remocon_key.old != remocon_key.current){
		debugprintf("\n\r old[%x], cur[%x]", remocon_key.old, remocon_key.current);
		remocon_key.old = remocon_key.current;
		remocon_key.count = KEY_CNT;
		remocon_key.pushed = false;
		remocon_key.run = false;
		remocon_key.long_pushed = false;
		remocon_key.hold_count = 0;
		return;
	}
	else{
		if(remocon_key.count){
			remocon_key.count--;
			return;
		}
		else{
			if((remocon_key.current  & KEY_MASK) == KEY_MASK){// all key not press
				remocon_key.pushed = false;
				remocon_key.run = false;
				remocon_key.long_pushed = false;
				remocon_key.hold_count = 0;
				return;
			}
			if((remocon_key.pushed == false) && (remocon_key.run == false)){
				remocon_key.pushed = true;
				remocon_key.hold_count = 0;
				return;
			}
			// 키가 계속 눌려있으면 홀드 카운터 증가
			if(remocon_key.pushed == true){
				remocon_key.hold_count++;
				if(remocon_key.hold_count >= LONG_KEY_CNT){
					remocon_key.long_pushed = true;
				}
			}
		}
	}
}

// 전원키 릴리즈 감지용
static bool power_key_active = false;	// 전원키 추적 중
static bool power_key_long = false;		// 롱클릭 감지됨
static U32 power_release_cnt = 0;		// 릴리즈 디바운스 카운터
static U32 power_hold_cnt = 0;			// 독립 홀드 카운터 (key_proc 바운스 영향 없음)
static U32 power_active_frames = 0;	// power_key_active 유지 프레임 수

// 방식 A 워치독 — 조그 홀드 중 명령을 200ms마다 재전송(keepalive).
// 대상: 자세제어(등판/다리판/높이) + 돌봄케어(식사 UP/DOWN, 틸팅 올림/내림).
// process_target_time_handler()의 100ms 틱에서 매 틱 호출된다.
// 마스터는 해당 명령이 ~700ms 끊기면 자동 STOP(마스터 firmware 담당).
// 첫 명령은 key_read() 누름 에지에서 전송, 여기서는 홀드 유지 동안 재전송만.
// 기존 마스터에서도 명령이 멱등이라 동작 변화 없음 → 하위호환.
void jog_keepalive(void)
{
	static U8 ka_tick = 2;	// 100ms 틱 카운트다운 (2 = 200ms)
	U8 tmp = 0;
	bool active = false;

	if(power){
		if(smart_bed_status.status == MODE_POSTURE &&
		   (posture_up_active || posture_down_active))
			active = true;
		else if(smart_bed_status.status == MODE_PATIENT_CARE &&
		   (care_up_active || care_down_active || care_tilt_active))
			active = true;
	}

	if(!active){
		ka_tick = 2;	// 비활성: 다음 홀드에서 200ms 뒤 첫 재전송
		return;
	}

	if(ka_tick) ka_tick--;
	if(ka_tick) return;	// 아직 200ms 미도달
	ka_tick = 2;		// 200ms 주기 도달 → 재전송

	// 자세제어 조그
	if(smart_bed_status.status == MODE_POSTURE){
		if(posture_up_active){
			if(cursor.type == POSTURE_BACK || cursor.type == POSTURE_ALL)
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_UP, &tmp, 0);
			if(cursor.type == POSTURE_LEG || cursor.type == POSTURE_ALL)
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_LEG_UP, &tmp, 0);
			if(cursor.type == POSTURE_HEIGHT)
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEIGHT_UP, &tmp, 0);
		}
		if(posture_down_active){
			if(cursor.type == POSTURE_BACK || cursor.type == POSTURE_ALL)
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_DOWN, &tmp, 0);
			if(cursor.type == POSTURE_LEG || cursor.type == POSTURE_ALL)
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_LEG_DOWN, &tmp, 0);
			if(cursor.type == POSTURE_HEIGHT)
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEIGHT_DOWN, &tmp, 0);
		}
	}
	// 돌봄케어 조그 (식사모드 UP·DOWN 등판, 틸팅 올림·내림)
	else if(smart_bed_status.status == MODE_PATIENT_CARE){
		if(care_up_active)   esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_UP, &tmp, 0);
		if(care_down_active) esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_DOWN, &tmp, 0);
		// 틸팅: key_read()와 동일한 방향 해석 (tilt_sel 0=좌 / 1=우, dir 1=올림 / 2=내림)
		if(care_tilt_active){
			U8 cmd;
			if(care_tilt_dir == 1)      cmd = (tilt_sel == 0) ? CMD2_MOVE_LEFT      : CMD2_MOVE_RIGHT;
			else if(care_tilt_dir == 2) cmd = (tilt_sel == 0) ? CMD2_MOVE_LEFT_DOWN : CMD2_MOVE_RIGHT_DOWN;
			else                        cmd = 0;
			if(cmd) esp32_packet_send(CMD1_SEND_RUN_ST, cmd, &tmp, 0);
		}
	}
}

void key_read(void){
	key_proc();

	// 확인창 홀드 상태는 매 사이클 새로 판정한다.
	// 아래 전원키/스크린샷 처리가 중간에 return하므로, 여기서 먼저 내려두지 않으면
	// 직전 값이 남아 100ms 틱 쪽에서 계속 카운트가 올라간다.
	conform_key_held = false;

	// 5초 무입력 자동 홈 복귀 타이머 — 어떤 키든 눌리면 리셋
	if(remocon_key.current != 0xFFFF) {
		auto_home_timer = 50;	// 50 × 100ms = 5초
	}

	bool power_pressed = !(remocon_key.current & POWER_KEY);
	bool set_pressed   = !(remocon_key.current & SET_KEY);

	// ---- 스크린샷 트리거: SET_KEY + POWER_KEY 동시 누름 ----
	// 노이즈/잡음 false-trigger 방지: 3 사이클 연속으로 combo 확인 후에만 실행
	{
		static bool sshot_combo_active = false;
		static U8 sshot_confirm = 0;
		if(power_pressed && set_pressed) {
			if(sshot_confirm < 3) {
				sshot_confirm++;
			} else if(!sshot_combo_active) {
				sshot_combo_active = true;
				debugprintf("\n\r KEY : SET+POWER -> SCREENSHOT");
				save_screenshot();
				power_key_active = false;
				power_hold_cnt = 0;
				power_release_cnt = 0;
				remocon_key.run = true;
				remocon_key.key_val = 0xFF;
			}
			if(sshot_combo_active) return;
		} else {
			sshot_confirm = 0;
			if(sshot_combo_active) {
				sshot_combo_active = false;
				power_key_active = false;
				remocon_key.run = true;
				remocon_key.key_val = 0xFF;
				return;
			}
		}
	}

	// ---- 전원 ON 상태: 숏클릭=릴리즈 판별, 롱클릭=즉시 실행 ----
	if(power == true){
		// 전원키 누름 감지 → 추적 시작 (PRESS 시점엔 stdby_in_progress를 켜지 않음)
		if(!power_key_active && power_pressed && remocon_key.pushed && !remocon_key.run){
			power_key_active = true;
			power_key_long = false;
			power_release_cnt = 0;
			power_hold_cnt = 0;
			power_active_frames = 0;
			remocon_key.run = true;	// 다른 키 처리 방지
			debugprintf("\n\r KEY : POWER PRESS (tracking)");
		}

		// 독립 홀드 카운터: SPI 원시값 기반, key_proc 리셋 영향 없음
		if(power_key_active){
			power_active_frames++;
			if(power_pressed){
				power_hold_cnt++;
				// 롱클릭 판정 → 즉시 종료 루틴 시작
				if(power_hold_cnt >= LONG_KEY_CNT){
					debugprintf("\n\r KEY : POWER LONG -> POWER OFF (immediate)");
					power_key_active = false;
					power_key_long = true;
					power_release_cnt = 0;
					power_hold_cnt = 0;
					// 3초 롱클릭 → ACK 대기 없이 즉시 전원 OFF
					// stdby_in_progress 잔여 플래그 클리어 (혹시 다른 경로로 켜진 경우)
					stdby_in_progress = false;
					power_off_pending = false;
					// 확인창 중 롱클릭 = 확인 취소 + 전원 OFF (spec §5.2.1)
					startup_confirm_active = false;
					conform_hold_cnt = 0;
					remocon_power_ctrl(REMO_LCD_OFF);
					power = false;
					remocon_key.run = true;
					return;
				}
			}
			// 릴리즈 디바운스 (노이즈 내성: 리셋 대신 감소)
			if(!power_pressed){
				power_release_cnt++;
			} else {
				if(power_release_cnt > 0) power_release_cnt--;
			}
			// 안전 타임아웃: 오래 유지되면 강제 릴리즈 확정
			if(!power_pressed && power_active_frames > (LONG_KEY_CNT + KEY_CNT) * 2){
				power_release_cnt = KEY_CNT;
			}
		}

		// 릴리즈 확정 → 숏클릭만 처리 (롱클릭은 위에서 즉시 처리됨)
		if(power_key_active && power_release_cnt >= KEY_CNT){
			power_key_active = false;
			power_release_cnt = 0;
			power_hold_cnt = 0;
			// 숏클릭(누르고 뗌) → "종료중"/"초기화중" 표시 + STDBY 명령 송신
			// 단, 확인창 중에는 차단한다. 막지 않으면 3초 확인을 거치지 않고 홈이 나간다 (spec §5.2.1).
			if(!power_key_long && startup_confirm_active){
				debugprintf("\n\r KEY : POWER SHORT ignored (startup confirm active)");
			}
			else if(!power_key_long){
				debugprintf("\n\r KEY : POWER SHORT RELEASE -> STDBY HOME");
				U8 tmp = 0;
				stdby_in_progress = true;
				stdby_complete = false;
				// Master watchdog(90s)보다 길어야 한다. 30초면 정상 홈 도중 발화해
				// run_state를 강제 0으로 만들고 "대기중"을 표시한다 (spec §8).
				stdby_timeout = STDBY_TIMEOUT_TICKS;
				stdby_initial_mode = bed_status.current_mode;
				bed_state_lock_remain = 100;	// 2초 잠금
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_STDBY, &tmp, 0);
				smart_bed_status.status = MODE_HOME;
				smart_bed_display.display_refresh = true;
				power_off_pending = false;
			}
			power_key_long = false;
			remocon_key.run = true;
			return;
		}
	}

	// ---- 부팅 확인창: CONFORM 3초 long-press만 유효 (spec §5.2.1, §6) ----
	// 전원키는 위 블록에서 처리된다 (숏클릭 차단, 롱클릭=취소+OFF).
	// 나머지 키는 전부 무시한다. 우발적 클릭으로 침대가 움직이면 안 된다.
	// 낙상 경고 중에는 확인창보다 낙상 처리가 우선한다 (spec §9, T8).
	// 여기서 return하면 낙상 해제 키(Pause)까지 막히므로 반드시 제외한다.
	// 여기서는 키 상태만 보고한다. 홀드 시간 계산과 화면 갱신은
	// process_target_time_handler()의 100ms 틱에서 처리한다 (user_task.c).
	// key_read() 호출 횟수로 시간을 재면 안 된다 — 메인 루프 한 바퀴에
	// 화면 전체 재그리기가 끼면 루프 주기가 3배 이상 늘어나 3초가 11초가 된다.
	if(startup_confirm_active && power == true &&
	   smart_bed_status.status != MODE_FALL_ALERT){
		conform_key_held = !(remocon_key.current & CONFORM_KEY);
		// VOLUME(음량) / HEAT(온열) 키는 확인창 중에도 조작 허용 → 아래 처리 블록으로 통과.
		// (호밍 중과 동일하게 음량/온열 조작만 예외적으로 통과시킴)
		bool volume_pressed = !(remocon_key.current & VOLUME_KEY);
		bool heat_pressed   = !(remocon_key.current & HEAT_KEY);
		if(!volume_pressed && !heat_pressed){	// 둘 다 안 눌림 → 다른 키 차단
			remocon_key.run = true;
			return;
		}
		// VOLUME 또는 HEAT 눌림 → 통과 (하단 처리 로직 실행)
	}

	// 초기화(호밍) 중에는 화면 전환/조작 키를 차단한다. (전원 OFF는 위 홀드 로직에서 이미 처리됨)
	// 초기화가 끝나면 is_homing_active()==false가 되어 정상적으로 다른 화면으로 전환된다.
	// 단, 음량(VOLUME) / 온열(HEAT) 키는 음량/온열 전용이라 호밍 중에도 조절을 허용한다.
	if(is_homing_active()){
		bool volume_pressed = !(remocon_key.current & VOLUME_KEY);
		bool heat_pressed   = !(remocon_key.current & HEAT_KEY);
		if(!volume_pressed && !heat_pressed){	// 둘 다 안 눌림 → 다른 키 차단
			remocon_key.run = true;
			return;
		}
		// VOLUME 또는 HEAT 눌림 → 통과 (하단 처리 블록 실행)
	}

	// 초기위치 복귀 중에도 키 입력 허용 (위에서 자동 캔슬됨)
	// if(stdby_in_progress) return;	// 비활성화 — 사용자 키로 stdby 캔슬 가능

	// ---- 자세제어 모드: UP/DOWN 누르는 동안 모터 동작, 떼면 정지 ----
	{
		if(smart_bed_status.status == MODE_POSTURE && power == true){
			bool up_held = !(remocon_key.current & UP_KEY);
			bool down_held = !(remocon_key.current & DOWN_KEY);
			U8 tmp = 0;

			// UP 키 누름 → 모터 올림
			if(up_held && !posture_up_active){
				posture_up_active = true;
				if(cursor.type == POSTURE_BACK || cursor.type == POSTURE_ALL)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_UP, &tmp, 0);
				if(cursor.type == POSTURE_LEG || cursor.type == POSTURE_ALL)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_LEG_UP, &tmp, 0);
				if(cursor.type == POSTURE_HEIGHT)	// 높이: 침대 전체 올림 (홀드)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEIGHT_UP, &tmp, 0);
				debugprintf("\n\r POSTURE: MOTOR UP (type=%d)", cursor.type);
			}
			// UP 키 뗌 → 모터 정지
			if(!up_held && posture_up_active){
				posture_up_active = false;
				if(cursor.type == POSTURE_BACK || cursor.type == POSTURE_ALL)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_STOP, &tmp, 0);
				if(cursor.type == POSTURE_LEG || cursor.type == POSTURE_ALL)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_LEG_STOP, &tmp, 0);
				if(cursor.type == POSTURE_HEIGHT)	// 높이: 뗌 시 정지
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEIGHT_STOP, &tmp, 0);
				debugprintf("\n\r POSTURE: MOTOR UP STOP");
			}

			// DOWN 키 누름 → 모터 내림
			if(down_held && !posture_down_active){
				posture_down_active = true;
				if(cursor.type == POSTURE_BACK || cursor.type == POSTURE_ALL)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_DOWN, &tmp, 0);
				if(cursor.type == POSTURE_LEG || cursor.type == POSTURE_ALL)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_LEG_DOWN, &tmp, 0);
				if(cursor.type == POSTURE_HEIGHT)	// 높이: 침대 전체 내림 (홀드)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEIGHT_DOWN, &tmp, 0);
				debugprintf("\n\r POSTURE: MOTOR DOWN (type=%d)", cursor.type);
			}
			// DOWN 키 뗌 → 모터 정지
			if(!down_held && posture_down_active){
				posture_down_active = false;
				if(cursor.type == POSTURE_BACK || cursor.type == POSTURE_ALL)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_STOP, &tmp, 0);
				if(cursor.type == POSTURE_LEG || cursor.type == POSTURE_ALL)
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_LEG_STOP, &tmp, 0);
				if(cursor.type == POSTURE_HEIGHT)	// 높이: 뗌 시 정지
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEIGHT_STOP, &tmp, 0);
				debugprintf("\n\r POSTURE: MOTOR DOWN STOP");
			}
		} else {
			// 자세제어 모드가 아닌데 모터가 동작 중이면 강제 정지
			if(posture_up_active || posture_down_active){
				U8 tmp = 0;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_STOP, &tmp, 0);
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_LEG_STOP, &tmp, 0);
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEIGHT_STOP, &tmp, 0);
				posture_up_active = false;
				posture_down_active = false;
				debugprintf("\n\r POSTURE: MODE EXIT -> ALL STOP");
			}
		}
	}

	// ---- 돌봄케어 모드: 조그 버튼으로 수동 제어 ----
	{
		if(smart_bed_status.status == MODE_PATIENT_CARE && running_flag && power == true){
			U8 tmp = 0;

			if(cursor.type == PATIENT_MEAL){
				// 식사모드: UP/DOWN 조그 → 등판 각도 조절
				bool up_held = !(remocon_key.current & UP_KEY);
				bool down_held = !(remocon_key.current & DOWN_KEY);

				if(up_held && !care_up_active){
					care_up_active = true;
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_UP, &tmp, 0);
					debugprintf("\n\r CARE JOG: UP");
				}
				if(!up_held && care_up_active){
					care_up_active = false;
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_STOP, &tmp, 0);
					debugprintf("\n\r CARE JOG: UP STOP");
				}
				if(down_held && !care_down_active){
					care_down_active = true;
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_DOWN, &tmp, 0);
					debugprintf("\n\r CARE JOG: DOWN");
				}
				if(!down_held && care_down_active){
					care_down_active = false;
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_STOP, &tmp, 0);
					debugprintf("\n\r CARE JOG: DOWN STOP");
				}
			}

			if(cursor.type == PATIENT_TILT && tilt_care_ready){
				// 틸팅(분할 후): ◀/▶는 방향 선택(좌/우, patient_care_proc 처리),
				//   UP▲ 홀드 = 선택된 쪽 올림, DOWN▼ 홀드 = 선택된 쪽 내림, 떼면 정지.
				bool up_held   = !(remocon_key.current & UP_KEY);
				bool down_held = !(remocon_key.current & DOWN_KEY);
				U8 want = 0;	// 0=정지, 1=올림, 2=내림
				if(up_held)        want = 1;
				else if(down_held) want = 2;

				if(want != care_tilt_dir){
					U8 cmd;
					if(want == 1)      cmd = (tilt_sel == 0) ? CMD2_MOVE_LEFT      : CMD2_MOVE_RIGHT;
					else if(want == 2) cmd = (tilt_sel == 0) ? CMD2_MOVE_LEFT_DOWN : CMD2_MOVE_RIGHT_DOWN;
					else               cmd = CMD2_MOVE_CENTER;	// 정지(제자리)
					esp32_packet_send(CMD1_SEND_RUN_ST, cmd, &tmp, 0);
					care_tilt_dir = want;
					care_tilt_active = (want != 0);
					debugprintf("\n\r CARE JOG: TILT %s %s", tilt_sel == 0 ? "L" : "R",
					            want == 1 ? "UP" : (want == 2 ? "DOWN" : "STOP"));
				}
			}
		} else {
			// 케어 모드 벗어나면 조그 강제 정지
			if(care_up_active || care_down_active){
				U8 tmp = 0;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_STOP, &tmp, 0);
				care_up_active = false;
				care_down_active = false;
				debugprintf("\n\r CARE JOG: EXIT -> UP/DOWN STOP");
			}
			if(care_left_active || care_right_active || care_tilt_active){
				U8 tmp = 0;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_MOVE_CENTER, &tmp, 0);
				care_left_active = false;
				care_right_active = false;
				care_tilt_active = false;
				care_tilt_dir = 0;
				debugprintf("\n\r CARE JOG: EXIT -> TILT STOP");
			}
		}
	}

	if((remocon_key.pushed == true) && (remocon_key.run == false)){
		remocon_key.run = true;
// Power key - 전원 OFF 상태에서만 즉시 ON
		if(!(remocon_key.current & POWER_KEY)){
			if(power == false){
				debugprintf("\n\r KEY : POWER CLICK -> ON");
				remocon_power_ctrl(REMO_PWR_ON);
				power = true;
			}
			// power ON 상태에서는 릴리즈 블록에서 처리
			return;
		}
		if(power == false) return;
// CONFORM KEY	
		if(!(remocon_key.current & CONFORM_KEY)){
			remocon_key.key_val = CONFORM_KEY;
			debugprintf("\n\r KEY : CONFORM_KEY");
			return;
		}
// UP
		if(!(remocon_key.current & UP_KEY)){
			debugprintf("\n\r KEY : UP_KEY");
			remocon_key.key_val = UP_KEY;
			return;
		}
// DOWN
		if(!(remocon_key.current & DOWN_KEY)){
			debugprintf("\n\r KEY : DOWN_KEY");
			remocon_key.key_val = DOWN_KEY;
			return;
		}
// LEFT
		if(!(remocon_key.current & LEFT_KEY)){
			debugprintf("\n\r KEY : LEFT_KEY");
			remocon_key.key_val = LEFT_KEY;
			return;
		}
// RIGHT
		if(!(remocon_key.current & RIGHT_KEY)){
			debugprintf("\n\r KEY : RIGHT_KEY");
			remocon_key.key_val = RIGHT_KEY;
			return;
		}
// 체압분산
		if(!(remocon_key.current & VAIRANCE_KEY)){
			debugprintf("\n\r KEY : VAIRANCE_KEY (욕창케어)");
			smart_bed_status.status = MODE_ULCER_CARE;
			remocon_key.key_val = VAIRANCE_KEY;
			return;
		}
// 마사지 (LEVITATE_KEY 물리위치)
		if(!(remocon_key.current & LEVITATE_KEY)){
			debugprintf("\n\r KEY : LEVITATE_KEY (마사지)");
			smart_bed_status.status = MODE_MASSAGE;
			remocon_key.key_val = LEVITATE_KEY;
			return;
		}
		
// 돌봄케어 (MASSA_KEY 물리위치)
		if(!(remocon_key.current & MASSA_KEY)){
			debugprintf("\n\r KEY : MASSA_KEY (돌봄케어)");
			smart_bed_status.status = MODE_PATIENT_CARE;
			remocon_key.key_val = MASSA_KEY;
			return;
		}
// 자세제어 (CARE_KEY 물리위치 → 자세제어 토글)
		if(!(remocon_key.current & CARE_KEY)){
			if(smart_bed_status.status == MODE_POSTURE){
				U8 tmp = 0;
				debugprintf("\n\r KEY : CARE_KEY POSTURE -> HOME (ALL STOP)");
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_BACK_STOP, &tmp, 0);
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_POSTURE_LEG_STOP, &tmp, 0);
				posture_up_active = false;
				posture_down_active = false;
				smart_bed_status.status = MODE_HOME;
				smart_bed_display.status = MODE_HOME;
			} else {
				debugprintf("\n\r KEY : CARE_KEY -> POSTURE");
				smart_bed_status.status = MODE_POSTURE;
			}
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = CARE_KEY;
			return;
		}
// 온열 (화면 전환 없이 LED + 설정값만 순환)
		if(!(remocon_key.current & HEAT_KEY)){
			U8 tmp = 0;
			heat.body[0][0]++;
			if(heat.body[0][0] > 3)
				heat.body[0][0] = 0;
			debugprintf("\n\r KEY : HEAT_KEY level=%d", heat.body[0][0]);
			heat_led_ctrl(heat.body[0][0]);
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEAT + heat.body[0][0], &tmp, 0);
			return;
		}
// 음량 (화면 전환 없이 LED + 설정값만 순환: 0=음소거, 1=25, 2=50, 3=100)
		if(!(remocon_key.current & VOLUME_KEY)){
			U8 tmp = 0;
			volume.body[0][0]++;
			if(volume.body[0][0] > 3)
				volume.body[0][0] = 0;
			debugprintf("\n\r KEY : VOLUME level=%d", volume.body[0][0]);
			volume_led_ctrl(volume.body[0][0]);
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_VOLUME + volume.body[0][0], &tmp, 0);
			// 방금 조절 → 2초간 ESP 상태 패킷의 볼륨 되동기화 차단 (왕복 지연 중 되돌림 방지)
			vol_lock_remain = 20;
			return;
		}
// 설정/저장
		if(!(remocon_key.current & SET_KEY)){
			debugprintf("\n\r KEY : SET_KEY");
			if(smart_bed_status.status == MODE_HOME)
				smart_bed_status.status = MODE_SET_SAVE;
			remocon_key.key_val = SET_KEY;
			return;
		}
// 무중력 (INIT_KEY 물리위치 → 무중력 ↔ 플랫 토글)
		if(!(remocon_key.current & INIT_KEY)){
			static bool gravity_flat_toggle = false;	// false→무중력, true→플랫(수평)
			U8 tmp = 0;
			if(!gravity_flat_toggle){
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_GRAVITY, &tmp, 0);
				debugprintf("\n\r KEY : 무중력버튼 -> 무중력");
			} else {
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_FLAT, &tmp, 0);
				debugprintf("\n\r KEY : 무중력버튼 -> 플랫");
			}
			gravity_flat_toggle = !gravity_flat_toggle;
			remocon_key.key_val = INIT_KEY;
			return;
		}
	}
}
// void key_proc(void){
	// if(key_read(&key)){
	// }
// }



