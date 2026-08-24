#pragma once

#include "protocol.h"

#define _USB_ENABLE()	*R_GPOHIGH(8) = (1<<2)
#define _USB_DISABLE()	*R_GPOLOW(8) = (1<<2)

extern U8 conform_key_run;
extern U16 radio_xy[4][2];
extern U16 num2_xy[2][2];
extern U16 num3_xy[3][2];
// ----교대 부양 ----
// ** 일반 **
//  - 머리 : 시간설정, 높이, 속도
// 	 - 몸통(상) : 시간설정, 높이, 속도
//  - 몸통(하) : 시간설정, 높이, 속도
// ** 집중 **
//  - 머리 : 시간설정, 높이, 속도
// 	 - 몸통(상) : 시간설정, 높이, 속도
//  - 몸통(하) : 시간설정, 높이, 속도
// ** 수면 **
//  - 머리 : 시간설정, 높이, 속도
// 	 - 몸통(상) : 시간설정, 높이, 속도
//  - 몸통(하) : 시간설정, 높이, 속도

enum _levit_mode{
	LEVIT_NOR = 0,	// 일반
	LEVIT_FOCUS,		// 집중
	LEVIT_SLEEP,		// 수면
	LEVIT_MAX
};

enum _body_reg
{
	BODY_HEAD = 0,
	BODY_UPPER,
	BODY_LOW,
	BODY_LEG,
	BODY_MAX
};
enum _body_levit
{
	BODY_ENABLE = 0,
	BODY_TIME = 0,
	BODY_HIGH,
	BODY_RPM,
	BODY_LEVIT_MAX,
};

enum _body_dispersion
{
	BODY_LEVEL = 0,
	BODY_SPEED,
	BODY_DSIP_MAX,

};

#define	BODY_MASS_MAX 2
#define MASSAGE_MAX	12		// ESP32 CMD 코드 범위 (0x31~0x3C)
#define MASSAGE_UI_MAX	8		// 리모컨 UI에 표시할 마사지 개수 (집중/트렌델/무중력/트위스트 삭제)
extern U8 body_set_max[BODY_LEVIT_MAX];

enum{
	PATIENT_MEAL = 0,	// 식사모드 (버튼 0, 상단)
	PATIENT_TILT,		// 틸팅 (버튼 1, 중간)
	PATIENT_TREND,		// 트렌델렌버그 (버튼 2, 하단)
};


#define MAX_BAR	16
typedef struct _body
{
	U8 body[BODY_MAX][BODY_LEVIT_MAX+1];
	//U8 body[BODY_MAX][12];
}BODY;


//extern BODY leviate[4];	// 교대부양, 체압분산, 마사지, 환자케어
extern U8 bar[MAX_BAR];
extern bool body_info;
extern U8 pressure_map[PRESSURE_ROWS][PRESSURE_COLS];
extern S16 motor_positions[11];	// ESP32에서 받은 11 모터 위치 (id 0~10, 0~3400)

extern U32 pressure_to_color(U8 value);
extern int motor_position_to_offset(S16 pos);

extern BODY levitate[LEVIT_MAX];// 교대 부양(일반/집중/수면)
extern BODY dispersion;// 체압분산
extern BODY massage[12];	// 마사지
extern int running_massage_type;	// 현재 동작 중인 마사지 (-1: 없음, 0~11)
extern bool running_flag;			// 돌봄케어 동작 중 플래그
extern bool tilt_care_ready;		// 돌봄 틸팅 준비 완료 (ESP32 tilt_care[] 종료 후 true)
extern int  tilt_sel;				// 틸팅 방향 선택 (0=좌틸팅, 1=우틸팅) — ◀/▶로 이동, CONFORM 홀드로 실행
extern BODY patient_care[3];
extern BODY heag;
extern BODY temp_body;
extern BODY heat;
extern BODY volume;

extern void heat_led_ctrl(U8 led);
extern void volume_led_ctrl(U8 led);



//U8 patient_care;	//환자케어(머리감기/배변/이동)

extern void levitate_value_power_init(void);
extern void dispersion_value_power_init(void);
extern void massage_value_power_init(void);
extern void patient_care_value_power_init(void);
extern void volume_value_power_init(void);

extern void levitate_proc(void);
extern void smart_bed_remocon_port_init(void);
extern U8 get_smart_bed_remocon_boot_mode(void);
extern bool usb_get_detection(void);
extern void get_bar_run_info(void);
extern void show_shutdown_screen(void);
extern void shutdown_draw(void);

// 자세제어
enum {
	POSTURE_BACK = 0,	// 등판
	POSTURE_LEG,		// 다리판
	POSTURE_ALL,		// 등·다리
	POSTURE_HEIGHT,		// 높이 (침대 전체 높이 조절)
	POSTURE_TYPE_MAX
};

extern void posture_proc(void);
extern void posture_draw(void);

// 욕창케어 (체압분산 + 교대부양 통합)
extern void ulcer_care_proc(void);
extern void ulcer_care_draw(void);

// 낙상 경고 화면
extern void fall_alert_draw(void);

// 부팅 확인창 / 통신 끊김 화면 (docs/remote_firmware_spec.md §6, §7)
extern void startup_confirm_draw(void);
extern void link_check_draw(void);

// 상태 오버레이 (홈 화면 등에서 호출)
extern void draw_status_overlay(void);

// 로딩 스피너 (전원 ON 직후 부팅 화면 위에 그림)
extern void draw_loading_spinner(int cx, int cy, U32 phase);

// 초기화(호밍) 화면 — 침대 그림 + 회전 스피너 + 안내 문구
extern void homing_draw(void);

// 모드 코드 → 기본 이름만 (예: "파도타기")
extern const char* get_mode_name(U8 mode);
// 모드 + 상태 → 풀 텍스트 (예: "파도타기 일시정지" / "초기화 중")
extern const char* get_mode_status_text(U8 mode, U8 state);

