#pragma once

#include "protocol.h"

#define _USB_ENABLE()	*R_GPOHIGH(8) = (1<<2)
#define _USB_DISABLE()	*R_GPOLOW(8) = (1<<2)

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
#define MASSAGE_MAX	12
extern U8 body_set_max[BODY_LEVIT_MAX];

enum{
	PATIENT_HEAD = 0,// 머리감기
	PATIENT_DEFEC,	// 배변
	PATIENT_SHIFT,		// 이동
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

extern BODY levitate[LEVIT_MAX];// 교대 부양(일반/집중/수면)
extern BODY dispersion;// 체압분산
extern BODY massage[12];	// 마사지
extern int running_massage_type;	// 현재 동작 중인 마사지 (-1: 없음, 0~11)
extern BODY patient_care[3];
extern BODY heag;
extern BODY temp_body;
extern BODY heat;
extern BODY ventilation;

extern void heat_led_ctrl(U8 led);
extern void ventilation_led_ctrl(U8 led);



//U8 patient_care;	//환자케어(머리감기/배변/이동)

extern void levitate_value_power_init(void);
extern void dispersion_value_power_init(void);
extern void massage_value_power_init(void);
extern void patient_care_value_power_init(void);

extern void levitate_proc(void);
extern void smart_bed_remocon_port_init(void);
extern U8 get_smart_bed_remocon_boot_mode(void);
extern bool usb_get_detection(void);
extern void get_bar_run_info(void);
extern void show_shutdown_screen(void);
extern void shutdown_draw(void);


