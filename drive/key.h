#pragma once

// 비트 14, 15 미사용 (get_spi_key에서 byte 1을 1비트 시프트 다운하여 비트 7 gap 제거)
#define VOLUME_KEY	(1<<13)	// 음량
#define HEAT_KEY	(1<<12)	// 온열
#define GRAVITY_KEY	(1<<11)	// 무중력 (무중력 ↔ 플랫 토글)
#define SET_KEY		(1<<10)	// 설정/저장
#define RIGHT_KEY	(1<<9)	// 우 / 값 증가
#define LEFT_KEY	(1<<8)	// 좌 / 값 감소
#define DOWN_KEY	(1<<7)	// 커서 아래
#define UP_KEY		(1<<6)	// 커서 위
#define CONFORM_KEY	(1<<5)	// 확인 (시작/일시정지/재시작)
#define POSTURE_KEY	(1<<4)	// 자세제어 (MODE_POSTURE 토글)
#define CARE_KEY	(1<<3)	// 돌봄케어 (MODE_PATIENT_CARE)
#define MASSAGE_KEY	(1<<2)	// 마사지 (MODE_MASSAGE)
#define ULCER_KEY	(1<<1)	// 욕창케어 (MODE_ULCER_CARE)
#define POWER_KEY	(1<<0)	// 전원 (ON / STDBY)

#define KEY_MASK	0x3FFF	// 비트 0~13 사용, 비트 14/15 미사용
#define KEY_CNT	1	// 디바운스 안정 프레임 수(반응 최대화 5→1). 0=디바운스 없음(노이즈 위험). 오작동 시 2~3으로.
#define LONG_KEY_CNT	300	// 롱클릭 판정 (~3초)

#define CURSOR_MODE	0
#define CURSOR_BODY	1
#define CURSOR_SET	2

typedef struct _key{
	bool pushed;
	bool run;
	bool long_pushed;	// 롱클릭 감지
	U32 count;
	U32 hold_count;		// 키 누름 유지 카운터
	U16 old;
	U16 current;
	U16 key_val;
} REMO_KEY;

extern REMO_KEY	remocon_key;



// typedef struct _key_config
// {
	// U8 mode;
	// U8 count;
// //	U8 position[3][7];

// }CURSOR;

 // extern CURSOR	cursor;

extern bool power ;

void key_init(void);
void key_read(void);
