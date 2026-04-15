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
extern SURFACE *home_img;
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
extern void remocon_power_ctrl(U8 remo_pwr);
extern void check_stdby_progress(void);

extern bool stdby_in_progress;	// 초기위치 복귀 중 (키 차단)
extern bool power_off_pending;	// ACK 후 전원 OFF 필요
extern bool stdby_complete;		// ESP32 ACK 수신 플래그
extern U32 stdby_timeout;		// 타임아웃 카운터

