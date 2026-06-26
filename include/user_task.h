#pragma once

typedef enum{
	MODE_HOME = 0,
	MODE_VAIRANCE,	// 체압분산
	MODE_LEVITATE,		// 교대부양
	MODE_MASSAGE, 		// 마사지
	MODE_PATIENT_CARE, 				// 환자케어
	MODE_HEAT, 		// 온열
	MODE_VENTILATION,	// 통풍
	MODE_SET_SAVE, 		// 설정,저장
	MODE_INITIAL	,			// 초기화
	MODE_POSTURE,				// 자세제어
	MODE_SHUTDOWN,				// 종료 화면
	MODE_FALL_ALERT,			// 낙상 경고 화면
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

extern SURFACE *ventil_titile_img ;
extern SURFACE *ventil_mian_icon_img;
extern SURFACE *ventil_on_img;
extern SURFACE *ventil_off_img;
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
extern void check_stdby_progress(void);

extern bool stdby_in_progress;	// 초기위치 복귀 중 (키 차단)
extern bool power_off_pending;	// ACK 후 전원 OFF 필요
extern bool stdby_complete;		// ESP32 ACK 수신 플래그
extern U32 stdby_timeout;		// 타임아웃 카운터

// 5초 무입력 자동 홈 복귀 타이머 (동작 중 모드 있을 때만 동작)
extern U32 auto_home_timer;		// 10ms 틱 카운트다운 (50 = 5초)

// 모드명 마퀴 스크롤 (좌측 헤더 박스에서 긴 모드명을 흘려 보여줌)
extern U32 mode_text_offset;	// 현재 스크롤 오프셋 (px)

// 로컬 상태 잠금 — 사용자 액션 후 ESP32 패킷이 current_mode/run_state를 덮어쓰지 못하게 함
extern U32 bed_state_lock_remain;	// 100ms 틱 단위 (30 = 3초)

// stdby 시작 시점의 mode 기억 (텍스트 일관성용)
extern U8 stdby_initial_mode;

// 외부(태블릿) 종료 감지 플래그
extern bool external_stopping;
extern U32 external_stopping_timeout;
extern U32 external_stopping_min_remain;	// 최소 표시 시간

// 모드 시작 ACK 직후 잠시 (false-positive stopping 방지)
extern bool pending_mode_start;
extern U32 pending_mode_start_timeout;

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

