#pragma once

#define LCD_ON()	*R_GPOHIGH(3) = (1<<5)
#define LCD_OFF()	*R_GPOLOW(3) = (1<<5)


#define SYS_TIMER_CH	1	// 0,1,2,3
#define SYS_TICK_1MS_DIV 6
#define SYS_TICK_10MS_DIV 62
#define SYS_TICK_100MS_DIV 611

//#define GET_CUR_SYS_TIME_CNT(b) b = *R_TM3CNT


////////// 매뉴얼 및 Self test //////////////
#define MANUAL_X  24
#define	MANUAL_Y  150
#define SEL_TEST_X 164
#define	SEL_TEST_Y 150

////////// 체압분산 /////////////////
#define HOME_ICON_X	15
#define HOME_ICON_Y	15
#define TITLE_X	65
#define TITLE_Y	0
#define MAIN_ICON_X	115
#define MAIN_ICON_Y	50

#define BAR_X	95	// 밸브
#define BAR_Y	214
#define BAR_I		16	// 밸브 간격

#define BODY_X	116
#define BODY_Y	216

#define RADIO_X	24
#define RADIO_HEAD_Y	228	// 머리
#define RADIO_BODYUP_Y		(RADIO_HEAD_Y + 50)	// 몸통(상)
#define RADIO_BODYDWN_Y	(RADIO_BODYUP_Y + 49)	// 몸통(하)
#define RADIO_LEG	(RADIO_BODYDWN_Y + 81)	// 다리

#define LABLE0_X	232	//(242, 210)
#define LABLE0_Y	210

#define LABLE1_X	LABLE0_X	//(242, 292)
#define LABLE1_Y	292

#define LABLE2_X	LABLE0_X	//(242, 374)
#define LABLE2_Y	374


///////////// 교대부양 /////////////////////
#define LEVIT_NOR_X	21
#define LEVIT_CON_X	(LEVIT_NOR_X + 90 + 4)
#define LEVIT_SLE_X	(LEVIT_CON_X + 90 + 4)
#define LEVIT_SUB_Y	148

//////////// 환자돌봄 /////////////////////
#define PATIENT_MAIN_IMG_X	24
#define PATIENT_MAIN_IMG_Y	214


//////////// 마사지 ///////////////////////
#define MASSAGE_SUB_X 70	// (320-180)/2 가운데 정렬
#define MASSAGE_SUB_Y 140



typedef struct _cur
{
	U8 mode;
	U8 type; // 일반, 집중, 수면
	U8 type_max;
	U8 body;	// 머리, 상체부, 하체부, 다리
	U8 set;	// 시간설정, 높이, 속도
	//U8 conform_mode;// start(1), pause/stop(0), select(2)
	U8 set_max;
}CURSOR;

extern CURSOR cursor;

// #define BAR_MAX 16
// typedef struct _display
// {
	// U8 type;
	// U8 body[4];// 머리, 상체부, 하체부, 다리
	// U8 set[3];// 마사지
	// U8 bar[BAR_MAX];
// }DISPLAY;


//extern DISPLAY lcd_screen;


////////// 체압 히트맵 /////////////////
#define HEATMAP_X       70
#define HEATMAP_Y       210
#define HEATMAP_CELL_W  22
#define HEATMAP_CELL_H  22
#define HEATMAP_GAP     1

extern bool spi_init_flag;
extern bool SPI_init(void);
extern void LCD_Init(void);


