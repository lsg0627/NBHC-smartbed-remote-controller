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
#define CMD1_SEND_RUN_ST	0x10	// 동작상태 전송 ( 교대부양, 체압분산, 마사지, 온열, 음량, 동작상태, 전원, 초기화 )
#define CMD1_SEND_SET_VAL	0x20	// 설정 값 전송 ( 동작시간, RPM, 높이, 간격(동작주기), 마사지, 감도, 밸브, 자체검사 )
#define CMD1_SEND_GET_BAR	0x30
#define CMD1_BED_STATUS		0x40	// 침대 상태 정보 (주기적 수신)
#define CMD1_GET_BAR_INFO	0x50	// 요청 (설정 값 요청, 동작 상태 요청 )
#define CMD1_GET_MOTOR_POSITION	0x52	// 11 모터 위치 (각 int16, 총 22 bytes)
#define CMD1_GET_PRESSURE_MAP	0x60	// 체압맵 데이터 수신 (7x10)
#define CMD1_GET_BODY_INFO	0x70
#define CMD1_DATA_SYNC		0x80	// ESP32 → 리모컨 설정 동기화
//#define CMD_SEND_	0x50	// BED에서 보낸 정보

// -------- ACT COMMAND ---------
// 카테고리별 nibble 배정: 1=체압분산, 2=교대부양, 3=마사지, 4=돌봄, 5=자세제어,
//                          6=무중력, 7=온열, 8=볼륨, 9=동작제어, A=전원, B=낙상/시스템

// [1] 체압분산
#define CMD2_DISPERSION		0x10	// 체압분산

// [2] 교대부양
#define CMD2_VENTIL_STD		0x20	// 교대부양 일반 (구 CMD2_VENTIL_NORMAL)
#define CMD2_VENTIL_FOCUS	0x21	// 교대부양 집중 (구 CMD2_VENTIL_FOCUR)
#define CMD2_VENTIL_SLEEP	0x22	// 교대부양 수면

// [3] 마사지 (0x30~0x37: 8종. CMD2_MASSAGE + offset 방식)
#define CMD2_MASSAGE		0x30	// massage 1

// [4] 돌봄
#define CMD2_READY_TILT		0x40	// 틸팅 준비 (구 CMD2_TILT_CARE)
#define CMD2_UP_LEFT_TILT	0x41	// 좌틸팅 상승 (구 CMD2_MOVE_LEFT)
#define CMD2_UP_RIGHT_TILT	0x42	// 우틸팅 상승 (구 CMD2_MOVE_RIGHT)
#define CMD2_PAUSE_TILT		0x43	// 틸트 정지 (구 CMD2_MOVE_CENTER)
#define CMD2_DOWN_LEFT_TILT	0x44	// 좌틸팅 하강 (구 CMD2_MOVE_LEFT_DOWN)
#define CMD2_DOWN_RIGHT_TILT 0x45	// 우틸팅 하강 (구 CMD2_MOVE_RIGHT_DOWN)
#define CMD2_MEAL			0x46	// 식사 모드
#define CMD2_TRENDELENBUG	0x47	// 거꾸리 (구 CMD2_TREND)
#define CMD2_HEAR			0x48	// 머리감기 (하위호환용)
#define CMD2_BOWEL			0x49	// 배변 (구 CMD2_CATHARSIS)

// [5] 자세제어 (등판/다리판/높이)
#define CMD2_UP_POSTURE_BACK	0x50	// 등판 상승 (구 CMD2_POSTURE_BACK_UP)
#define CMD2_PAUSE_POSTURE_BACK	0x51	// 등판 정지 (구 CMD2_POSTURE_BACK_STOP)
#define CMD2_DOWN_POSTURE_BACK	0x52	// 등판 하강 (구 CMD2_POSTURE_BACK_DOWN)
#define CMD2_UP_POSTURE_LEG		0x53	// 다리판 상승 (구 CMD2_POSTURE_LEG_UP)
#define CMD2_PAUSE_POSTURE_LEG	0x54	// 다리판 정지 (구 CMD2_POSTURE_LEG_STOP)
#define CMD2_DOWN_POSTURE_LEG	0x55	// 다리판 하강 (구 CMD2_POSTURE_LEG_DOWN)
#define CMD2_UP_HEIGHT			0x56	// 높이 상승 (구 CMD2_HEIGHT_UP)
#define CMD2_PAUSE_HEIGHT		0x57	// 높이 정지 (구 CMD2_HEIGHT_STOP)
#define CMD2_DOWN_HEIGHT		0x58	// 높이 하강 (구 CMD2_HEIGHT_DOWN)

// [6] 무중력
#define CMD2_FLAT			0x60	// 플랫 (수평)
#define CMD2_ZERO_GRAVITY	0x61	// 무중력 (구 CMD2_GRAVITY)

// [7] 온열
#define CMD2_HEAT			0x70	// 온열 OFF
#define CMD2_HEAT1			0x71
#define CMD2_HEAT2			0x72
#define CMD2_HEAT3			0x73

// [8] 볼륨
#define CMD2_VOLUME0		0x80	// 음소거 (구 CMD2_VOLUME)
#define CMD2_VOLUME1		0x81	// Low
#define CMD2_VOLUME2		0x82	// Mid
#define CMD2_VOLUME3		0x83	// Max

// [9] 동작 제어
#define CMD2_PAUSE			0x90	// 일시정지
#define CMD2_RESTART		0x91	// 재동작

// [A] 전원
#define CMD2_PWR_ON			0xA0	// POWER ON
#define CMD2_PWR_OFF		0xA1	// POWER OFF

// [B] 낙상 경고 (메인보드 → 리모컨)
#define CMD2_FALL_ALERT		0xB0	// 낙상 경고 발생
#define CMD2_FALL_CLEAR		0xB1	// 낙상 경고 해제

// [B] 시스템/초기화
#define CMD2_STDBY			0xB2	// 초기화 실시 (구 0xF0)
#define CMD2_GET_BED_STATUS	0xB3	// 침대 상태 즉시 요청 (구 0xF1)
#define CMD2_TILT_READY		0xB4	// (수신 전용) ESP32 tilt_care[] 완료 통지 (구 0xF2)
#define CMD2_INIT			0xB5	// 초기화 confirm (구 0xFF)
#define CMD2_PRESENCE_OFF	0xB6	// 재실 감지 자동종료 OFF (설정화면 토글 → 메인보드)
#define CMD2_PRESENCE_ON	0xB7	// 재실 감지 자동종료 ON

// 낙상 감지 기능이 아직 동작하지 않아 경고 화면을 띄우지 않는다.
// 기능이 살아나면 1로 바꾸면 된다 — 관련 로직(키 게이팅, 확인창 우선순위)은 그대로 남겨둠.
#define FALL_ALERT_UI_ENABLED	0

// // ------- DATA ----------
// #define DATA_HEAD	0x10
// #define DATA_UPBODY	0x20
// #define DATA_DNBODY	0x30
// #define DATA_LEG	0x40





// #define CMD2_RUN		0x01
// #define CMD2_AUDIO	0x04
// #define CMD2_HEAT	0x06	// 온열
// #define CMD2_MOTOR	0x07
// #define CMD2_LED		0x08
// #define CMD2_PWR		0x09	// 전원 OFF : 0 전원 ON : 1
// //#define CMD2_INIT		0x0A	// 전원 값 초기화

// --- 침대 상태 데이터 (CMD1_BED_STATUS, ESP32 → 리모컨 2초 주기) ---
typedef struct {
	U8 current_mode;       // [0] 현재 모드 (0x00=정지, 0x10=교대표준, 0x31=마사지01...)
	U8 run_state;          // [1] 동작 (0=정지, 1=동작중, 2=일시정지, 3=초기화중)
	U8 backrest_percent;   // [2] 등판 각도 (0~100%)
	U8 leg_panel_percent;  // [3] 다리판 각도 (0~100%)
	U8 heat_level;         // [4] 히터 (0=OFF, 1=약, 2=중, 3=강)
	U8 volume_level;       // [5] 볼륨 (0=Mute, 1=Low, 2=Mid, 3=Max)
	U8 fall_state;         // [6] 낙상 (0=정상, 1=주의, 2=경고, 3=알람) — 리모컨 미사용 (MODE_FALL_ALERT로 관리)
	U8 powered_on;         // [7] 전원 (0=OFF, 1=ON)
	U8 startup_pending;    // [8] 0=정상, 1=사용자 확인 필요 (Master v3.2.4+). 확인창의 단일 진실 원천
	U8 reserved;           // [9] 전시모드 플래그 (메인보드 EXHIBITION_MODE): 0=일반(집중/수면 표시), 1=전시(집중/수면 숨김)
	U8 height_percent;     // [10] 높이 각도 (0~100%) — 자세제어 '높이' 탭 상/하한 화살표 숨김용 (NineBed 전용)
	U8 zero_gravity;       // [11] 무중력 상태 (1=무중력 위치, 0=아님) — 우측 상단 표시용
	U32 elapsed_sec;       // [12..15] 현재 모드(체압분산/교대부양) 경과 초 — 메인보드 계산값.
	                       //      연결이 끊겼다 붙어도 메인보드 값을 그대로 표시하므로 초기화 안 됨.
	// 주의: 이 구조체는 packed가 아니므로 끝에 U8을 더하면 U32 정렬 패딩으로 sizeof가
	//       16→20으로 튀어 BedStatus 길이검사(dlen >= sizeof)가 깨진다. 그래서 [16]
	//       presence_detect는 구조체에 넣지 않고 g_presence_detect 전역으로 따로 파싱한다.
} BED_STATUS_DATA;

extern BED_STATUS_DATA bed_status;  // 전역 상태 (protocol.c에서 갱신)
extern U8 g_presence_detect;        // [16] 재실 감지 자동종료 설정 echo (0=OFF, 1=ON) — 메인보드 NVS값
extern U8 g_bed_variant;            // [17] 베드 종류 (0=AnyBed, 1=NineBed). AnyBed면 자세제어 '높이' 탭 숨김

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


#define PRESSURE_ROWS	11	// ESP32 NUM_MOTOR_DRIVERS (실제 건반 11개, id 0~10)
#define PRESSURE_COLS	7	// ESP32 NUM_LOAD_CELLS
#define PRESSURE_MAP_SIZE	(PRESSURE_ROWS * PRESSURE_COLS)	// 77 bytes

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
