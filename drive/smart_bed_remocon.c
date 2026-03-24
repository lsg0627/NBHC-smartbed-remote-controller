#include "../app/main.h"
#include "smart_bed_remocon.h"
#include "etc_driver/misc.h"


U8 conform_key_run = 0;
int running_massage_type = -1;	// 현재 동작 중인 마사지 (-1: 없음, 0~11: 마사지 번호)
U8 body_set_max[BODY_LEVIT_MAX] = {0,};
U8 bar[16] = {0,};
bool body_info = false;
U8 pressure_map[PRESSURE_ROWS][PRESSURE_COLS] = {{0,}};
BODY levitate[LEVIT_MAX];// 교대 부양(일반/집중/수면)
BODY dispersion;// 체압분산
BODY massage[12];	// 마사지
BODY patient_care[3];	// 환자 돌봄케어
BODY heat;	// 온열
BODY ventilation;	// 통풍
BODY temp_body;

CURSOR cursor;



U16 radio_xy[4][2] = {
	{ RADIO_X, RADIO_HEAD_Y},// 머리 
	{ RADIO_X, RADIO_BODYUP_Y},	// 몸통(상부)
	{ RADIO_X, RADIO_BODYDWN_Y},	// 몸통(하부)
	{ RADIO_X, RADIO_LEG},// 다리
};

U16 num2_xy[2][2] = {
// 2num
	{ LABLE0_X, (LABLE0_Y + 50)},
	{ LABLE1_X, (LABLE1_Y + 70)},
};
U16 num3_xy[3][2] = {
// 3num
	{ LABLE0_X, LABLE0_Y},
	{ LABLE1_X, LABLE1_Y},
	{ LABLE2_X, LABLE2_Y},
};
//////////////////////////////////////////////////////////////////////////////
// UTF-8 → EUC-KR 변환 유틸리티
//////////////////////////////////////////////////////////////////////////////
extern int unicode_to_oem(unsigned short src);

// UTF-8 문자열을 EUC-KR로 변환
static void utf8_to_euckr(const char* utf8, char* euckr, int max_len)
{
	int i = 0, j = 0;
	while(utf8[i] && j < max_len - 2){
		U8 c = (U8)utf8[i];
		if(c < 0x80){
			// ASCII
			euckr[j++] = c;
			i++;
		}
		else if((c & 0xE0) == 0xC0){
			// 2바이트 UTF-8
			U16 uni = ((c & 0x1F) << 6) | (utf8[i+1] & 0x3F);
			int oem = unicode_to_oem(uni);
			if(oem > 0xFF){ euckr[j++] = (oem >> 8); euckr[j++] = (oem & 0xFF); }
			else { euckr[j++] = (oem & 0xFF); }
			i += 2;
		}
		else if((c & 0xF0) == 0xE0){
			// 3바이트 UTF-8 (한글 범위)
			U16 uni = ((c & 0x0F) << 12) | ((utf8[i+1] & 0x3F) << 6) | (utf8[i+2] & 0x3F);
			int oem = unicode_to_oem(uni);
			if(oem > 0xFF){ euckr[j++] = (oem >> 8); euckr[j++] = (oem & 0xFF); }
			else { euckr[j++] = (oem & 0xFF); }
			i += 3;
		}
		else{
			i++;
		}
	}
	euckr[j] = '\0';
}

// 한글 텍스트 출력 래퍼
static void draw_text_kr(EGL_FONT* font, int x, int y, const char* utf8_str)
{
	char euckr[256];
	utf8_to_euckr(utf8_str, euckr, sizeof(euckr));
	bitfont_draw(font, x, y, euckr);
}

// 종료 화면 그리기 (progress_lcd_display에서 호출)
void shutdown_draw(void)
{
	set_draw_target(getbackframe());
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0, 0, 0));
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
	draw_text_kr(g_pFontKor, 95, 226, "종료합니다.");
	flip();
}

// 종료 화면 표시 (LCD OFF 직전, 딜레이 포함)
void show_shutdown_screen(void)
{
	shutdown_draw();
	delayms(1500);
}

// 설정 모드 표시 바 (임시 비활성)
static void draw_mode_indicator(void)
{
	// 오버레이 비활성
}

// 선택 부위 하이라이트 테두리 (임시 비활성)
static void draw_body_highlight(void)
{
	// 오버레이 비활성
}

//////////////////////////////////////////////////////////////////////////////
// SMART BED REMOCON portinit
//////////////////////////////////////////////////////////////////////////////
void smart_bed_remocon_port_init(void)
{
// P0.7 -> GPIO_IN : Not USED
// P0.6 -> S FLASH
// P0.5 -> S FLASH
// P0.4 -> S FLASH
// P0.3 -> GPIO IN : USER MODE
// P0.2 -> GPIO_IN : USER MODE
// P0.1 -> GPIO IN :  Not USED
// P0.0 -> GPIO IN : Not USED
	*R_PAF0 = F_PAF0_7_GP7 | F_PAF0_6_SFLASH_DQ2 | F_PAF0_5_SFLASH_DQ1 | F_PAF0_4_SFLASH_CS | 
					 F_PAF0_3_GP3 | F_PAF0_2_GP2 | F_PAF0_1_GP1 | F_PAF0_0_GP0;
	
// P1.7 -> NAND_nBUSY
// P1.6 -> NAND_nRE
// P1.5 -> NAND_nWE
// P1.4 -> NAND_CLE
// P1.3-> NAND ALE
// P1.2-> NAND CS
// P1.1 -> GPIO OUT : Not USED
// P1.0 -> UART TX0(Debug mode)
	*R_PAF1 = F_PAF1_7_NF_BUSYX | F_PAF1_6_NF_RE | F_PAF1_5_NF_WE | F_PAF1_4_NF_CLE | F_PAF1_3_NF_ALE | 
					 F_PAF1_2_NF_CS | F_PAF1_1_GP1 | F_PAF1_0_UART_TX0;
	*R_GPODIR(1) = (1<<1);// SD Card Detector LED
	
	*R_PAF2 = 0x0000; //nand data
	
// P3.7 -> GPIO OUT : KEY READ CS1
// P3.6 -> GPIO OUT : KEY READ LATCH1
// P3.5 -> TIMER OUT2 : LCD BACKLIGHT PWM
// P3.4 -> GPIO OUT : LED
// P3.3 -> GPIO OUT : LED
// P3.2 -> GPIO OUT ; LED
// P3.1 -> GPIO OUT : LED
// P3.0 -> GPIO OUT : LED
	*R_PAF3 = F_PAF3_7_GP7  | F_PAF3_6_GP6 | F_PAF3_5_GP5 | F_PAF3_4_GP4 | F_PAF3_3_GP3 |
					 F_PAF3_2_GP2 | F_PAF3_1_GP1 | F_PAF3_0_GP0;
	*R_GPPUEN(3) = (1<<7) | (1<<6) ;
	*R_GPODIR(3) = (1<<7) | (1<<6) | (1<<5) | (1<<4) | (1<<3) | (1<<2) | (1<<1) | (1<<0);
	*R_GPOLOW(3) = (1<<5);// LCD Backlingh Off
	*R_GPOHIGH(3) = (1<<7) | (1<<6) | (1<<4) |(1<<3) | (1<<2) | (1<<1) | (1<<0);
	
	
// P4.7 -> GPIO OUT : LCD CS
// P4.6 -> SPI SCK1 : LCD, KEY READ
// P4.5 -> GPIO OUT : Not USED
// P4.4 -> GPIO OUT : LED
// P4.3 -> UART_RX1
// P4.2 -> UART_TX1
// P4.1 -> GPIO OUT : KEY READ CS 0
// P4.0 -> GPIO OUT : KEY READ LATCH 0
	*R_PAF4 = F_PAF4_7_GP7 | F_PAF4_6_SPI_SCK1 | F_PAF4_5_GP5 | F_PAF4_4_GP4 | 
					 F_PAF4_3_UART_RX1 | F_PAF4_2_UART_TX1 | F_PAF4_1_GP1 | F_PAF4_0_GP0;	
	*R_GPPUEN(4) = (1<<7) | (1<<1) | (1<<0);	
	*R_GPODIR(4) =  (1<<7) | (1<<5) | (1<<4) | (1<<1) | (1<<0);
	*R_GPOHIGH(4) = (1<<7) | (1<<4) | (1<<1) | (1<<0);
	
// P5.7 -> LCD DOT CLOCK : LCD
// P5.6 -> DISP ENABLE : LCD
// P5.5 -> HSYNC : LCD
// P5.4 -> VSYNC : LCD
// P5.3 -> GPIO : LCD RESET
// P5.2 -> GPIO : LCD RS
// P5.1 -> SPI MOSI1 : LCD
// P5.0 -> SPI MISO1 : KEY
	*R_PAF5 = F_PAF5_7_CRTC_CLK_OUT | F_PAF5_6_DISP_EN | F_PAF5_5_HSYNC | F_PAF5_4_VSYNC | 
					 F_PAF5_3_GP3 | F_PAF5_2_GP2 | F_PAF5_1_SPI_MOSI1 | F_PAF5_0_SPI_MISO1;
	*R_GPPUEN(5) = (1<<3) | (1<<2);	
	*R_GPODIR(5) = (1<<3) | (1<<2);		 
	*R_GPOHIGH(5) = (1<<2)| (1<<3);
	//*R_GPOLOW(5) = (1<<3);
	
	// *R_PAF5 = F_PAF5_7_CRTC_CLK_OUT | F_PAF5_6_DISP_EN | F_PAF5_5_GP5 | F_PAF5_4_GP4 | 
					 // F_PAF5_3_GP3 | F_PAF5_2_GP2 | F_PAF5_1_SPI_MOSI1 | F_PAF5_0_SPI_MISO1;
	// *R_GPPUEN(5) = (1<<3) | (1<<2);	
	// *R_GPODIR(5) = (1<<5) | (1<<4) | (1<<3) | (1<<2);		 
	// *R_GPOHIGH(5) = (1<<5)| (1<<4);
	
// P6 -> RED[7:0]
	*R_PAF6 = 0x0000;
	
// P[7:3] -> GREEN[7:3]
	*R_PAF7 = 0x0000;
	
// P8.7 -> B7
// P8.6 -> B6
// P8.5 -> B5
// P8.4 -> B4
// P8.3 -> B3
// P8.2 -> GPIO OUT : USB nDISABLE
// P8.1 -> GPIO OUT : USB Detection
// P8.0 -> GPIO OUT : Not USED
	*R_PAF8 = F_PAF8_2_GP2 | F_PAF8_1_GP1 | F_PAF8_0_GP0;
	*R_GPODIR(8) = (1<<2) | (1<<0);
	*R_GPOLOW(8) = (1<<2) | (1<<0);	
	
// P9 -> Serial Flash
	*R_PAF9 =F_PAF9_2_SFLASH_DQ3 | F_PAF9_1_SFLASH_CLK | F_PAF9_0_SFLASH_DQ0;
	
}

int count_digits_string(int num)
{
	U8 str[100] = {0, };
	
	sprintf(str, "%d", num);
	return strlen(str);
}

 /////////////////////////////////////////////////////////////////////////////////////////////////
// booting mode 확인 함수
// GPIO0.2, GPIO0.3
// 11xx : Normal boot
// 10xx : 매뉴얼 모드
// 01xx : Masstorage Mode
//////////////////////////////////////////////////////////////////////////////////////////////////
U8 get_smart_bed_remocon_boot_mode(void)
{
	U8 boot_mode;
	
	boot_mode = *R_GPILEV(0);
	return boot_mode&((1<<3) | (1<<2));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// USB DETECTION
// PORT : GPIO8 1
// Detection : LOW
// NO : LOW
// return : true   -> detection
// return : false -> non detection
//////////////////////////////////////////////////////////////////////////////////////////////////
bool usb_get_detection(void)
{
	int level_cnt = 0;
	U8 usb_level = 0, usb_level1 = 0;
	
	while(1){
		usb_level = (*R_GPILEV(8) & 0x02);
		if(usb_level != usb_level1)
		{
			usb_level1 = usb_level;
			level_cnt = 0;
		}
		else{
			level_cnt++;
			if(level_cnt >20){
				if(usb_level == 0x02) return false;
				return true;
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//
// LED OUT
//
//////////////////////////////////////////////////////////////////////////////////////////////////
// P3.0, P3.1, P3.2
#define test_led	// tera
void heat_led_ctrl(U8 led){
	// All OFF first
	*R_GPOHIGH(3) = (1<<0);
	*R_GPOHIGH(3) = (1<<1);
	*R_GPOHIGH(3) = (1<<2);

	// 누적 점등: 왼쪽(P3.2)부터 오른쪽(P3.0)으로
	if(led >= 1)
		*R_GPOLOW(3) = (1<<2);  // 왼쪽 LED
	if(led >= 2)
		*R_GPOLOW(3) = (1<<1);  // 가운데 LED
	if(led >= 3)
		*R_GPOLOW(3) = (1<<0);  // 오른쪽 LED
}
// P3.3, P3.4, P4.4
void ventilation_led_ctrl(U8 led){
	// All OFF first
	*R_GPOHIGH(3) = (1<<3);
	*R_GPOHIGH(3) = (1<<4);
	*R_GPOHIGH(4) = (1<<4);

	// 누적 점등: 왼쪽(P3.4)부터 오른쪽(P4.4)으로
	if(led >= 1)
		*R_GPOLOW(3) = (1<<4);  // P3.4 (왼쪽)
	if(led >= 2)
		*R_GPOLOW(3) = (1<<3);  // P3.3 (가운데)
	if(led >= 3)
		*R_GPOLOW(4) = (1<<4);  // P4.4 (오른쪽)
}

SURFACE* mass_image;
extern EGL_FONT* g_pFontbit;

// 교대부양 초기화
void levitate_value_power_init(void)
{
	int i, j;
	
	// debugprintf("\n\r ------- leviate init -----");
	for(i = BODY_HEAD; i < LEVIT_MAX; i++)// 일반/집중/수면
	{
		for(j = 0; j < BODY_MAX; j++)// 신체부위
		{
			levitate[i].body[j][BODY_ENABLE] = 0x10 * (j+1);// ((머리, 상체부, 하체부, 다리)Enable/Disable
			levitate[i].body[j][BODY_TIME + 1] = 0x30 ;	// 시간 (3단계)
			levitate[i].body[j][BODY_HIGH + 1] = 0x30 ;	// 높이 (3단계)
			levitate[i].body[j][BODY_RPM + 1] = 0x30 ;	// 속도 (3단계)
			// debugprintf("\n\r levitate[%d].body[%d] = 0x%x, 0x%x, 0x%x, 0x%x", 
				// i,j, levitate[i].body[j][BODY_ENABLE], levitate[i].body[j][BODY_TIME + 1],levitate[i].body[j][BODY_HIGH + 1], levitate[i].body[j][BODY_RPM + 1]);
		}
	}
}
// 체압분산 초기화
// body[i][0] = 부위 ID + Enable bit0 (ESP32: HeadOff=0x10, UpperBodyOn=0x21, ...)
// body[i][1] = pressure_level (0x10~0x60)
// body[i][2] = speed_level (0x10~0x60)
void dispersion_value_power_init(void)
{
	int i;
	// 머리는 고정 매트리스라 비활성, 나머지 활성
	U8 body_id[] = {0x10, 0x21, 0x31, 0x41};  // Head OFF, Upper ON, Lower ON, Leg ON

	for(i = 0; i < BODY_MAX; i++)
	{
		dispersion.body[i][0] = body_id[i];   // 부위 ID + Enable
		dispersion.body[i][1] = 0x30;          // pressure_level (3단계=100%)
		dispersion.body[i][2] = 0x30;          // speed_level (3단계=기존동작)
		dispersion.body[i][3] = 0x00;          // 예약 (ESP32에서 무시)
	}
}
// 마사지 초기화
void massage_value_power_init(void)
{
	int i,j;
	//debugprintf("\n\r ------- massage init -----");
	for(i = 0; i < MASSAGE_MAX; i++)// 일반/집중/수면
	{
		for(j = 0; j < BODY_MAX; j++)// 신체부위
		{
			massage[i].body[j][BODY_ENABLE] = 0x10 * (j+1);// (머리, 상체부, 하체부, 다리)Enable/Disable
			massage[i].body[j][BODY_TIME + 1] = 0x30 ;	// 시간 (3단계)
			massage[i].body[j][BODY_HIGH + 1] = 0x30 ;	// 높이 (3단계)
			massage[i].body[j][BODY_RPM + 1] = 0x30 ;	// 속도 (3단계)
			// debugprintf("\n\r massage[%d].body[%d] = 0x%x, 0x%x, 0x%x, 0x%x", 
				// i,j, massage[i].body[j][BODY_ENABLE], massage[i].body[j][BODY_TIME + 1],massage[i].body[j][BODY_HIGH + 1], massage[i].body[j][BODY_RPM + 1]);
		}
	}	
}
// 환자케어 초기화
void patient_care_value_power_init(void)
{
	memset(&patient_care, 0, sizeof(BODY));
}

void heat_value_power_init(void)
{
	memset(&heat, 0, sizeof(BODY));
}

void ventilation_value_power_init(void)
{
	memset(&ventilation, 0, sizeof(BODY));
}
//////////////////////////////////////////////////////////////////////////////////////////////////
// up key로 이동을 수행한다.
// 설정/저장 키를 눌러야만 저장 한다.
//////////////////////////////////////////////////////////////////////////////////////////////////
void up_key_proc(void)
{
	switch(cursor.mode)
	{
		case CURSOR_BODY: // 머리, 상체부, 하체부, 다리
			if(cursor.body) cursor.body--;
			else cursor.body = BODY_LEG;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
			
		case CURSOR_SET:// 화면 우측 설정값
			if(cursor.set) cursor.set--;
			else cursor.set = cursor.set_max;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// down key로 이동을 수행한다.
// 설정/저장 키를 눌러야만 저장 한다.
//////////////////////////////////////////////////////////////////////////////////////////////////
void down_key_proc(void)
{
	switch(cursor.mode)
	{
		case CURSOR_BODY :// 머리, 몸체 상, 몸체 하, 다리
			cursor.body++;
			if(cursor.body >= BODY_MAX)
				cursor.body = BODY_HEAD;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
			
		case CURSOR_SET://  화면 우측 설정값
				cursor.set++;
			if(cursor.set >= cursor.set_max)
				cursor.set = 0;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// 설정/저장 방향으로 이동을 수행한다.
// 설정/저장 키를 누르면 첫 번째 키를 눌러야 설정값 쪽으로 이동한다.
// 설정값 쪽으로 이동 후 값 감소로 된다.
//
//////////////////////////////////////////////////////////////////////////////////////////////////
void left_key_proc(bool left, BODY *sub_title)
{
	switch(cursor.mode)
	{
		case CURSOR_BODY :// Enable/Disable 토글
			temp_body.body[cursor.body][BODY_ENABLE] ^= 0x01;
			smart_bed_display.display_refresh = true;
			debugprintf("\n\rLEFT: TOGGLE [0x%x]", temp_body.body[cursor.body][BODY_ENABLE]);
			break;
			
		case CURSOR_SET:// 설정값 감소 (--)
			if(temp_body.body[cursor.body][cursor.set + 1] >= 0x20)
				temp_body.body[cursor.body][cursor.set + 1] -= 0x10;
			smart_bed_display.display_refresh = true;// lcd refresh
			break;
			
		default :
			if(left == true)
			{// 커서 상하 이동(메인 설정값 쪽)
				if(cursor.type)
				{
					cursor.type--;
				}
				else
				{
					cursor.type = cursor.type_max-1;
				}
				memcpy(&temp_body, &sub_title[cursor.type], sizeof(BODY));
				smart_bed_display.display_refresh = true;// lcd refresh
			}
			conform_key_run = 0;
	}
}

void left_key_proc1(void)
{
	if(cursor.type)
	{
		cursor.type--;
	}
	else
	{
		cursor.type = cursor.type_max-1;
		debugprintf("cur type[%d]\n\r", cursor.type);
	}
	smart_bed_display.display_refresh = true;// lcd refresh
	conform_key_run = 0;
}		

//////////////////////////////////////////////////////////////////////////////////////////////////
// 설정/저장 방향으로 이동을 수행한다.
// 설정/저장 키를 누르면 첫 번째 키를 눌러야 설정값 쪽으로 이동한다.
// 설정값 쪽으로 이동 후 값 증가로 된다.
// right옵션이 1인경우 상위 메뉴 커서를 좌우 이동 시킨다.
//////////////////////////////////////////////////////////////////////////////////////////////////
void right_key_proc(bool right, BODY *sub_title)
{
	switch(cursor.mode)
	{
		case CURSOR_BODY :// Enable/Disable 토글
			temp_body.body[cursor.body][BODY_ENABLE] ^= 0x01;
			smart_bed_display.display_refresh = true;
			debugprintf("\n\rRIGHT: TOGGLE [0x%x]", temp_body.body[cursor.body][BODY_ENABLE]);
			break;
			
		case CURSOR_SET:// 설정값 증가
			if(temp_body.body[cursor.body][cursor.set + 1] < body_set_max[cursor.set])
				temp_body.body[cursor.body][cursor.set + 1] += 0x10;
			smart_bed_display.display_refresh = true;// refresh
			break;
			
		default :// 일반, 집중, 수면
			if(right == true)
			{
				cursor.type++;
				if(cursor.type >= cursor.type_max)
					cursor.type = 0;
				smart_bed_display.display_refresh = true;// lcd refresh
				//memcpy(&temp_body, sub_title, sizeof(BODY));
				memcpy(&temp_body, &sub_title[cursor.type], sizeof(BODY));
			}
			conform_key_run = 0;
	}
}

void right_key_proc1(void)
{
	cursor.type++;
	if(cursor.type >= cursor.type_max)
		cursor.type = 0;
	debugprintf("cur type[%d]\n\r", cursor.type);
	smart_bed_display.display_refresh = true;// lcd refresh
	conform_key_run = 0;
}

// 설정 및 저장 키
// NORMAL -> 설정 -> 저장 -> NORMAL
// 환자케어는 저장 키를 사용하지 않으므로 set은 0부터
// 전체 부위를 한번에 저장하고 ESP32로 전송
void set_key_proc(U8 cmd_act, BODY *dest)
{
	int i, j;

	if(cursor.mode == CURSOR_MODE)
	{
		cursor.mode = CURSOR_BODY;
		cursor.body = 0;
		cursor.set = 0;
		smart_bed_display.display_refresh = true;// lcd refresh
	}
	else
	{
		cursor.mode = CURSOR_MODE;
		// 전체 부위 설정을 원본에 복사
		memcpy(dest, &temp_body, sizeof(BODY));
		// 전체 부위 ESP32 전송
		for(i = 0; i < BODY_MAX; i++)
		{
			debugprintf("\n\rbody[%d]: ", i);
			for(j = 0; j < BODY_LEVIT_MAX + 1; j++)
				debugprintf("0x%2x ", dest->body[i][j]);
			esp32_packet_send(CMD1_SEND_SET_VAL, cmd_act, dest->body[i], BODY_LEVIT_MAX + 1);
		}
		smart_bed_display.display_refresh = true;// lcd refresh
		conform_key_run = 0;
	}
}

//
// 확인 키(CONFORM KEY)
// 확인키는 두가지 MODE가 있다.
// 1. NORMAL MODE: START, PAUSE, RESTART
// 2. SET MODE : Enalbe/Disable용
// 
void conform_key_proc(U8 cmd_act)
{
	switch(cursor.mode)
	{
		case CURSOR_MODE:	// start/pause/restart
			if(conform_key_run == CMD3_START)
			{// 최초 시작
				// 마사지가 아닌 다른 모드 시작 시 running_massage_type 클리어
				if(cmd_act < CMD2_MASSAGE || cmd_act >= (CMD2_MASSAGE + MASSAGE_MAX))
					running_massage_type = -1;
				esp32_packet_send(CMD1_SEND_RUN_ST, cmd_act, &conform_key_run , 1);
				conform_key_run = CMD3_PAUSE;
			}
			else
			{
				if(conform_key_run == CMD3_PAUSE)
				{//일시정지
					esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_PAUSE, &conform_key_run, 1);
					conform_key_run = CMD3_RESTART;
				}
				else
				{// 재 시작
					esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_RESTART, &conform_key_run, 1);
					conform_key_run = CMD3_PAUSE;
				}
			}
			break;
			
		case CURSOR_BODY :// 부위 선택 → 값 조정 모드 진입
			cursor.mode = CURSOR_SET;
			cursor.set = 0;
			smart_bed_display.display_refresh = true;
			debugprintf("\n\rCONFORM: BODY -> SET");
			break;
		case CURSOR_SET :// 값 조정 → 부위 선택 모드 복귀
			cursor.mode = CURSOR_BODY;
			smart_bed_display.display_refresh = true;
			debugprintf("\n\rCONFORM: SET -> BODY");
			break;
	}
}

void conform_key_proc1(U8 cmd_act)
{
	U8 tmp = 0;
	running_massage_type = -1;	// 환자케어 시작 시 마사지 상태 클리어

	if(temp_body.body[0][0] != (cursor.type + 1))
	{
		temp_body.body[0][0] = cursor.type + 1;
		tmp = 1;
		esp32_packet_send(CMD1_SEND_RUN_ST, cmd_act, &tmp, 1);
	}
	else
	{
		temp_body.body[0][0] = 0;
		esp32_packet_send(CMD1_SEND_RUN_ST, cmd_act, &tmp, 1);
	}
	smart_bed_display.display_refresh = true;// lcd refresh 
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 체압분산
// KEY : up, down, left, right, 확인, 설정/저장, 초기화, 온열, 통풍
// 동작 중  체압분산 키를 다시누르면 동작을 종료하고 현재 값을 저장후 된다.
// pressure value(0~255) -> RGB color (blue->cyan->green->yellow->red)
U32 pressure_to_color(U8 value)
{
	if(value == 0)
		return MAKE_COLORREF(0, 0, 40);
	if(value < 64)
		return MAKE_COLORREF(0, value * 4, 255);
	if(value < 128)
		return MAKE_COLORREF(0, 255, 255 - (value - 64) * 4);
	if(value < 192)
		return MAKE_COLORREF((value - 128) * 4, 255, 0);
	return MAKE_COLORREF(255, 255 - (value - 192) * 4, 0);
}

// 체압분산 관련된 키 입력을 체크하는 루틴
// 체압분산 모드에서 체압분산 키를 다시 누르면은 홈화면으로 전환된다.
// 설정/저장 키를 누르지않은 상태에서는 UP/DOWN/LEFT/RIGHT를 누르지는 않는다.
// 신체를 선택후 UP/DOWN으로 선택하고 설정 변경은 LEFT/RIGHT로 한다.
// 선택확인은 확인 버튼으로 한다.
//
//////////////////////////////////////////////////////////////////////////////////////////////////
void dispersion_proc(void)
{
	if(smart_bed_display.status != MODE_VAIRANCE)
	{
		smart_bed_display.status = MODE_VAIRANCE;	// 교대부양 display 갱신
		smart_bed_display.display_refresh = true;// 체압분산 display
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = 2;// 커서표시 개수
		cursor.type_max = 0;
		memcpy(&temp_body, &dispersion, sizeof(BODY));// 현재 설정 임시 버퍼로 복사
		
		remocon_key.key_val = 0xFF;
		body_set_max[0] = 0x60;// LEVEL
		body_set_max[1] = 0x60;	// 몸체(동작속도)
		conform_key_run = 0;
		return;
	}
	
	switch(remocon_key.key_val){
		case VAIRANCE_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case UP_KEY:
			up_key_proc();
			remocon_key.key_val = 0xFF;
			break;
		case DOWN_KEY:
			down_key_proc();
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:		// 상체/하체 설정 변경
			left_key_proc(false, &dispersion);
			remocon_key.key_val = 0xFF;
			break;
		case RIGHT_KEY:	// 상체/하체 설정 변경
			right_key_proc(false, &dispersion);
			remocon_key.key_val = 0xFF;
			break;
			
		case CONFORM_KEY:// 확인(설정중에서는 확인키로 Enable/Disable
			conform_key_proc(CMD2_DISPERSION);

			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:
			set_key_proc(CMD2_DISPERSION, &dispersion);	// 체압분산
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
	}
}

void dispersion_draw(void){
	int i = 0, num = 0, digits = 0;
	char string[100] = {0, };

	set_draw_target(getbackframe());// back frame select
	draw_surface(btm1_img,0,0);	// 배경 이미지(의미 없음)
	draw_surface(vaira_title_imag, TITLE_X, TITLE_Y);	// title 이미지
	draw_surface(vaira_main_icon_imag, MAIN_ICON_X, MAIN_ICON_Y);	// 메인 아이콘

// 설정 모드 표시 바
	draw_mode_indicator();

// BAR draw (동작 중 매트리스 표시)
	for(i = 0; i < MAX_BAR; i++)
	{
		if( bar[i] )
			draw_surface(bar_img, BAR_X, (BAR_Y + BAR_I * i));
	}
// 인체 이미지 draw
	if(body_info == true)
		draw_surface(body_imag, BODY_X, BODY_Y);

// 7x10 pressure heatmap (임시 비활성)
#if 0
	{
		int row, col;
		for(row = 0; row < PRESSURE_ROWS; row++)
		{
			for(col = 0; col < PRESSURE_COLS; col++)
			{
				draw_rectfill(
					HEATMAP_X + col * (HEATMAP_CELL_W + HEATMAP_GAP),
					HEATMAP_Y + row * (HEATMAP_CELL_H + HEATMAP_GAP),
					HEATMAP_CELL_W,
					HEATMAP_CELL_H,
					pressure_to_color(pressure_map[row][col])
				);
			}
		}
	}
#endif

// 머리, 상체부, 하체부, 다리
	for(i = 0; i < BODY_MAX; i++)
	{
		if(cursor.mode != CURSOR_MODE)
		{// 설정 모드: 커서 선택 표시
			if(cursor.body == i)
				draw_surface(radio_avail_selt_img,radio_xy[i][0], radio_xy[i][1]);// 선택됨
			else
			{
				if(temp_body.body[i][0] & 0x01)
					draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
				else
					draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
			}
		}
		else
		{// 일반 모드: Enable/Disable 상태만 표시
			if(temp_body.body[i][0] & 0x01)
				draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
			else
				draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
		}
	}
// 선택 부위 하이라이트 테두리
	draw_body_highlight();

// 설정 값 표시 : 레벨, 동작속도
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	for(i = 0; i < cursor.set_max; i++)
	{
		if(cursor.mode == CURSOR_SET)
		{
			if(cursor.set == i)
				draw_surface(dip_en_num_img, num2_xy[i][0], num2_xy[i][1]);// Enable 활성
			else
				draw_surface(dip_ds_num_img, num2_xy[i][0], num2_xy[i][1]);// Disable 비활성
		}
		else
		{
			draw_surface(dip_ds_num_img, num2_xy[i ][0], num2_xy[i ][1]);// Disable 비활성
		}
		debugprintf("\n\r -> 0x%x", temp_body.body[cursor.body][i+1]);
		num = (temp_body.body[cursor.body][i+1]>>4);
		
		sprintf(string, "%d", num);
		// digits = count_digits_string(num);
		// bmpfont_draw(g_pFont40, num2_xy[i ][0]+(3-1)*10 , num2_xy[i ][1]+10, string);
		digits = count_digits_string(num);
		switch(digits)
		{
			case 1:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+ 22, num2_xy[i ][1]+11, string);
				break;
			case 2:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+13, num2_xy[i ][1]+11, string);
				break;
			default:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+(4-digits) *8, num2_xy[i ][1]+11, string);
		}
	}
	flip();
}

// ====================교대 부양 START=============================== //
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 교대 부양
// 1. LEFT, RIGHT 버튼으로 일반->집중->수면으로 이동 가능함
// 2. 교대부양 키를 다시 누르면 홈 화면으로 돌아간다.
// 3. 설정/저장으로 저장.
//
//////////////////////////////////////////////////////////////////////////////////////////////////
void levitate_proc(void)
{
	if(smart_bed_display.status != MODE_LEVITATE)
	{// 처음 교대부양 진입 할 경우
		smart_bed_display.status = MODE_LEVITATE;	// 교대부양 display 갱신
		smart_bed_display.display_refresh = true;// 교대부양 display
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = BODY_LEVIT_MAX;
		cursor.type_max = LEVIT_MAX;
		memcpy(&temp_body, &levitate[cursor.type], sizeof(BODY));// 현재 설정 임시 버퍼로 복사
		
		remocon_key.key_val = 0xFF;
		body_set_max[BODY_TIME] = 0xC0;// time
		body_set_max[BODY_HIGH] = 0x60;	// high
		body_set_max[BODY_RPM] = 0x30;// rpm
		conform_key_run = 0;
		return;
	}
	
	switch(remocon_key.key_val){
		case LEVITATE_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case UP_KEY:
			up_key_proc();
			remocon_key.key_val = 0xFF;
			break;
		case DOWN_KEY:
			down_key_proc();
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:		// 상체/하체 설정 감소수
			left_key_proc(true, levitate);
			remocon_key.key_val = 0xFF;
			break;
		case RIGHT_KEY:	// 상체/하체 설정 증가
			right_key_proc(true, levitate);
			remocon_key.key_val = 0xFF;
			break;
		case CONFORM_KEY:// 확인(설정중에서는 확인키로 Enable/Disable
			if(cursor.type == 0)
				conform_key_proc(CMD2_VENTIL_NORMAL);	// 일반
			else if(cursor.type == 1)
				conform_key_proc(CMD2_VENTIL_FOCUR);	// 집중
			else if(cursor.type == 2)
				conform_key_proc(CMD2_VENTIL_SLEEP);	// 수면
			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:
			if(cursor.type == 0)
				set_key_proc(CMD2_VENTIL_NORMAL, &levitate[cursor.type]);	// 일반
			else if(cursor.type == 1)
				set_key_proc(CMD2_VENTIL_FOCUR, &levitate[cursor.type]);	// 집중
			else if(cursor.type == 2)
				set_key_proc(CMD2_VENTIL_SLEEP, &levitate[cursor.type]);	// 수면
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
	}
}





void levitate_draw(void){
	int i = 0, num = 0, digits = 0;
	char string[200] = {0, };
	
	set_draw_target(getbackframe());// back frame select
	draw_surface(btm1_img,0,0);	// 배경 이미지(의미 없음)
	draw_surface(levitate_title_img, TITLE_X, TITLE_Y);	// title 이미지
	draw_surface(levitate_main_icon_img, MAIN_ICON_X, MAIN_ICON_Y);	// 메인 아이콘

// 일반, 집중, 수면
	switch(cursor.type){
		case LEVIT_NOR:
			draw_surface(levitate_normal_sel_icon_img, LEVIT_NOR_X, LEVIT_SUB_Y);	// 일반
			draw_surface(levitate_concent_icon_img, LEVIT_CON_X, LEVIT_SUB_Y);	// 집중
			draw_surface(levitate_sleep_icon_img, LEVIT_SLE_X, LEVIT_SUB_Y);	// 수면
			break;
		case LEVIT_FOCUS:
			draw_surface(levitate_normal_icon_img, LEVIT_NOR_X, LEVIT_SUB_Y);	// 일반
			draw_surface(levitate_concent_sel_icon_img, LEVIT_CON_X, LEVIT_SUB_Y);	// 집중
			draw_surface(levitate_sleep_icon_img, LEVIT_SLE_X, LEVIT_SUB_Y);	// 수면
			break;
		case LEVIT_SLEEP:
			draw_surface(levitate_normal_icon_img, LEVIT_NOR_X, LEVIT_SUB_Y);	// 일반
			draw_surface(levitate_concent_icon_img, LEVIT_CON_X, LEVIT_SUB_Y);	// 집중
			draw_surface(levitate_sleep_sel_icon_img, LEVIT_SLE_X, LEVIT_SUB_Y);	// 수면
	}

// 설정 모드 표시 바
	draw_mode_indicator();

// BAR draw
	for(i = 0; i < MAX_BAR; i++)
	{
		if( bar[i] )
			draw_surface(bar_img, BAR_X, (BAR_Y + BAR_I * i));
	}
// 인체 이미지 draw
	if(body_info == true)
		draw_surface(body_imag, BODY_X, BODY_Y);	// body image

// 머리, 상체부, 하체부, 다리
	for(i = 0; i < BODY_MAX; i++)
	{
		if(cursor.mode != CURSOR_MODE)
		{// 설정 모드: 커서 선택 표시
			if(cursor.body == i)
				draw_surface(radio_avail_selt_img,radio_xy[i][0], radio_xy[i][1]);// 선택됨
			else
			{
				if(temp_body.body[i][0] & 0x01)
					draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
				else
					draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
			}
		}
		else
		{// 일반 모드: Enable/Disable 상태만 표시
			if(temp_body.body[i][0] & 0x01)
				draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
			else
				draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
		}
	}
// 선택 부위 하이라이트 테두리
	draw_body_highlight();

// 설정 값 표시 : 시간설정, 높이, 속도
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	for(i = 0; i < cursor.set_max; i++)
	{
		if(cursor.mode == CURSOR_SET)
		{
			if(cursor.set == i)
				draw_surface(dip_en_num_img, num3_xy[i][0], num3_xy[i][1]);// Enable 활성
			else
				draw_surface(dip_ds_num_img, num3_xy[i][0], num3_xy[i][1]);// Disable 비활성
		}
		else
		{
			draw_surface(dip_ds_num_img, num3_xy[i ][0], num3_xy[i ][1]);// Disable 비활성
		}
		debugprintf("\n\r -> 0x%x", temp_body.body[cursor.body][i+1]);

		if(i == 0) num = (temp_body.body[cursor.body][i+1]>>4) * 10;
		else
		{
			if(i == 1)
			{
				num = (temp_body.body[cursor.body][i+1]>>4);
			}
			else
			{
				num = (temp_body.body[cursor.body][i+1]>>4);
			}
		}
		sprintf(string, "%d", num);
		digits = count_digits_string(num);
		switch(digits)
		{
			case 1:
				bmpfont_draw(g_pFont28, num3_xy[i ][0]+ 22, num3_xy[i ][1]+11, string);
				break;
			case 2:
				bmpfont_draw(g_pFont28, num3_xy[i ][0]+13, num3_xy[i ][1]+11, string);
				break;
			default:
				bmpfont_draw(g_pFont28, num3_xy[i ][0]+(4-digits) *8, num3_xy[i ][1]+11, string);
		}
	}
	flip();
}
// =====================교대 부양 END ============================== //

// ====================== 마사지 Start ===============================//
void massage_proc(void){
	if(smart_bed_display.status != MODE_MASSAGE)
	{
		smart_bed_display.status = MODE_MASSAGE;	// 마사지 display 갱신
		smart_bed_display.display_refresh = true;// 마사지 display
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = 2;
	    cursor.type_max = MASSAGE_MAX;

		// 동작 중인 마사지가 있으면 해당 마사지로 커서 이동 및 동작 상태 복원
		if(running_massage_type >= 0 && running_massage_type < MASSAGE_MAX){
			cursor.type = running_massage_type;
			conform_key_run = CMD3_PAUSE;
		} else {
			conform_key_run = 0;
		}

		memcpy(&temp_body, &massage[cursor.type], sizeof(BODY));// 현재 설정 임시 버퍼로 복사

		remocon_key.key_val = 0xFF;
		body_set_max[0] = 0xC0;// time
		body_set_max[1] = 0x60;	// high
		return;
	}
	
	switch(remocon_key.key_val){
		case MASSA_KEY:// 체압 분산, 마사지 키를 다시누르지 않는다.
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case UP_KEY:
			up_key_proc();
			remocon_key.key_val = 0xFF;
			break;
		case DOWN_KEY:
			down_key_proc();
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:		// 상체/하체 설정 변경
			left_key_proc(true, &massage[cursor.type]);
			remocon_key.key_val = 0xFF;
			break;
		case RIGHT_KEY:	// 상체/하체 설정 변경
			right_key_proc(true, &massage[cursor.type]);
			remocon_key.key_val = 0xFF;
			break;
			
		case CONFORM_KEY:// 확인(설정중에서는 확인키로 Enable/Disable
			conform_key_proc(CMD2_MASSAGE + cursor.type);
			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:
			set_key_proc(CMD2_MASSAGE + cursor.type, &massage[cursor.type]);	
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
	}
}



void massage_draw(void){
	int i = 0, num = 0, digits = 0;
	char string[100] = {0, };
	
	set_draw_target(getbackframe());// back frame select
	draw_surface(btm1_img,0,0);	// 배경 이미지(의미 없음)
	draw_surface(massage_title_img, TITLE_X, TITLE_Y);	// title 이미지
	draw_surface(massage_main_icon_img, MAIN_ICON_X, MAIN_ICON_Y);	// 메인 아이콘

	// 마사지 선택 이미지 (가운데 정렬 180x60)
	draw_surface(massage_sub_icon_img[cursor.type], MASSAGE_SUB_X, MASSAGE_SUB_Y);
	// 좌우 선택 화살표 (40x40, 세로 중앙 정렬)
	if(left_select_img)
		draw_surface(left_select_img, 25, MASSAGE_SUB_Y + 10);
	if(right_select_img)
		draw_surface(right_select_img, 255, MASSAGE_SUB_Y + 10);

// 설정 모드 표시 바
	draw_mode_indicator();

// BAR draw
	for(i = 0; i < MAX_BAR; i++)
	{
		if( bar[i] )
			draw_surface(bar_img, BAR_X, (BAR_Y + BAR_I * i));
	}
// 인체 이미지 draw
	if(body_info == true)
		draw_surface(body_imag, BODY_X, BODY_Y);	// body image

// 머리, 상체부, 하체부, 다리
	for(i = 0; i < BODY_MAX; i++)
	{
		if(cursor.mode != CURSOR_MODE)
		{// 설정 모드: 커서 선택 표시
			if(cursor.body == i)
				draw_surface(radio_avail_selt_img,radio_xy[i][0], radio_xy[i][1]);// 선택됨
			else
			{
				if(temp_body.body[i][0] & 0x01)
					draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
				else
					draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
			}
		}
		else
		{// 일반 모드: Enable/Disable 상태만 표시
			if(temp_body.body[i][0] & 0x01)
				draw_surface(radio_avail_img,radio_xy[i][0], radio_xy[i][1]);// Enable
			else
				draw_surface(radio_dis_selt_img,radio_xy[i][0], radio_xy[i][1]);// Disable
		}
	}
// 선택 부위 하이라이트 테두리
	draw_body_highlight();

// 설정 값 표시 : 시간설정, 높이, 속도
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	for(i = 0; i < cursor.set_max; i++)
	{
		if(cursor.mode == CURSOR_SET)
		{
			if(cursor.set == i)
				draw_surface(dip_en_num_img, num2_xy[i][0], num2_xy[i][1]);// Enable 활성
			else
				draw_surface(dip_ds_num_img, num2_xy[i][0], num2_xy[i][1]);// Disable 비활성
		}
		else
		{
			draw_surface(dip_ds_num_img, num2_xy[i ][0], num2_xy[i ][1]);// Disable 비활성
		}
		debugprintf("\n\r -> 0x%x", temp_body.body[cursor.body][i+1]);
		if(i == 0) num = (temp_body.body[cursor.body][i+1]>>4) * 10;
		else
		{
			num = (temp_body.body[cursor.body][i+1]>>4);
			num *= 30;
			if(num > 160) num = 170;
		}
		
		sprintf(string, "%d", num);
		// bmpfont_draw(g_pFont28, num2_xy[i ][0]+15, num2_xy[i ][1]-3, string);
		digits = count_digits_string(num);
		switch(digits)
		{
			case 1:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+ 22, num2_xy[i ][1]+11, string);
				break;
			case 2:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+13, num2_xy[i ][1]+11, string);
				break;
			default:
				bmpfont_draw(g_pFont28, num2_xy[i ][0]+(4-digits) *8, num2_xy[i ][1]+11, string);
		}
	}
	flip();
}
// ====================== 마사지 End ================================//


// ====================== 환자 케어 Start ============================//
//////////////////////////////////////////////////////////////////////////////////////////////////
// 환자 케어
// 머리감기, 배변, 이동
//////////////////////////////////////////////////////////////////////////////////////////////////
bool move_flag = false;
bool left_key = false;
bool right_key = false;
bool running_flag = false;
void patient_care_proc(void)
{
	if(smart_bed_display.status != MODE_PATIENT_CARE)
	{
		smart_bed_display.status = MODE_PATIENT_CARE;
		smart_bed_display.display_refresh = true;
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = 0;
		cursor.type_max = 2;	// 머리감기, 배변 (이동 제거)
		memcpy(&temp_body, &patient_care, sizeof(BODY));

		remocon_key.key_val = 0xFF;
		conform_key_run = 0;
		running_flag = false;
		return;
	}

	switch(remocon_key.key_val){
		case CARE_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case UP_KEY:
			remocon_key.key_val = 0xFF;
			break;
		case DOWN_KEY:
			if(running_flag && cursor.type == PATIENT_DEFEC)
			{	// 배변 모드 실행 중: 틸트 원위치 명령 전송
				U8 tmp = 0;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_MOVE_CENTER, &tmp, 1);
				smart_bed_display.display_refresh = true;
			}
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:
			if(running_flag && cursor.type == PATIENT_DEFEC)
			{	// 배변 모드 실행 중: 좌 틸팅 명령 전송
				U8 tmp = 0;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_MOVE_LEFT, &tmp, 1);
				smart_bed_display.display_refresh = true;
			}
			else
			{
				left_key_proc(true, patient_care);
				running_flag = false;
			}
			remocon_key.key_val = 0xFF;
			break;
		case RIGHT_KEY:
			if(running_flag && cursor.type == PATIENT_DEFEC)
			{	// 배변 모드 실행 중: 우 틸팅 명령 전송
				U8 tmp = 0;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_MOVE_RIGHT, &tmp, 1);
				smart_bed_display.display_refresh = true;
			}
			else
			{
				right_key_proc(true, patient_care);
				running_flag = false;
			}
			remocon_key.key_val = 0xFF;
			break;
		case CONFORM_KEY:// 확인 (토글: 시작↔정지)
			if(running_flag)
			{
				running_flag = false;
				U8 tmp = 0;
				esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_PAUSE, &tmp, 0);
			}
			else
			{
				running_flag = true;
				conform_key_proc(CMD2_HEAR + cursor.type);
			}
			smart_bed_display.display_refresh = true;
			remocon_key.key_val = 0xFF;
			break;
		case SET_KEY:
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
			break;
	}
}

// 환자케어: 머리감기/배변 2버튼 중앙 배치 (이동 제거)
#define CARE_BTN1_X	50		// 머리감기 버튼 X
#define CARE_BTN2_X	180		// 배변 버튼 X

void patient_care_draw(void)
{
	set_draw_target(getbackframe());
	draw_surface(btm2_img, 0, 0);
	draw_surface(patient_title_img, TITLE_X, TITLE_Y);
	draw_surface(patient_main_icon_img, MAIN_ICON_X, MAIN_ICON_Y);

	// 머리감기 버튼
	if(cursor.type == PATIENT_HEAD)
	{
		if(running_flag)
			draw_surface(patient_head_sel_img, CARE_BTN1_X, LEVIT_SUB_Y);
		else
		{
			draw_surface(patient_head_img, CARE_BTN1_X, LEVIT_SUB_Y);
			draw_roundrect(CARE_BTN1_X, LEVIT_SUB_Y, patient_head_sel_img->w, patient_head_sel_img->h, 10, MAKE_COLORREF(255,255,255));
		}
	}
	else
		draw_surface(patient_head_img, CARE_BTN1_X, LEVIT_SUB_Y);

	// 배변 버튼
	if(cursor.type == PATIENT_DEFEC)
	{
		if(running_flag)
			draw_surface(patient_defec_sel_img, CARE_BTN2_X, LEVIT_SUB_Y);
		else
		{
			draw_surface(patient_defec_img, CARE_BTN2_X, LEVIT_SUB_Y);
			draw_roundrect(CARE_BTN2_X, LEVIT_SUB_Y, patient_defec_img->w, patient_defec_img->h, 10, MAKE_COLORREF(255,255,255));
		}
	}
	else
		draw_surface(patient_defec_img, CARE_BTN2_X, LEVIT_SUB_Y);

	// 메인 이미지
	if(cursor.type == PATIENT_HEAD)
		draw_surface(patient_head_main_img, PATIENT_MAIN_IMG_X, PATIENT_MAIN_IMG_Y);
	else
		draw_surface(patient_shift_main_img, PATIENT_MAIN_IMG_X, PATIENT_MAIN_IMG_Y);

	// 배변 모드 실행 중: 좌/우 틸팅 화살표 + 중앙 평탄화 버튼
	if(running_flag && cursor.type == PATIENT_DEFEC)
	{
		draw_surface(left_arrow_img, 40, 380);
		draw_surface(right_arrow_img, 213, 380);

		// 중앙 평탄화 버튼 (▼ 버튼으로 동작, 화살표보다 1/3 아래)
		draw_roundrectfill(120, 393, 80, 40, 8, MAKE_COLORREF(0, 100, 150));
		draw_roundrect(120, 393, 80, 40, 8, MAKE_COLORREF(255, 255, 255));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255, 255, 255));
		draw_text_kr(g_pFontKor16, 136, 405, "평탄화");
	}

	flip();
}
// ====================== 환자 케어 End =============================//

// ====================== 온열 START =============================//

void heat_proc(void)
{
	U8 tmp = 0;
	
	if(smart_bed_display.status != MODE_HEAT)
	{
		smart_bed_display.status = MODE_HEAT;
		smart_bed_display.display_refresh = true;
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.type = heat.body[0][0];
		cursor.set_max = 0;
	    cursor.type_max = LEVIT_MAX+1;
		memcpy(&temp_body, &heat, sizeof(BODY));
		remocon_key.key_val = 0xFF;
		conform_key_run = 0;
		return;
	}
	
	switch(remocon_key.key_val){
		case HEAT_KEY:
			cursor.type++;
			if(cursor.type >= cursor.type_max)
					cursor.type = 0;// all off
			smart_bed_display.display_refresh = true;// lcd refresh
			temp_body.body[0][0] =  cursor.type;
			memcpy(&heat, &temp_body, sizeof(BODY));
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_HEAT + cursor.type, &tmp , 0);
			remocon_key.key_val = 0xFF;
			heat_led_ctrl(cursor.type);
			//break;
		default:
			remocon_key.key_val = 0xFF;
			break;

	}
}

void heat_draw(void)
{
	int i = 0;
	
	set_draw_target(getbackframe());// back frame select
	draw_surface(btm2_img,0,0);	
	draw_surface(heat_titile_img, TITLE_X, TITLE_Y);	
	draw_surface(heat_mian_icon_img, MAIN_ICON_X, MAIN_ICON_Y);	

	for(i = 0; i < 4; i++)
	{
		// if(cursor.type == (i-1))
			// draw_roundrectfill(50 + (i - 1) * 100-5, 162-5,   30,    30,		60, MAKE_COLORREF(0,0,255));// outer line
			//draw_circle(50 + i * 100 +10 , 162+10,		11, MAKE_COLORREF(0,0,255));// outer line
		if(temp_body.body[0][0] == (i + 1))
			draw_surface(heat_on_img, 50 + i * 100, 162);
		else
			draw_surface(heat_off_img, 50 + i * 100, 162);
		
	}
	flip();

}
// ====================== 온열 END =============================//

// ====================== 통풍 START =============================//
void ventilation_proc(void)
{
	U8 tmp = 0;
	
	if(smart_bed_display.status != MODE_VENTILATION)
	{
		smart_bed_display.status = MODE_VENTILATION;
		smart_bed_display.display_refresh = true;
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.type = ventilation.body[0][0];
		cursor.set_max = 0;
	    cursor.type_max = LEVIT_MAX+1;
		memcpy(&temp_body, &ventilation, sizeof(BODY));
		remocon_key.key_val = 0xFF;
		conform_key_run = 0;
		return;
	}
	switch(remocon_key.key_val){
		case VENTIL_KEY:
			cursor.type++;
			if(cursor.type >= cursor.type_max)
					cursor.type = 0;// all off
			smart_bed_display.display_refresh = true;// lcd refresh
			temp_body.body[0][0] =  cursor.type;
			memcpy(&ventilation, &temp_body, sizeof(BODY));
			esp32_packet_send(CMD1_SEND_RUN_ST, CMD2_VENTIL + cursor.type, &tmp , 0);
			ventilation_led_ctrl(cursor.type);
			remocon_key.key_val = 0xFF;
			break;
		default:
			remocon_key.key_val = 0xFF;
			break;
	}
}

void ventilation_draw(void)
{
	int i = 0;

	set_draw_target(getbackframe());
	draw_surface(btm2_img,0,0);
	draw_surface(ventil_titile_img, TITLE_X, TITLE_Y);
	draw_surface(ventil_mian_icon_img, MAIN_ICON_X, MAIN_ICON_Y);

	for(i = 0; i<4; i++)
	{
		// if(cursor.type == i)
			// draw_roundrectfill(50 + i * 100-5, 162-5,   30,    30,		60, MAKE_COLORREF(0,0,255));// outer line
			//draw_circle(50 + i * 100 +10 , 162+10,		11, MAKE_COLORREF(0,0,255));// outer line
		if(temp_body.body[0][0] == (i + 1))
			draw_surface(ventil_on_img, 50 + i * 100, 162);
		else
			draw_surface(ventil_off_img, 50 + i * 100, 162);
	}
	flip();
}
/////////////////////////////// 통풍 END //////////////////////////////
BODY initial;
void initial_proc(void)
{
	if(smart_bed_display.status != MODE_INITIAL)
	{
		smart_bed_display.status = MODE_INITIAL;	// 
		smart_bed_display.display_refresh = true;//
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.set_max = 0;
	    cursor.type_max = 0;
		memcpy(&temp_body, &initial, sizeof(BODY));
		
		remocon_key.key_val = 0xFF;
		//body_set_max[0] = 0xC0;// time
	//	body_set_max[1] = 0x60;	// high
		conform_key_run = 0;
		return;
	}
	
	switch(remocon_key.key_val){
		case INIT_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;

			
		case CONFORM_KEY:
			conform_key_proc1(CMD2_INIT + cursor.type);
			levitate_value_power_init();
			dispersion_value_power_init();
			massage_value_power_init();
			patient_care_value_power_init();
			heat_value_power_init();
			ventilation_value_power_init();
			initial.body[0][0] = true;
			//memcpy(&initial, &temp_body, sizeof(BODY));
			//remocon_key.key_val = 0xFF;
		//	smart_bed_status.status = MODE_HOME;
			smart_bed_display.display_refresh = true;// 
			//break;

		default:
			remocon_key.key_val = 0xFF;
	}
}

void initial_draw(void)
{
	set_draw_target(getbackframe());

	// 배경: 진한 네이비
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(15, 20, 35));

	// 상단 장식 라인
	if(initial.body[0][0] == true)
		draw_rectfill(0, 0, 320, 4, MAKE_COLORREF(0, 180, 80));
	else
		draw_rectfill(0, 0, 320, 4, MAKE_COLORREF(0, 150, 200));

	// 타이틀 바 (둥근 사각형)
	if(initial.body[0][0] == true)
		draw_roundrectfill(30, 25, 260, 45, 10, MAKE_COLORREF(0, 140, 65));
	else
		draw_roundrectfill(30, 25, 260, 45, 10, MAKE_COLORREF(0, 85, 135));

	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
	draw_text_kr(g_pFontKor, 118, 34, "초기화");

	// 구분선
	draw_rectfill(50, 90, 220, 1, MAKE_COLORREF(50, 60, 80));

	if(initial.body[0][0] == true)
	{
		// 성공 원형 아이콘 (이중 원)
		draw_circlefill(160, 185, 45, MAKE_COLORREF(0, 160, 70));
		draw_circlefill(160, 185, 38, MAKE_COLORREF(0, 130, 55));

		// OK 텍스트
		egl_font_set_color(g_pFont28, MAKE_COLORREF(255, 255, 255));
		bmpfont_draw(g_pFont28, 143, 173, "OK");

		// 완료 메시지
		egl_font_set_color(g_pFontKor, MAKE_COLORREF(0, 220, 100));
		draw_text_kr(g_pFontKor, 55, 275, "시스템 초기화 완료");

		// 하단 안내
		draw_rectfill(50, 320, 220, 1, MAKE_COLORREF(50, 60, 80));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(120, 130, 150));
		draw_text_kr(g_pFontKor16, 40, 340, "초기화 버튼을 눌러 돌아갑니다");
	}
	else
	{
		// 경고 원형 아이콘 (이중 원)
		draw_circlefill(160, 185, 45, MAKE_COLORREF(220, 70, 30));
		draw_circlefill(160, 185, 38, MAKE_COLORREF(190, 55, 20));

		// ! 텍스트
		egl_font_set_color(g_pFont28, MAKE_COLORREF(255, 255, 255));
		bmpfont_draw(g_pFont28, 155, 173, "!");

		// 안내 메시지
		egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 255, 255));
		draw_text_kr(g_pFontKor, 35, 275, "시스템을 초기화하려면");

		egl_font_set_color(g_pFontKor, MAKE_COLORREF(255, 80, 60));
		draw_text_kr(g_pFontKor, 25, 315, "확인 버튼을 누르세요");

		// 하단 주의 문구
		draw_rectfill(50, 375, 220, 1, MAKE_COLORREF(50, 60, 80));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(120, 130, 150));
		draw_text_kr(g_pFontKor16, 48, 395, "모든 설정이 초기화됩니다");
	}

	flip();
}


// 
// BAR의 On/Off 상태를 읽어 온다.
// SEND : FF81 '9''B''1''C' 31 00 00 xxxx 
// ACK   : FF81 '9''B''1''C' 31 FF 10 on off ~~~ xx xx
//
/*
bool get_cmd_send = false;
void get_bar_run_info(void)
{
	U8 tmp;
	U8 buff[20]={0,};
	int i;

	if(get_cmd_send == false)
	{// command send
		get_cmd_send = true;
		esp32_packet_send(CMD1_GET_BAR_INFO, 0x10, &tmp, 0);
	}
	else
	{// ack receive
		//get_cmd_send = false;
	//esp32_packet_receive(CMD1_GET_BAR_INFO,  0x10, 12+LENGTH + 1+2, buff) ;
		if(esp32_packet_receive(CMD1_GET_BAR_INFO,  0x10, 12+LENGTH + 1+2, buff)  == true)
		{
			debugprintf("\n\r");
			memcpy(&bar, buff, 12);
			for(i = 0; i< 12; i++)
				debugprintf("%x ", bar[i]);
			get_cmd_send = false;
		}
		//get_cmd_send = false;
		//else PRINTLINE;
	}
}
*/

////////////////////// Self test /////////////////////
	
//
// self test
//
// void self_test_lcd(void)
// {
	// int i = 0;
	// set_draw_target(getbackframe());
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(255,0, 0));
	// delayms(500);
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,255,0));
	// delayms(500);
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,255));
	// delayms(500);
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	// delayms(500);
	
	
// }


// ====================== 매뉴얼설정 & 자가진단 Start ================== //
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 자가진단은 설정/저장을 누르면 현재 값 Home mode로 돌아간다.
// 매뉴얼 <-> 자가진단
//
//	 REMOCON LED TEST 6개의 LED를 ON/OFF
//	AUDIO 
//	FAN
//	HEAT
//	ACTUATOR
//	BAR SENSOR
//	MOTOR
//	RPM
//	MOTOR BOARD LED
//////////////////////////////////////////////////////////////////////////////////////////////////
void down_key_self_test(CURSOR *cur)
{
	cur->type++;
	if(cur->type >= cur->type_max)
		cur->type = 0;
	debugprintf("cur type[%d]\n\r", cur->type);
	smart_bed_display.display_refresh = true;// lcd refresh
}		

void up_key_self_test(CURSOR *cur)
{
	if(cur->type)
	{
		cur->type--;
	}
	else
	{
		cur->type = cur->type_max-1;
		debugprintf("cur type[%d]\n\r", cur->type);
	}
	smart_bed_display.display_refresh = true;// lcd refresh
}
void remocon_led_test(void)
{
	heat_led_ctrl(1);
	delayms(500);
	heat_led_ctrl(2);
	delayms(500);
	heat_led_ctrl(4);
	delayms(500);
	heat_led_ctrl(7);
	delayms(500);
	
	ventilation_led_ctrl(1);
	delayms(500);
	ventilation_led_ctrl(2);
	delayms(500);
	ventilation_led_ctrl(4);
	delayms(500);
	ventilation_led_ctrl(7);
	delayms(500);

}


// #define KEY_TITLE_X 30
// #define KEY_TITLE_Y 3
// #define KEY_TITLE_W	250
// #define KEY_TITLE_H	40
// #define KEY_TITLE_TXT_X	(KEY_TITLE_X + 15)
// #define KEY_TITLE_TXT_Y	(KEY_TITLE_Y + 5)

// #define SUB_TITLE_X 10
// #define SUB_TITLE_Y 50
// #define SUB_TITLE_W	139
// #define SUB_TITLE_H	30
// #define SUB_TITLE_TXT_X	(SUB_TITLE_X + 15)
// #define SUB_TITLE_TXT_Y	(SUB_TITLE_Y + 8)

// void self_test_key(void)
// {
	// int i = 0;
	// U8 string[100] = {0, };
	
	// set_draw_target(getbackframe());// back frame select
	// draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,0));
	// draw_rect(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	// draw_rect(1, 1, 320-2, 480-2, MAKE_COLORREF(255,255,255));
	
	// draw_roundrectfill(KEY_TITLE_X, KEY_TITLE_Y, KEY_TITLE_W, KEY_TITLE_H, 10, MAKE_COLORREF(0,100,150));
	// egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	// sprintf(string, "--> KEY TEST  <--");
	// bmpfont_draw(g_pFont28, KEY_TITLE_TXT_X, KEY_TITLE_TXT_Y, string);
	// flip();
	// while(1)
	// {
		// key_proc();
		
		// if(i == 14)
			// break;
	// }
// }

// --> BAR TEST <--
#define BAR_TITLE_X 30
#define BAR_TITLE_Y 3
#define BAR_TITLE_W	250
#define BAR_TITLE_H	40
#define BAR_TITLE_TXT_X	(BAR_TITLE_X + 20)
#define BAR_TITLE_TXT_Y	(BAR_TITLE_Y + 5)

// -----> BAR n <----
#define BAR_SUB_BOOX_X 35
#define BAR_SUB_BOX_Y 53
#define BAR_SUB_BOX_W	139
#define BAR_SUB_BOX_H	30
#define BAR_SUB_TXT_X	(BAR_SUB_BOOX_X + 0)
#define BAR_SUB_TXT_Y	(BAR_SUB_BOX_Y + 0)

// adc value
#define BAR_TXT_BOX_X	BAR_SUB_BOOX_X
#define BAR_TXT_BOX_Y	(BAR_SUB_BOX_Y + BAR_SUB_BOX_H)
#define BAR_TXT_BOX_W	250
#define BAR_TXT_BOX_H	20
#define BAR_TXT_X	(BAR_TXT_BOX_X + 0)
#define BAR_TXT_Y	(BAR_TXT_BOX_Y + 0)


typedef struct _self_test
{
	U16 bar[10];
	U8 send_step;
	bool time_out;
	//U8 display_page;
	
}SELF_TEST_ST;
SELF_TEST_ST self_test_val[16];

// 
// bar의 adc값을 읽어온다.
// bar에는 10개의 load cell이 있고
// bar는 총 16개가 있다.
// display에 adc값을 hex로 표시한다.
// 1화면에 3개의 bar 정보를 표시한다
// exit는 설정 키로 한다.
//

enum _test_mode{
	BAR_TEST_1_3 = 0,
	BAR_TEST_4_6,
	BAR_TEST_7_9,
	BAR_TEST_10_12,
	BAR_TEST_13_15,
	BAR_TEST_16,
	
	MOTOR_TEST,
	RPM_TEST,
	LED_TEST,
	TEST_MAX
};

//
// 1~3,4~6,7~9,10~12,14~15, 16

//


void self_test_bar_draw(U8 bar_line, int bar_cnt, bool ready)
{
	int i = 0;
	U8 string[200] = {0, };
	memset(&self_test_val[bar_line],0,sizeof(SELF_TEST_ST));
	set_draw_target(getbackframe());// back frame select
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,0));
	draw_rect(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	draw_rect(1, 1, 320-2, 480-2, MAKE_COLORREF(255,255,255));
	
	draw_roundrectfill(BAR_TITLE_X, BAR_TITLE_Y, BAR_TITLE_W, BAR_TITLE_H, 10, MAKE_COLORREF(0,100,150));
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
	sprintf(string, "--> BAR TEST  <--");
	bmpfont_draw(g_pFont28, BAR_TITLE_TXT_X, BAR_TITLE_TXT_Y, string);
	
	//draw_roundrectfill(SUB_TITLE_X, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255, 255, 255));
	for(i=0; i<bar_cnt; i++){
		//draw_rectfill(BAR_TITLE_X, BAR_TITLE_Y, BAR_TITLE_W, BAR_TITLE_H, MAKE_COLORREF(255,255,255));
		egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,255));
		sprintf(string, "------> BAR %d <------", bar_line+ i + 1);
		bmpfont_draw(g_pFont28, BAR_SUB_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100), string);
		
		sprintf(string, "SEN1,  SEN2,  SEN3,  SEN4,  SEN5");
		bmpfont_draw(g_pFont16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) +BAR_SUB_BOX_H , string);
		
		sprintf(string, "SEN6,  SEN7,  SEN8,  SEN9,  SEN10");
		bmpfont_draw(g_pFont16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H*2 , string);
			
		if(ready & (1<<i))
		{
			memset(string, 0, sizeof(string));
			//sprintf(string, "%04x, %04x, %04x, %04x, %04x", self_test_val.bar[0][0], self_test_val.bar[0][1], self_test_val.bar[0][2], self_test_val.bar[0][3], self_test_val.bar[0][4]);
			sprintf(string, "%04x, %04x, %04x, %04x, %04x", self_test_val[bar_line+i].bar[0], self_test_val[bar_line+i].bar[1], self_test_val[bar_line+i].bar[2], self_test_val[bar_line+i].bar[3], self_test_val[bar_line+i].bar[4]);
			bmpfont_draw(g_pFont16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H, string);
		
			
			memset(string, 0, sizeof(string));
			//sprintf(string, "%04x, %04x, %04x, %04x, %04x", self_test_val.bar[0][0], self_test_val.bar[0][1], self_test_val.bar[0][2], self_test_val.bar[0][3], self_test_val.bar[0][4]);
			sprintf(string, "%04x, %04x, %04x, %04x, %04x", self_test_val[bar_line+i].bar[5], self_test_val[bar_line+i].bar[6], self_test_val[bar_line+i].bar[7], self_test_val[bar_line+i].bar[8], self_test_val[bar_line+i].bar[9]);
			bmpfont_draw(g_pFont16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H*3, string);
		}
		else
		{
			egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
			draw_text_kr(g_pFontKor16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H, "대기");
			draw_text_kr(g_pFontKor16, BAR_TXT_X, BAR_SUB_TXT_Y + i*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H*3, "대기");
		}
	}
	egl_font_set_color(g_pFont28, MAKE_COLORREF(255,255,100));
	//sprintf(string, "READ PAGE %d/6", self_test_val.display_page+1);
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255,255,100));
	draw_text_kr(g_pFontKor, BAR_TXT_X+20, BAR_SUB_TXT_Y + 2*(BAR_SUB_BOX_H + 100) + BAR_SUB_BOX_H + BAR_TXT_BOX_H*3+40, "종료 : 설정/저장 키");
	flip();
}



// 총 5개의 명령어 데이터를 순서대로 처리한다.

bool self_test_bar_send_cmd(U8 bar_line, int bar_cnt)
{
	U8 tmp = 0;
	bool send_flag = false;
	int send_cnt;
	U16 rx_time_cnt = 0;

	for(send_cnt = 0; send_cnt < 5;)
	{
		// send command
		if(send_flag == false)
		{
			esp32_packet_send(CMD1_SEND_GET_BAR, bar_line<<4, &tmp, 0);
			rx_time_cnt = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));
		}
		else
		{
			// if(esp32_packet_receive(CMD1_SEND_GET_BAR,  bar_line<<4, 20+LENGTH + 1+2, buff)  == true)
				// return true;
			// else
			// {
				// if(time_10msec_interval_get(rx_time_cnt)>100)//100msec waite
					// send_cnt++;
			// }
		}
	}
	return false;
}

void self_test_bar_proc(U8 bar_line, int bar_cnt)
{
	int i;
	
	U8 send_cnt = 0;
	bool send_end = false;
	U8 bar_ready = 0;
	CURSOR bar_cur;
	
	memset(&bar_cur, 0, sizeof(CURSOR));
	for(i = 0; i<bar_cnt; i++)
		memset(&self_test_val[i+bar_line*3], 0, sizeof(SELF_TEST_ST));
	remocon_key.key_val = 0xFF;
	smart_bed_display.display_refresh  = true;
	
	while(1)
	{
// KEY READ
		key_read();
		switch(remocon_key.key_val){
			case SET_KEY:
				remocon_key.key_val = 0xFF;
				return;
			default:
				remocon_key.key_val = 0xFF;
		}
		if(send_end == false)
		{
			// send command

			// receive bar data
		}
		else
		{
		}
// display
		if(smart_bed_display.display_refresh == true)
		{
			self_test_bar_draw(bar_line, bar_cnt, bar_ready);
			smart_bed_display.display_refresh = false;
		}
	}
}

U8 test_menu[TEST_MAX][100] = {
	{"BAR TEST 1 ~ 3"},
	{"BAR TEST 4 ~ 6"},
	{"BAR TEST 7 ~ 9"},
	{"BAR TEST 10 ~ 12"},
	{"BAR TEST 13 ~ 15"},
	{"BAR TEST 16"},
	{"MOTOR"},
	{"RPM"},
	{"LED"},
};
#define SEL_BOX_X	20
#define SEL_BOX_Y	50
#define SEL_BOX_W	280
#define SEL_BOX_H	25

void self_test_main_draw(U8 select_line)
{
	int i;

	set_draw_target(getbackframe());// back frame select
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,0));
	draw_rect(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	draw_rect(1, 1, 320-2, 480-2, MAKE_COLORREF(255,255,255));
	
	draw_rectfill(20, 5, 280, 40, MAKE_COLORREF(155,100,155));
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(0,00,0));
	draw_text_kr(g_pFontKor, 30, 10, "***** 자가 진단 *****");
	
	for(i = 0; i<TEST_MAX; i++)
	{
		if(i == select_line)
		{
			//memset(string, 0, sizeof(string));
			draw_rectfill(SEL_BOX_X , SEL_BOX_Y + (i * SEL_BOX_H), SEL_BOX_W, SEL_BOX_H, MAKE_COLORREF(0,0,255));
			egl_font_set_color(g_pFont16, MAKE_COLORREF(255,255,255));
			//sprintf(string, "SEFL TEST");
			bmpfont_draw(g_pFont16, SEL_BOX_X + 30, SEL_BOX_Y + (i * SEL_BOX_H) + 5, test_menu[i]);
			//debugprintf("\n\rtrue:%s i[%d], select_line[%d]",test_menu[i], i, select_line);
		}
		else
		{
			egl_font_set_color(g_pFont16, MAKE_COLORREF(255,255,255));
			//sprintf(string, "SEFL TEST");
			bmpfont_draw(g_pFont16, SEL_BOX_X + 30, SEL_BOX_Y + (i * SEL_BOX_H) + 5, test_menu[i]);
			//debugprintf("\n\rfalse:%s i[%d], select_line[%d]", test_menu[i], i, select_line);
		}
	}
	flip();
}

// 
// 메뉴는 UP/DOWN KEY로 선택한다.
// 실행은 확인 키로 실행한다.
// EXIT는 설정 KEY로 EXIT한다.
//
void self_test_main(void)
{
	//U8 disp_ref = true;
	CURSOR self_cur;
	
	memset(&self_cur, 0, sizeof(CURSOR));
	self_cur.type_max = TEST_MAX;
	smart_bed_display.display_refresh  = true;
	remocon_key.key_val = 0xFF;
	while(1)
	{
		key_read();
		switch(remocon_key.key_val){
			case SET_KEY:
				remocon_key.key_val = 0xFF;
				return;
				//break;
			case UP_KEY:		
				up_key_self_test(&self_cur);
				remocon_key.key_val = 0xFF;
				debugprintf("\n\r %d", self_cur.type);
				break;
			case DOWN_KEY:	
				down_key_self_test(&self_cur);
				remocon_key.key_val = 0xFF;
			debugprintf("\n\r %d", self_cur.type);
				break;
				
			case CONFORM_KEY:
				switch(self_cur.type)
				{
					case BAR_TEST_1_3:
						self_test_bar_proc(BAR_TEST_1_3, 3);
						break;
						
				}
		//		if(cursor.type == SELF_TEST)
				//	self_test();
				remocon_key.key_val = 0xFF;
				smart_bed_display.display_refresh  = true;
				break;
			// case SET_KEY:
				// //set_key_proc(CMD2_DISPERSION, dispersion.body[cursor.body], sizeof(dispersion.body[cursor.body]));	// 일반
				// remocon_key.key_val = 0xFF;
				// break;
			default:
				remocon_key.key_val = 0xFF;
		}
		if(smart_bed_display.display_refresh == true)
		{
			self_test_main_draw(self_cur.type);
			smart_bed_display.display_refresh = false;
		}
	}
}

#define USER_MANUAL	0
#define SELF_TEST 1
void manual_selft_test_draw(void);
BODY set;
U8 set_mode = USER_MANUAL;
void manual_selft_test_proc(void)
{
	if(smart_bed_display.status != MODE_SET_SAVE)
	{
		smart_bed_display.status = MODE_SET_SAVE;
		smart_bed_display.display_refresh = true;
		memset(&cursor, 0, sizeof(CURSOR));// key init
		cursor.type_max = 2;
		memcpy(&temp_body, &dispersion, sizeof(BODY));// 현재 설정 임시 버퍼로 복사
		
		remocon_key.key_val = 0xFF;

		conform_key_run = 0;
		set_mode = USER_MANUAL;
		memset(&self_test_val, 0, sizeof(SELF_TEST_ST));
		return;
	}
	
	switch(remocon_key.key_val){
		case SET_KEY:
			smart_bed_status.status = MODE_HOME;
			remocon_key.key_val = 0xFF;
			break;
		case LEFT_KEY:		
			left_key_proc1();
			remocon_key.key_val = 0xFF;
			break;
		case RIGHT_KEY:	
			right_key_proc1();
			remocon_key.key_val = 0xFF;
			break;
			
		case CONFORM_KEY:
			if(cursor.type == SELF_TEST)
			{
				self_test_main();
				smart_bed_display.display_refresh = true;
				PRINTLINE;
				manual_selft_test_draw();
			}
			remocon_key.key_val = 0xFF;
			break;
		// case SET_KEY:
			// //set_key_proc(CMD2_DISPERSION, dispersion.body[cursor.body], sizeof(dispersion.body[cursor.body]));	// 일반
			// remocon_key.key_val = 0xFF;
			// break;
		default:
			remocon_key.key_val = 0xFF;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 매뉴얼 및 자가 테스트 Display
// 매뉴얼, 자가 테스트는 LEFT, RIGHT Key로 이동한다.
// 자가 테스트는 확인 Key로 실행한다.
//  - 에러가 없는 경우 "TEST OK"
//  - 에러가 발생했을 경우 "ERROR LIST"를 표시한다.
//
//////////////////////////////////////////////////////////////////////////////////////////////////

#define MAIN_TITLE_X 60
#define MAIN_TITLE_Y 3
#define MAIN_TITLE_W	200
#define MAIN_TITLE_H	40
#define MAIN_TITLE_TXT_X	(MAIN_TITLE_X + 35)
#define MAIN_TITLE_TXT_Y	(MAIN_TITLE_Y + 10)

#define SUB_TITLE_X 10
#define SUB_TITLE_Y 50
#define SUB_TITLE_W	139
#define SUB_TITLE_H	30
#define SUB_TITLE_TXT_X	(SUB_TITLE_X + 15)
#define SUB_TITLE_TXT_Y	(SUB_TITLE_Y + 8)

void manual_selft_test_draw(void){
	set_draw_target(getbackframe());// back frame select
	draw_rectfill(0, 0, 320, 480, MAKE_COLORREF(0,0,0));
	draw_rect(0, 0, 320, 480, MAKE_COLORREF(255,255,255));
	draw_rect(1, 1, 320-2, 480-2, MAKE_COLORREF(255,255,255));
	
	draw_roundrectfill(MAIN_TITLE_X, MAIN_TITLE_Y, MAIN_TITLE_W, MAIN_TITLE_H, 10, MAKE_COLORREF(0,100,150));
	egl_font_set_color(g_pFontKor, MAKE_COLORREF(255,255,255));
	draw_text_kr(g_pFontKor, MAIN_TITLE_TXT_X, MAIN_TITLE_TXT_Y, "*** 설정 ***");
	
	if(cursor.type == USER_MANUAL)
	{
		draw_roundrectfill(SUB_TITLE_X, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255, 255, 255));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(0,2,0));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y, "사용자 매뉴얼");

		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y + 100, "1. 매뉴얼 테스트");
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y + 130, "2. 매뉴얼 테스트");
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y + 160, "3. 매뉴얼 테스트");
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y + 190, "4. 매뉴얼 테스트");

		draw_roundrect(SUB_TITLE_X + SUB_TITLE_W + 20, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255,255,255));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X + SUB_TITLE_W+30, SUB_TITLE_TXT_Y, "자가 진단");
		
	}
	else
	{
		draw_roundrect(SUB_TITLE_X, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255, 255, 255));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y, "사용자 매뉴얼");

		draw_roundrectfill(SUB_TITLE_X + SUB_TITLE_W + 20, SUB_TITLE_Y, SUB_TITLE_W, SUB_TITLE_H, 40, MAKE_COLORREF(255,255,255));
		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(0,0,0));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X + SUB_TITLE_W+30, SUB_TITLE_TXT_Y, "자가 진단");

		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y + 100, "자가 진단을 실행하시겠습니까?");

		egl_font_set_color(g_pFontKor16, MAKE_COLORREF(255,255,255));
		draw_text_kr(g_pFontKor16, SUB_TITLE_TXT_X, SUB_TITLE_TXT_Y + 150, "확인 버튼을 눌러주세요!!");
	}
	
	flip();
}
// ====================== 매뉴얼설정 & 자가진단 End ================= //

 
	
	
