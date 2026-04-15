#pragma once

#define ESP32_UART_CH	1

#define ESP32_PACK_NUM	11

#define STAX	0
#define PRO_CODE	2
#define CMD_ID	6
#define ACTI_D	7
#define LENGTH	8
#define CHKSUM 10
#define TX_MAX_NUM	1024
#define RX_MAX_NUM	(1024*2)
#define RX_MAX_TIME	100	// 100msec


#define STX_CMD	0xFF81
#define PROD_CODE	0x53423143	//"SB1C"

// COMMAND ID
#define CMD1_SEND_RUN_ST	0x10	// 동작상태 전송 ( 교대부양, 체압분산, 마사지, 온열, 통풍, 온열, 동작상태, 전원, 초기화 )
#define CMD1_SEND_SET_VAL	0x20	// 설정 값 전송 ( 동작시간, RPM, 높이, 간격(동작주기), 마사지, 감도, 밸브, 자체검사 )
#define CMD1_SEND_GET_BAR	0x30
#define CMD1_GET_BAR_INFO	0x50	// 요청 (설정 값 요청, 동작 상태 요청 )
#define CMD1_GET_PRESSURE_MAP	0x60	// 체압맵 데이터 수신 (7x10)
#define CMD1_GET_BODY_INFO	0x70
#define CMD1_DATA_SYNC		0x80	// ESP32 → 리모컨 설정 동기화
//#define CMD_SEND_	0x50	// BED에서 보낸 정보

// -------- ACT COMMAND ---------
// 교대부양
#define CMD2_VENTIL_NORMAL	0x10	// 교대부양 일반
#define CMD2_VENTIL_FOCUR	0x11		// 교대부양 집중
#define CMD2_VENTIL_SLEEP	0x12		// 교대부양 수면

#define CMD2_PWR_ON	0x80	// POWER ON
#define CMD2_PWR_OFF 0x81	// POWER OFF
#define CMD2_INIT	0xFF	// 초기화 confirm(처음 키에 입력시)
#define CMD2_STDBY	0xF0	// 초기화 실시

#define CMD2_DISPERSION	0x20	// 체압분산

#define CMD2_PAUSE		0x70	// 일시정지
#define CMD2_RESTART	0x71	// 재동작

#define CMD2_MASSAGE	0x31	// massage 1

#define CMD2_HEAR		0x40	// 머리감기
#define CMD2_CATHARSIS	0x41	// 배변
#define CMD2_MOVE_LEFT	0X42	// LEFT KEY
#define CMD2_MOVE_RIGHT	0x43 // RIGHT KEY 
#define CMD2_MOVE_CENTER	0x44	// 틸트 원위치

#define CMD2_HEAT	0x50	// 온열 OFF
#define CMD2_HEAT1	0x51
#define CMD2_HEAT2	0x52
#define CMD2_HEAT3	0x53

#define CMD2_VENTIL	0x60	// (미사용 — 통풍 기능 없음)
#define CMD2_VOLUME	0xB0	// 볼륨 0 (음소거)
#define CMD2_VOLUME1	0xB1	// 볼륨 25
#define CMD2_VOLUME2	0xB2	// 볼륨 50
#define CMD2_VOLUME3	0xB3	// 볼륨 100

// 자세제어 (등판/다리판 모터)
#define CMD2_POSTURE_BACK_UP	0x45	// 등판 올림
#define CMD2_POSTURE_BACK_DOWN	0x46	// 등판 내림
#define CMD2_POSTURE_BACK_STOP	0x47	// 등판 정지
#define CMD2_POSTURE_LEG_UP		0x48	// 다리판 올림
#define CMD2_POSTURE_LEG_DOWN	0x49	// 다리판 내림
#define CMD2_POSTURE_LEG_STOP	0x4A	// 다리판 정지

// 낙상 경고 (메인보드 → 리모컨)
#define CMD2_FALL_ALERT		0xA0	// 낙상 경고 발생
#define CMD2_FALL_CLEAR		0xA1	// 낙상 경고 해제

// // ------- DATA ----------
// #define DATA_HEAD	0x10
// #define DATA_UPBODY	0x20
// #define DATA_DNBODY	0x30
// #define DATA_LEG	0x40





// #define CMD2_RUN		0x01
// #define CMD2_AUDIO	0x04
// #define CMD2_VENTIL	0x05	// ventilation(통풍)
// #define CMD2_HEAT	0x06	// 온열
// #define CMD2_MOTOR	0x07
// #define CMD2_LED		0x08
// #define CMD2_PWR		0x09	// 전원 OFF : 0 전원 ON : 1
// //#define CMD2_INIT		0x0A	// 전원 값 초기화

#define CMD3_START		0x00
#define CMD3_PAUSE	0x01
#define CMD3_RESTART	0x02

// //#define DISPERSION	0x05	// 체압분산
// #define MASSAGE_1	0x06
// #define MASSAGE_2	0x07
// #define MASSAGE_3	0x08
// #define MASSEGE_4	0x09
// #define MASSAGE_5	0x0A
// #define MASSAGE_6	0x0B
// #define MASSAGE_7	0x0C
// #define MASSAGE_8	0x0D
// #define MASSAGE_9	0x0E
// #define MASSAGE_10	0x0F
// #define MASSAGE_11	0x10
// #define MASSAGE_12	0x11

// #define CARE_HEAD_WARP	0x12	// 케어(머리 감기)
// #define CARE_BOWEL	0x13	// 케어(배변)
// #define CARE_LEFT	0x14	// 케어(위치이동 좌)
// #define CARE_RIGHT	0x15// 케어(위치이동 우)

//////////// 모터 동작 제어 //////////
#define MOTOR_STOP			0x01	// 모터 정지
#define MOTOR_NORMAL	0x02	// 정회전(정방향)
#define MOTOR_REVERSE	0x03	// 역회전(역방향)
#define MOTOR_INIT			0x04	// 초기 위치

//////////// LED 동작 제어 //////////
#define MAIN_LED_OFF	0x00
#define MAIN_LED_R	0x01
#define MAIN_LED_G	0x02
#define MAIN_LED_B	0x03
#define MAIN_LED_RG	0x04
#define MAIN_LED_RB	0x05
#define MAIN_LED_GB	0x06
#define MAIN_LED_RGB	0x07

//////////////////////////////////////////////////////////////////////////////////////////////////
// 설정 값 전송
// remote control -> Smart bed
// CMD1_SEND_SET_VAL( 0x20) :  rpm, 높이, 감도, 간격, 시간
//////////////////////////////////////////////////////////////////////////////////////////////////
#define RUN_TIME_10MIN	0x01	// 동작시간 10분
#define RUN_TIME_20MIN	0x02	// 동작시간 20분
#define RUN_TIME_30MIN	0x03	// 동작시간 30분
#define RUN_TIME_1HOUR	0x04	// 동작시간 1시간
#define RUN_TIME_6HOUR	0x05	// 동작시간 6시간
#define RUN_TIME_12HOUR	0x06	// 동작시간 12시간
#define RUN_TIME_24HOUR	0x07	// 동작시간 24시간
#define RPM_200	0x01	// rmp200
#define RPM_250	0x02	// rmp250
#define RPM_300	0x03	// rem 300
#define RUN_INTV_010MIN	0x01		// 동작간격 10분
#define RUN_INTV_020MIN	0x02		// 동작간격 20분
#define RUN_INTV_030MIN	0x03		// 동작간격 30분
#define RUN_INTV_040MIN	0x04		// 동작간격 40분
#define RUN_INTV_050MIN	0x05		// 동작간격 50분
#define RUN_INTV_060MIN	0x06		// 동작간격 60분
#define RUN_INTV_070MIN	0x07		// 동작간격 70분
#define RUN_INTV_080MIN	0x08		// 동작간격 80분
#define RUN_INTV_090MIN	0x09		// 동작간격 90분
#define RUN_INTV_100MIN	0x0A		// 동작간격 100분
#define RUN_INTV_110MIN	0x0B	// 동작간격 110분
#define RUN_INTV_120MIN	0x0C	// 동작간격 120분

#define HIGH_30	30	// BAR 높이
#define HIGH_60	60
#define HIGH_90	90
#define HIGH_120	120
#define HIGH_150	150
#define HIGH_170	170

#define SENSIT_42	0x01	// 4.2kps 감도
#define SENSIT_30	0x02	// 3.0kps
#define SENSIT_25	0x03	// 2.5kps
#define SENSIT_20	0x04	// 2.0kps
#define SENSIT_15	0x05	// 1.5kps
#define SENSIT_10	0x06	// 1.0kps



//////////////////////////////////////////////////////////////////////////////////////////////////
#define GET_BAR_INFO	0x01
#define GET_BAR_INFO_ALL 	0x0F
#define MOTOR_INFO	0x06

//////////////////////////////////////////////////////////////////////////////////////////////////
#define PACK_OK	0x00
#define PACK_CRC_ERR	0x01
#define PACK_UNKN_ERR	0x02
#define PACK_BAR_ERR	0x03
#define PACK_DLENG_ERR	0x05
#define PACK_STX_ERR	0x06
#define PACK_PROD_ERR	0x07


#define REMO_PWR_ON	0
#define REMO_LCD_ON	1
#define REMO_LCD_OFF 2
#define REMO_STDBY_OFF 3	// 초기위치 복귀 후 전원 OFF


#define PRESSURE_ROWS	10
#define PRESSURE_COLS	7
#define PRESSURE_MAP_SIZE	(PRESSURE_ROWS * PRESSURE_COLS)	// 70 bytes

#define GET_INFO_BAR	0
#define GET_INFO_BODY	1

// typedef struct _protocol
// {
    // U8 cmd_id;
    // U8 act_cmd;
    // U8 leng;
    // U8 buff[100];
// }PROTOCOL_PACKET;




typedef struct _esp32_comm
{
	bool rx_flag;
	int rx_read_pointer;
	int rx_write_pointer;
	U8 packet_status;
	U16 rx_time_count;
	U8 rx_buff[RX_MAX_NUM];
	U8 tx_buff[TX_MAX_NUM];
	U8 pt_buff[RX_MAX_NUM];
	int pt_cnt;
	U16 crc16;
	int data_leng;
	U8 comm_status;
} ESP32_COMM;

typedef struct _esp32_get{
	U16 get_info_time;
	U16 get_rx_time;
	U8 info_get_status;
	bool rx_flag;
} ESP32_GET;


extern ESP32_COMM esp32_uart;
extern ESP32_GET esp32_get_infomation;


extern void esp32_packet_send(U8 cmd_id, U8 act_id, U8 *buff, U8 data_leng);
extern bool esp32_packet_parsing(U8 cmd1, U8 cmd2, U8 leng, U8 *buff);
extern bool esp32_packet_receive(U8 cmd1, U8 cmd2, U8 get_size, U8 *buff);
extern  void esp32_get_info_bar_body(void);

extern void esp32_get_info_init(void);
extern void esp32_get_info(void);
