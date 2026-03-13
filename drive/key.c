#include "../app/main.h"
\
/*******************************************************************
	Smart bed remocon
	Key read
******************************************************************/
extern void spi_wait_finish (int ch);

#define KEY_CS_LOW0() 	*R_GPOLOW(4) = (1<<1)	// P4.1
#define KEY_CS_HIGH0() 	*R_GPOHIGH(4) = (1<<1)	// P4.1
#define LATCH_LOW0() 		*R_GPOLOW(4) = (1<<0)	// P4.0 
#define LATCH_HIGH0()		*R_GPOHIGH(4) = (1<<0)	// P4.0
#define KEY_CS_LOW1()		*R_GPOLOW(3) = (1<<7)	// P3.7
#define KEY_CS_HIGH1()	*R_GPOHIGH(3) = (1<<7)	// P3.7
#define LATCH_LOW1()		*R_GPOLOW(3) = (1<<6)	// P3.6
#define LATCH_HIGH1()		*R_GPOHIGH(3) = (1<<6) 	// P3.6

REMO_KEY	remocon_key;
CURSOR	cursor;
bool power = false;

void key_init(void){
	
	KEY_CS_HIGH0();
	KEY_CS_HIGH1();
	
	memset(&remocon_key,0xFF, sizeof(REMO_KEY));
	remocon_key.count = KEY_CNT;
}

U16 get_spi_key(void){
	U16 key0 = 0;
	U16 key1 = 0;

	// latch
	LATCH_LOW0();
	delayus(200);
	LATCH_HIGH0();
	delayus(500);
	
	// cs
	KEY_CS_LOW0();
	delayus(500);
	*R_SPI0DATA(1) = 0xFF;
	spi_wait_finish(1);
	delayus(100);
	key0 = *R_SPI0DATA(1);
	KEY_CS_HIGH0();
	delayus(100);
	
	// latch
	LATCH_LOW1();
	delayus(200);
	LATCH_HIGH1();
	delayus(500);
	
	// cs
	KEY_CS_LOW1();
	delayus(500);
	*R_SPI0DATA(1) = 0xFF;
	spi_wait_finish(1);
	delayus(100);
	key1 = *R_SPI0DATA(1);
	KEY_CS_HIGH1();
	delayus(100);
	return ((key1<<8) | key0);
}


void key_proc(void){
	remocon_key.current = get_spi_key();
	if(remocon_key.old != remocon_key.current){
		debugprintf("\n\r old[%x], cur[%x]", remocon_key.old, remocon_key.current);
		remocon_key.old = remocon_key.current;
		remocon_key.count = KEY_CNT;
		remocon_key.pushed = false;
		remocon_key.run = false;
		remocon_key.long_pushed = false;
		remocon_key.hold_count = 0;
		return;
	}
	else{
		if(remocon_key.count){
			remocon_key.count--;
			return;
		}
		else{
			if((remocon_key.current  & KEY_MASK) == KEY_MASK){// all key not press
				remocon_key.pushed = false;
				remocon_key.run = false;
				remocon_key.long_pushed = false;
				remocon_key.hold_count = 0;
				return;
			}
			if((remocon_key.pushed == false) && (remocon_key.run == false)){
				remocon_key.pushed = true;
				remocon_key.hold_count = 0;
				return;
			}
			// 키가 계속 눌려있으면 홀드 카운터 증가
			if(remocon_key.pushed == true){
				remocon_key.hold_count++;
				if(remocon_key.hold_count >= LONG_KEY_CNT){
					remocon_key.long_pushed = true;
				}
			}
		}
	}
}

// 전원키 릴리즈 감지용
static bool power_key_active = false;	// 전원키 추적 중
static bool power_key_long = false;		// 롱클릭 감지됨

void key_read(void){
	key_proc();

	bool power_pressed = !(remocon_key.current & POWER_KEY);

	// ---- 전원 ON 상태: 전원키 릴리즈 시점에서 숏/롱 판별 ----
	if(power == true){
		// 전원키 누름 감지 → 추적 시작 (stdby 중에도 동작)
		if(!power_key_active && power_pressed && remocon_key.pushed && !remocon_key.run){
			power_key_active = true;
			power_key_long = false;
			remocon_key.run = true;	// 다른 키 처리 방지
		}

		// 누르고 있는 동안 롱클릭 감지 업데이트
		if(power_key_active && remocon_key.long_pushed){
			power_key_long = true;
		}

		// 키 릴리즈 감지 → 동작 실행
		if(power_key_active && !power_pressed){
			power_key_active = false;
			if(power_key_long){
				// 롱클릭 → STDBY OFF
				debugprintf("\n\r KEY : POWER LONG RELEASE -> STDBY OFF");
				if(!stdby_in_progress){
					remocon_power_ctrl(REMO_STDBY_OFF);
				} else {
					power_off_pending = true;
				}
				power = false;
			} else {
				// 숏클릭 → STDBY HOME (stdby 중에는 무시)
				if(!stdby_in_progress){
					debugprintf("\n\r KEY : POWER SHORT RELEASE -> STDBY HOME");
					U8 tmp = 0;
					esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_STDBY, &tmp, 0);
					smart_bed_status.status = MODE_HOME;
					smart_bed_display.display_refresh = true;
					stdby_in_progress = true;
					stdby_complete = false;
					power_off_pending = false;
					stdby_timeout = 6000;
				}
			}
			power_key_long = false;
			remocon_key.long_pushed = false;
			remocon_key.hold_count = 0;
			remocon_key.run = true;
			return;
		}
	}

	// 초기위치 복귀 중이면 나머지 키 입력 차단
	if(stdby_in_progress) return;

	if((remocon_key.pushed == true) && (remocon_key.run == false)){
		remocon_key.run = true;
// Power key - 전원 OFF 상태에서만 즉시 ON
		if(!(remocon_key.current & POWER_KEY)){
			if(power == false){
				debugprintf("\n\r KEY : POWER CLICK -> ON");
				remocon_power_ctrl(REMO_PWR_ON);
				power = true;
			}
			// power ON 상태에서는 릴리즈 블록에서 처리
			return;
		}
		if(power == false) return;
// CONFORM KEY	
		if(!(remocon_key.current & CONFORM_KEY)){
			remocon_key.key_val = CONFORM_KEY;
			debugprintf("\n\r KEY : CONFORM_KEY");
			return;
		}
// UP
		if(!(remocon_key.current & UP_KEY)){
			debugprintf("\n\r KEY : UP_KEY");
			remocon_key.key_val = UP_KEY;
			return;
		}
// DOWN
		if(!(remocon_key.current & DOWN_KEY)){
			debugprintf("\n\r KEY : DOWN_KEY");
			remocon_key.key_val = DOWN_KEY;
			return;
		}
// LEFT
		if(!(remocon_key.current & LEFT_KEY)){
			debugprintf("\n\r KEY : LEFT_KEY");
			remocon_key.key_val = LEFT_KEY;
			return;
		}
// RIGHT
		if(!(remocon_key.current & RIGHT_KEY)){
			debugprintf("\n\r KEY : RIGHT_KEY");
			remocon_key.key_val = RIGHT_KEY;
			return;
		}
// 체압분산
		if(!(remocon_key.current & VAIRANCE_KEY)){
			debugprintf("\n\r KEY : VAIRANCE_KEY");
			smart_bed_status.status = MODE_VAIRANCE;
			remocon_key.key_val = VAIRANCE_KEY;
			return;
		}
// 교대부양
		if(!(remocon_key.current & LEVITATE_KEY)){
			debugprintf("\n\r KEY : LEVITATE_KEY");
			smart_bed_status.status = MODE_LEVITATE;
			remocon_key.key_val = LEVITATE_KEY;
			return;
		}
		
// 마사지
		if(!(remocon_key.current & MASSA_KEY)){
			debugprintf("\n\r KEY : MASSA_KEY");
			smart_bed_status.status = MODE_MASSAGE;
			remocon_key.key_val = MASSA_KEY;
			return;
		}
// 환자케어
		if(!(remocon_key.current & CARE_KEY)){
			debugprintf("\n\r KEY : CARE_KEY");
			smart_bed_status.status = MODE_PATIENT_CARE;
			remocon_key.key_val = CARE_KEY;
			return;
		}
// 온열 (화면 전환 없이 LED + 설정값만 순환)
		if(!(remocon_key.current & HEAT_KEY)){
			U8 tmp = 0;
			heat.body[0][0]++;
			if(heat.body[0][0] > 3)
				heat.body[0][0] = 0;
			debugprintf("\n\r KEY : HEAT_KEY level=%d", heat.body[0][0]);
			heat_led_ctrl(heat.body[0][0]);
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEAT + heat.body[0][0], &tmp, 0);
			return;
		}
// 통풍 (화면 전환 없이 LED + 설정값만 순환)
		if(!(remocon_key.current & VENTIL_KEY)){
			U8 tmp = 0;
			ventilation.body[0][0]++;
			if(ventilation.body[0][0] > 3)
				ventilation.body[0][0] = 0;
			debugprintf("\n\r KEY : VENTIL_KEY level=%d", ventilation.body[0][0]);
			ventilation_led_ctrl(ventilation.body[0][0]);
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_VENTIL + ventilation.body[0][0], &tmp, 0);
			return;
		}
// 설정/저장
		if(!(remocon_key.current & SET_KEY)){
			debugprintf("\n\r KEY : SET_KEY");
			if(smart_bed_status.status == MODE_HOME)
				smart_bed_status.status = MODE_SET_SAVE;
			remocon_key.key_val = SET_KEY;
			return;
		}
// 초기화
		if(!(remocon_key.current & INIT_KEY)){
			debugprintf("\n\r KEY : INIT_KEY");
			smart_bed_status.status = MODE_INITIAL;
			remocon_key.key_val = INIT_KEY;
			return;
		}
	}
}
// void key_proc(void){
	// if(key_read(&key)){
	// }
// }



