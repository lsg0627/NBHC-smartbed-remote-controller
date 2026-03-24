#pragma once

#define INIT_KEY		(1<<12) // 초기화
#define SET_KEY		(1<<11)	// 설정 저장
#define VENTIL_KEY (1<<14)	// 통풍
#define HEAT_KEY	(1<<13)	// 온열
#define RIGHT_KEY	(1<<10)	// right
#define LEFT_KEY		(1<<9)
#define DOWN_KEY	(1<<8)
#define UP_KEY		(1<<6)
#define CONFORM_KEY	(1<<5)	// 확인
#define CARE_KEY	(1<<4)	// 환자케어
#define MASSA_KEY	(1<<3)	// 마사지
#define LEVITATE_KEY		(1<<2)	// 교대부양
#define VAIRANCE_KEY	(1<<1)	// 체압분산
#define POWER_KEY	(1<<0)	// 전원

#define KEY_MASK	0x7F7F
#define KEY_CNT	5
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
