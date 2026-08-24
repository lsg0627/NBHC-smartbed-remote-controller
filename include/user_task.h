#pragma once

typedef enum{
	MODE_HOME = 0,
	MODE_VAIRANCE,	// 체압분산
	MODE_LEVITATE,		// 교대부양
	MODE_MASSAGE, 		// 마사지
	MODE_PATIENT_CARE, 				// 환자케어
	MODE_HEAT, 		// 온열
	MODE_SET_SAVE, 		// 설정,저장
	MODE_INITIAL	,			// 초기화
	MODE_POSTURE,				// 자세제어
	MODE_SHUTDOWN,				// 종료 화면
	MODE_FALL_ALERT,			// 낙상 경고 화면
	MODE_STARTUP_CONFIRM,		// 부팅 확인창 (startup_pending=1)
	MODE_ULCER_CARE,			// 욕창케어 (체압분산 + 교대부양 통합)
	MODE_MAX
}_status;

typedef struct _lcd_st{
	U8 status;
	bool display_refresh;

}SMART_BED_DISP_STATUS;

typedef struct _smart_bed_st{
	U8 status;
	//bool display_refresh;

}SMART_BED_STATUS;

#define HEAD_VAIR		(1<<0)	// 머리
#define BODY_UP_VAIR	(1<<1)	// 몸통(상)
#define BODY_BELOW_VAIR	(1<<2)	// 몸통(하)
#define LEG_VAIR		(1<<3)	// 다리
#define BODY_UP_LEVAL	(1<<4)	// 몸체 상단
#define BODY_BELOW_LEVAL	(1<<5)	// 몸체 하단

// typedef struct _vairance{
	// U8 cur;	// 현재 커서위치
	// bool set_en;
	// U8 mode;
	// U8 status;
	// U8 body_up_val;// 몸체 상단
	// U8 body_below_val;// 몸체 하단
// }VAIRANCE_ST;
// extern VAIRANCE_ST vairance;
// extern VAIRANCE_ST vairance_temp;

extern EGL_FONT* g_pFont48;
extern EGL_FONT* g_pFont40;
extern EGL_FONT* g_pFont32;
extern EGL_FONT* g_pFont28;
extern EGL_FONT* g_pFont16;
extern EGL_FONT* g_pFontKor;	// 한글 bitfont (28px)
extern EGL_FONT* g_pFontKor16;	// 한글 bitfont (16px)


extern SMART_BED_DISP_STATUS smart_bed_display;
extern SMART_BED_STATUS smart_bed_status;
extern SURFACE *home_img;     // boot.suf
extern SURFACE *main_img;     // main.suf
extern SURFACE *btm1_img;
extern SURFACE *btm2_img;
extern SURFACE *vaira_title_imag;
extern SURFACE *vaira_main_icon_imag;
extern SURFACE *vaira_title_imag;
extern SURFACE *bar_img;
extern SURFACE *body_imag;
extern SURFACE *radio_avail_img;
extern SURFACE *radio_avail_selt_img;
extern SURFACE *radio_dis_img;
extern SURFACE *radio_dis_selt_img;
extern SURFACE *dip_en_num_img;
extern SURFACE *dip_ds_num_img;

extern SURFACE *set_title_img;
extern SURFACE *set_top_manual_sel_img;
extern SURFACE *set_top_manual_img;
extern SURFACE *set_top_selftest_sel_img;
extern SURFACE *set_top_selftest_img;
extern SURFACE *set_icon_img;

// 교대부양
extern SURFACE *levitate_title_img;
extern SURFACE *levitate_main_icon_img;
extern SURFACE *levitate_normal_icon_img;
extern SURFACE *levitate_concent_icon_img;
extern SURFACE *levitate_sleep_icon_img;
extern SURFACE *levitate_normal_sel_icon_img;
extern SURFACE *levitate_concent_sel_icon_img;
extern SURFACE *levitate_sleep_sel_icon_img;

// MASSAGE
extern SURFACE *massage_title_img;
extern SURFACE *massage_main_icon_img;
extern SURFACE *massage_sub_icon_img[12];

// 환자 케어
extern SURFACE *patient_title_img;
extern SURFACE *patient_main_icon_img;
extern SURFACE *patient_head_img;
extern SURFACE *patient_head_sel_img;
extern SURFACE *patient_defec_img;
extern SURFACE *patient_defec_sel_img;
extern SURFACE *patient_shift_img;
extern SURFACE *patient_shift_sel_img;

extern SURFACE *patient_head_main_img;	// 머리감기 이미지
extern SURFACE *patient_shift_main_img;	// 이동 이미지

extern SURFACE *heat_titile_img;
extern SURFACE *heat_mian_icon_img;
extern SURFACE *heat_on_img;
extern SURFACE *heat_off_img;

extern SURFACE *left_arrow_img;
extern SURFACE *right_arrow_img;
extern SURFACE *left_run_arrow_img;
extern SURFACE *right_run_arrow_img;
extern SURFACE *left_select_img;
extern SURFACE *right_select_img;

// 자세제어
extern SURFACE *posture_bg_img;
extern SURFACE *posture_back_plate_img;
extern SURFACE *posture_back_plate_a_img;
extern SURFACE *posture_back_plate_icon_img;
extern SURFACE *posture_leg_plate_img;
extern SURFACE *posture_leg_plate_a_img;
extern SURFACE *posture_leg_plate_icon_img;
extern SURFACE *posture_all_plate_img;
extern SURFACE *posture_all_plate_a_img;
extern SURFACE *posture_all_plate_icon_img;
extern SURFACE *posture_height_img;
extern SURFACE *posture_height_a_img;

extern SMART_BED_DISP_STATUS smart_bed_display;
extern SMART_BED_STATUS smart_bed_status;

extern U8 bar_status[16];

extern void progress_10ms_condition(void);

extern void image_load(void);
extern void load_font(void);
extern void progress_lcd_display(void);
extern void save_screenshot(void);
extern void remocon_power_ctrl(U8 remo_pwr);
extern void remocon_boot_power_on(void);	// 메인 전원과 함께 리모컨 자동 ON
extern void check_stdby_progress(void);

extern bool stdby_in_progress;	// 초기위치 복귀 중 (키 차단)
extern bool power_off_pending;	// ACK 후 전원 OFF 필요
extern bool stdby_complete;		// ESP32 ACK 수신 플래그
extern U32 stdby_timeout;		// 타임아웃 카운터

// 초기화(호밍) 화면 관련
extern U32 loading_anim_phase;		// 스피너 회전 위상 (0~7)
extern bool show_loading_screen;	// 부팅 초기 로딩
extern bool awaiting_homing_start;	// 전원 ON 후 ESP32 state=3 도착 대기
extern bool master_booted;			// 마스터 첫 BedStatus 수신 여부 (부팅 스피너 유지 조건)
extern bool is_homing_active(void);	// 초기화 화면 표시 + 키 차단 조건

// 홈 타임아웃 (100ms 틱). Master watchdog 90s + 브로드캐스트 2s + 마진 20s.
// 90s(900)로 잡으면 Master 복구 브로드캐스트 도착 전에 발화하여 경합함.
#define STDBY_TIMEOUT_TICKS		1100

// --- 부팅 확인창 (docs/remote_firmware_spec.md §5) ---
extern bool startup_confirm_active;	// 확인창 표시 중 (키 차단)
extern bool conform_key_held;		// key_read()가 매 루프 갱신. 시간 계산은 100ms 틱에서.
extern U32  conform_hold_cnt;		// 홀드 경과 (100ms 틱 단위)
extern void startup_confirm_accept(void);	// 3초 확인 완료 → CMD2_STDBY 송신

// 확인 long-press 시간. 100ms 틱 기준이므로 30 = 3.0초.
// 메인 루프 반복 횟수로 재면 화면 재그리기 부하에 따라 실제 시간이 3~4배로 늘어난다.
#define CONFORM_HOLD_TICKS		30
#define CONFORM_BAR_SEGMENTS	10

// --- 부팅 시 GetBedStatus 요청 (§3) ---
extern bool boot_status_done;		// 요청 절차 종료 (응답 수신 or fallback)
extern U8   boot_status_retry;		// 남은 재시도 횟수
extern U32  boot_status_wait;		// 응답 대기 (100ms 틱, 2 = 200ms)

// --- 통신 끊김 감지 (§7) ---
extern U32  bed_status_silence;		// 마지막 BedStatus 이후 경과 (100ms 틱)
extern bool link_lost;				// 4.5초 이상 무수신
#define LINK_LOST_TICKS			45	// 45 × 100ms = 4.5초 (브로드캐스트 2주기)

// --- Path B / §5.4 전이 감지용 이전 값 ---
extern U8 prev_startup_pending;

// 5초 무입력 자동 홈 복귀 타이머 (동작 중 모드 있을 때만 동작)
extern U32 auto_home_timer;		// 10ms 틱 카운트다운 (50 = 5초)

// 모드명 마퀴 스크롤 (좌측 헤더 박스에서 긴 모드명을 흘려 보여줌)
extern U32 mode_text_offset;	// 현재 스크롤 오프셋 (px)

// 로컬 상태 잠금 — 사용자 액션 후 ESP32 패킷이 current_mode/run_state를 덮어쓰지 못하게 함
extern U32 bed_state_lock_remain;	// 100ms 틱 단위 (30 = 3초)

// 볼륨 잠금 — 사용자가 방금 음량을 조절한 직후, ESP32 상태 패킷(왕복 지연 중 이전 값)이
// 리모컨 LED/카운터를 되돌리지 못하게 함. 0이면 ESP 보고값으로 동기화 허용.
extern U32 vol_lock_remain;	// 100ms 틱 단위 (20 = 2초)

// stdby 시작 시점의 mode 기억 (텍스트 일관성용)
extern U8 stdby_initial_mode;

// 외부(태블릿) 종료 감지 플래그
extern bool external_stopping;
extern U32 external_stopping_timeout;
extern U32 external_stopping_min_remain;	// 최소 표시 시간

// 모드 시작 ACK 직후 잠시 (false-positive stopping 방지)
extern bool pending_mode_start;
extern U32 pending_mode_start_timeout;

// 모드 전환 시 ESP32가 홈(init) 진행 중임을 추적. CONFORM 시점 → 홈 완료(2번째 state=1)까지.
// 활성 중엔: 경과시간 카운트 정지 + HOME 화면 "초기화 중" 표시.
// 감지 원리: ESP32는 CONFORM 처리 후 즉시 (mode,1)을 한 번 보내고(pre-homing),
//   홈 완료 시 BarsInitialized에서 (mode,1)을 또 보냄(post-homing).
//   state=1 패킷 count가 >= 2 되면 홈 완료로 판정 → 안정적으로 감지 (홈 시간 무관).
extern bool init_home_pending;
extern U32 init_home_pending_timeout;
extern U8 init_home_state1_count;	// 이번 init 사이클 동안 관측한 state=1 packet 개수

// 리모컨 전원 ON 후 첫 BED_STATUS로 침대 상태 확인 후 PWR_ON 조건부 송신
extern bool pending_pwr_on_check;
extern U32 pending_pwr_on_check_timeout;

// 마사지 모드 사이클 타이머
extern U32 massage_timer_elapsed_ms;
extern U8 massage_timer_mode;
extern const U32 massage_durations_ms[12];

// VAIRANCE/LEVITATE 모드 경과 시간 타이머
extern U32 mode_timer_elapsed_ms;
extern U8  mode_timer_mode;

// Pretendard 통합 폰트 사용 스위치 (한글/영문 모두 UTF-8 native).
// 1 : NAND에 pretendard{16,28}.fnt + _0.tga 배치 후 활성화 (40px는 기존 font40 유지)
// 0 : 기존 폰트 (font*.fnt + SDK bitfont) 사용 — 안전 기본값
// 생성 파일:
//   flash_image/image/font/pretendard16.fnt + pretendard16_0.tga (~70KB)
//   flash_image/image/font/pretendard28.fnt + pretendard28_0.tga (~270KB)
// [주의] 반드시 NAND에 .fnt+.tga 배치 후 flash 완료된 상태에서만 1로 변경할 것.
//        파일 없이 1로 두면 create_bmpfont()가 NULL 반환 → 첫 draw에서 크래시.
#define USE_PRETENDARD_FONT   0

// text_width 계산 함수 프로토타입 (Pretendard 프로포셔널 지원)
extern U32 estimate_text_width_28(const char* utf8_str);

