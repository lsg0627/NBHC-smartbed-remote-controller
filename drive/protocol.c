#include "adstar.h"
#include "../app/main.h"
#include "smart_bed_remocon.h"
// self test


U8 esp32_pack_buff[1024] = {0,};
#if 0
void selftest(void)
{
   // selftest_motor_drive();
//    selftest_esp32();
   // selftest_remocon();
}

//
// 1. audio test
// 2. head test
// 3. actuator
typedef struct _protocol
{
    U8 cmd_id;
    U8 act_cmd;
    U8 leng;
    U8 buff[100];
}PROTOCOL_PACKET;
PROTOCOL_PACKET packet;


void selftest_esp32(void)
{
    U8 buff;

    egl_font_set_color(g_pFont48, MAKE_COLORREF(255,255,255));
    set_draw_target(getbackframe());// back frame select
    draw_rect(0,0, 320,480, MAKE_COLORREF(0,0,0));
    bmpfont_draw(g_pFont48, radio_xy[i + BODY_MAX][0]+15, radio_xy[i + BODY_MAX][1]-3, "MAIN SELF TEST START");
    flip();

    //while(1)
    {
        // AUDIO
        buff = true;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_AUDIO, 1, &buff);// audio on
        delayms(1000);// 1sec delay 
        buff = false;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_AUDIO, 1, &buff);// audio on

        // HEAT
        buff = 0;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_HEAT, 1, &buff);
        delayms(10000);// 10sec delay 
        buff = 1;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_HEAT, 1, &buff);
        delayms(10000);// 10sec delay 
        buff = 2;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_HEAT, 1, &buff);
        delayms(10000);// 10sec delay 
        buff = 3;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_HEAT, 1, &buff);
        delayms(10000);// 10sec delay 
         buff = 0;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_HEAT, 1, &buff);

    }
    set_draw_target(getbackframe());// back frame select
    draw_rect(0,0, 320,480, MAKE_COLORREF(0,0,0));
    bmpfont_draw(g_pFont48, radio_xy[i + BODY_MAX][0]+15, radio_xy[i + BODY_MAX][1]-3, "MAIN SELF TEST END");
     bmpfont_draw(g_pFont48, radio_xy[i + BODY_MAX][0]+15, radio_xy[i + BODY_MAX][1]-3, "Press the CONFORM KEY");
    flip();

}
//
// if KEY PUSHED : BLUE
// EXIT : POWER KEY
//
/*
u8 test_key_xy[14][2] ={{10,50}, // power
                        {10, 60}, {50, 60}, 
                            };
void key_test_draw(U16 key_st)
{
    int i;

    egl_font_set_color(g_pFont48, MAKE_COLORREF(255,255,255));
    set_draw_target(getbackframe());// back frame select
    draw_rect(0,0, 320,480, MAKE_COLORREF(0,0,0));
    bmpfont_draw(g_pFont48, radio_xy[i + BODY_MAX][0]+15, radio_xy[i + BODY_MAX][1]-3, "REMOCON KEY TEST");
    for(i=0; i<; i++)
    {
        if(key_set & (1<< i))
            draw_rectfill(test_key_xy[i][0],test_key_xy[i][1], 20,20, MAKE_COLORREF(0,0,255));
        else
            draw_rect(test_key_xy[i][0],test_key_xy[i][1], 20,20, MAKE_COLORREF(255,255,255));
    }
    flip();
}
void remocon_key_test(void)
{
    U16 pushed_key = 0;
    bool refresh = true;

    while(1)
    {
        if(remocon_key.key_val != 0xFF){
            pushed_key ^= remocon_key.key_val;
            remocon_key.key_val = 0xFF;
            refresh = true;
        }
        if(refresh == true){
            key_test_draw(pushed_key);
            refresh = false;
        }
    }
}
*/
//
// led test
// 1. all on/off
// 2. shift move on/off
//
void remocon_led_test(void)
{
    int i;

    egl_font_set_color(g_pFont48, MAKE_COLORREF(255,255,255));
    set_draw_target(getbackframe());// back frame select
    draw_rect(0,0, 320,480, MAKE_COLORREF(0,0,0));
    bmpfont_draw(g_pFont48, radio_xy[i + BODY_MAX][0]+15, radio_xy[i + BODY_MAX][1]-3, "REMOCON LED TEST");
    draw_rect(0,0, 320,480, MAKE_COLORREF(0xFF,0,0));
     bmpfont_draw(g_pFont48, radio_xy[i + BODY_MAX][0]+15, radio_xy[i + BODY_MAX][1]-3, "When the test is complete, press the Conform key");
    flip();
    while(1)
    {
        for(i=0; i<3; i++)
        {
            *R_GPOLOW(3) = (1<<i);// on
            *R_GPOHIGH(3) = ~(1<<i);// off
        }
        *R_GPOLOW(3) = 0x07;// all on
        delayms(500);
        *R_GPOHIGH(3) = 0x07;// all off
        delayms(500);
        if(remocon_key.key_val == CONFORM_KEY)
        {
            remocon_key.key_val = 0xFF;
            return;
        }
    }
}
/*
    1. key test : 
    2. LED Test
    3. nand test
*/
void selftest_remocon(void)
{
    remocon_key_test();
}

#endif
//
//////////////////////////////////////////////////////////////////////////////////////////////////
//
// BAUD : 115200
// Data : 8bit
// Parity : None
// Stop bit : 1
// Flow Control : None
// Little Endian
// Checksum(2Byte) : Data(nByte)
// STX : 0xFF81
// PRODUCT CODE : "SB1C"
//
//  FF81 "9B1C" CMD1 CMD2 DataLeng Data N, CRC16
//
//////////////////////////////////////////////////////////////////////////////////////////////////
#include "../app/main.h"

ESP32_COMM esp32_uart;
ESP32_GET esp32_get_infomation;

void esp32_com_init(void)
{
	uart_config(ESP32_UART_CH, 115200, DATABITS_8, STOPBITS_1, UART_PARNONE);
	memset(&esp32_uart, 0x00, sizeof(ESP32_COMM));
}




//////////////////////////////////////////////////////////////////////////////////////////////////
//
// 16bit CRC CCITT : 0x1021(X^(16)+X^(12)+X^5+X
//  
U16 agms_calc_crc16(U8 *payload, U8 length)
{
	U8 i, j;
	U32 temp, temp2, flag;
//	PRINTLINE;
	temp = 0xFFFF;
	for(i=0; i<length; i++)
	{
	//	debugprintf("0x%x ", payload[i]);
		temp = temp ^ payload[i];
		for(j=0; j<=8; j++)
		{
			flag = temp & 0x0001;
			temp >>= 1;
			if(flag)
				temp ^= 0xA001;
		}
	}
	// Reverse byte order.
	temp2 = temp >> 8;
	temp = (temp<<8) | temp2;
	temp &= 0xFFFF;
	// the returned value is already swapped
	// crcLo byte is first & crcHi byte is last
	return temp;
}

bool compare_checksum(U8 *data, int length, U16 crc16)
{
	U16 crctmp = 0;
	
	crctmp = agms_calc_crc16(data, length);
	debugprintf("\n\r rx crc[%x] mk crc[%x] leng[%d]", crc16, crctmp, length);
	if(crctmp == crc16) return true;
	return false;
}



#define SYNC_CMD_TH 0
#define ID_CMD_TH   6
#define AT_CMD_TH   7
#define LENGTH_TH   8

//
// packet transive
//

void esp32_packet_send(U8 cmd_id, U8 act_id, U8 *buff, U8 data_leng)
{
	int i = 0;
	U16 crc16 = 0;
	
	esp32_uart.tx_buff[0] = 0xFF;
	esp32_uart.tx_buff[1] = 0x81;
	esp32_uart.tx_buff[2] = 'S';
	esp32_uart.tx_buff[3] = 'B';
	esp32_uart.tx_buff[4] = '1';
	esp32_uart.tx_buff[5] = 'C';
	
	esp32_uart.tx_buff[ID_CMD_TH] = cmd_id;
	esp32_uart.tx_buff[AT_CMD_TH] = act_id;
	esp32_uart.tx_buff[LENGTH_TH] = data_leng;
// DATA
	for(i = 0; i<data_leng; i++)
		esp32_uart.tx_buff[LENGTH_TH +1 + i] = buff[i];
// CRC
	crc16 = agms_calc_crc16(esp32_uart.tx_buff, (data_leng + LENGTH_TH +1));
	memcpy(&esp32_uart.tx_buff[(LENGTH +1)+data_leng], &crc16, sizeof(crc16));
	
	debugprintf("\n\r");

	for(i=0; i< (11+data_leng); i++)
		debugprintf("0x%02x, ", esp32_uart.tx_buff[i]);
	uart_putdata(ESP32_UART_CH, esp32_uart.tx_buff, (data_leng + LENGTH_TH + 1 + 2));
}


bool esp32_packet_parsing(U8 cmd1, U8 cmd2, U8 leng, U8 *buff)
{
	U8 tmp_buff[RX_MAX_NUM] = {0,};
	U8 tmp_packet[6] = {0xFF, 0x81, '9', 'B', '1', 'C'};
	U8 tmp_status = 0;
	U16 crc16 = 0;
	int i = 0;
	
	if(esp32_uart.rx_write_pointer > esp32_uart.rx_read_pointer)
    {
		while(1)
		{// Start sync
			switch(tmp_status)
			{
				case 0:// SYNC
					if(memcmp(&esp32_uart.rx_buff[esp32_uart.rx_read_pointer], tmp_packet, sizeof(tmp_packet)))
					{// Start sync가 맞으면 한단계씩 전진하면서 찾기.
					debugprintf("\n\r [%d]--->%x [%x] ",esp32_uart.rx_read_pointer, esp32_uart.rx_buff[esp32_uart.rx_read_pointer], tmp_packet[esp32_uart.rx_read_pointer]);
						esp32_uart.rx_read_pointer++;
						if(esp32_uart.rx_read_pointer >= esp32_uart.rx_write_pointer)
						{
							PRINTLINE;
							return false;
						}
					}
					else 
					{
#if 0
						memcpy(&crc16, &esp32_uart.rx_buff[esp32_uart.rx_read_pointer+ esp32_uart.rx_buff[esp32_uart.rx_read_pointer+ LENGTH] + LENGTH + 1] , 2);
						if(compare_checksum(&esp32_uart.rx_buff[esp32_uart.rx_read_pointer],  8+esp32_uart.rx_buff[esp32_uart.rx_read_pointer + LENGTH]+1, crc16) == false)
						{
						//	debugprintf("\n\r > RX CRC[%x]  [%d]", crc16, , 6+esp32_uart.rx_buff[esp32_uart.rx_read_pointer + LENGTH]);
							PRINTLINE;
							return false;
						}
#endif
						tmp_status++;
						esp32_uart.rx_read_pointer += sizeof(tmp_packet);
					}
					//PRINTLINE;
					break;
				case 1:// CMD1
					if(esp32_uart.rx_buff[esp32_uart.rx_read_pointer] == cmd1)
					{
						tmp_status++;
						esp32_uart.rx_read_pointer++;
					}
					else{
						PRINTLINE;
						tmp_status = 0;
						return false;
					}
					break;
				case 2:// CMD2
					if(esp32_uart.rx_buff[esp32_uart.rx_read_pointer] == cmd2)
					{
						tmp_status++;
						esp32_uart.rx_read_pointer++;
					}
					else{
						PRINTLINE;
						tmp_status = 0;
						return false;
					}
					break;
				case 3:// data length
					if(esp32_uart.rx_buff[esp32_uart.rx_read_pointer] == leng)
					{
						tmp_status++;
						esp32_uart.rx_read_pointer++;
					}
					else{
						PRINTLINE;
						tmp_status = 0;
						return false;
					}
					break;
				case 4:// data n
					memcpy(buff, &esp32_uart.rx_buff[esp32_uart.rx_read_pointer], leng);
					esp32_uart.rx_read_pointer += 2 + leng;
				tmp_status = 0;
					return true;
			}
		}
		memcpy(esp32_pack_buff, &esp32_uart.rx_buff[esp32_uart.rx_read_pointer + ID_CMD_TH], esp32_uart.rx_buff[esp32_uart.rx_read_pointer + LENGTH_TH] + LENGTH_TH + 2);
		memcpy(&crc16, &esp32_pack_buff[LENGTH_TH-6], 2);
		compare_checksum(esp32_pack_buff, (esp32_uart.rx_buff[esp32_uart.rx_read_pointer + LENGTH_TH] + LENGTH_TH), crc16);
		esp32_uart.rx_read_pointer  = esp32_uart.rx_write_pointer;
//		complite = true;
		esp32_uart.rx_flag = false;
		PRINTLINE;
    }
    else
    {// data가 중간에 끊긴 경우
        int tmp_leng = (RX_MAX_NUM - esp32_uart.rx_read_pointer) + esp32_uart.rx_write_pointer;
     //   if(tmp_leng > ESP32_PACK_NUM)
        {
			memcpy(tmp_buff, &esp32_uart.rx_buff[esp32_uart.rx_read_pointer], (RX_MAX_NUM - esp32_uart.rx_read_pointer));
			memcpy(&tmp_buff[RX_MAX_NUM - esp32_uart.rx_read_pointer], esp32_uart.rx_buff, esp32_uart.rx_write_pointer);
			i = 0;
			while(1)
			{// Start sync
				if(memcmp(&tmp_buff[i], tmp_packet, sizeof(tmp_packet)))
				{// Start sync가 맞으면 한단계씩 전진하면서 찾기.
					i++;
					tmp_leng--;
					if(i >= tmp_leng)
						return;
				}
				else
				{// Start sync 성공.
					if(tmp_leng >= (tmp_buff[i + LENGTH_TH] + LENGTH_TH + 2))
						break;
					else 
						return;
				}
			}
			memcpy(esp32_pack_buff, &esp32_uart.rx_buff[esp32_uart.rx_read_pointer + ID_CMD_TH], esp32_uart.rx_buff[esp32_uart.rx_read_pointer + LENGTH_TH] + LENGTH_TH + 2);
			memcpy(&crc16, &esp32_pack_buff[LENGTH_TH-6], 2);
			compare_checksum(esp32_pack_buff, (esp32_uart.rx_buff[esp32_uart.rx_read_pointer + LENGTH_TH] + LENGTH_TH), crc16);
			esp32_uart.rx_read_pointer  = esp32_uart.rx_write_pointer;
//			complite = true;
			esp32_uart.rx_flag = false;
			PRINTLINE;
        }
    }
	return false;
}

//
// packet receive
// If no data is received for 50 msec, the value is discarded and the pointer is initialized.
//
//extern bool get_cmd_send;
bool esp32_packet_receive(U8 cmd1, U8 cmd2, U8 get_size, U8 *buff)
{
    int rx_leng = 0;
    U8 tmp_buff[RX_MAX_NUM/2] = {0,};
    U16 cur_time = 0;
	int i;
	static int rx_cnt = 0;
//    U16 stx;
//    U32 pdcode;

    rx_leng = uart_getdata(ESP32_UART_CH, tmp_buff, RX_MAX_NUM/2);// uart data get
    if(rx_leng)
    {// if rx data
        esp32_uart.rx_time_count = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		esp32_uart.rx_flag = true;
		rx_cnt += rx_leng;
        // data copy
        if((esp32_uart.rx_write_pointer + rx_leng) >  RX_MAX_NUM)
        {// max buffer size over
            memcpy( &esp32_uart.rx_buff[esp32_uart.rx_write_pointer], tmp_buff, (RX_MAX_NUM - esp32_uart.rx_write_pointer) );
            memcpy( esp32_uart.rx_buff, &tmp_buff[RX_MAX_NUM - esp32_uart.rx_write_pointer], (esp32_uart.rx_write_pointer- rx_leng) );
            esp32_uart.rx_write_pointer = (esp32_uart.rx_write_pointer + rx_leng) - RX_MAX_NUM;
			debugprintf("\n\r ddd [%d] %d]", esp32_uart.rx_write_pointer,esp32_uart.rx_write_pointer + rx_leng);
			debugprintf("\n\r cccc [%d] [%d] ", esp32_uart.rx_write_pointer );
			PRINTLINE;
        }
        else
        {
            memcpy(&esp32_uart.rx_buff[esp32_uart.rx_write_pointer], tmp_buff, rx_leng);
            esp32_uart.rx_write_pointer += rx_leng;
			//PRINTLINE;
        }
//		debugprintf("\n\r %d %d", 
		if(rx_cnt >= get_size)
		{
			//PRINTLINE;
			rx_cnt = 0;
			if(esp32_packet_parsing(cmd1, cmd2, get_size - 11, buff) == true)
			{
				esp32_uart.rx_time_count = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
				return true;
			}
			//get_cmd_send = false;
			PRINTLINE;
			return false;
		}
			//esp32_packet_parsing();
    }  
    else
    {// If no data is received for 100 msec, the value is discarded and the pointer is initialized.
        if(esp32_uart.rx_flag)
        {// If received data?
			if(rx_cnt >= get_size)
			{
				PRINTLINE;
				esp32_packet_parsing(cmd1, cmd2, get_size - 11, buff);
				rx_cnt = 0;
				esp32_uart.rx_flag = false;
			}
			//else
			{
				cur_time = time_10msec_interval_get(esp32_uart.rx_time_count);
				//debugprintf("\n\r time %d", cur_time);
				if(cur_time > RX_MAX_TIME)
				{
					PRINTLINE;
					for(i = 0; esp32_uart.rx_read_pointer !=  esp32_uart.rx_write_pointer ; i++)
					{
						debugprintf("-==>0x%02x ", esp32_uart.rx_buff[esp32_uart.rx_read_pointer]);
						esp32_uart.rx_read_pointer++;
					}
					esp32_uart.rx_read_pointer =  esp32_uart.rx_write_pointer = 0;
					esp32_uart.rx_flag = false;
					rx_cnt = 0;
					esp32_get_info_init();
					//get_cmd_send = false;
				}
				
				//else
			//		esp32_packet_parsing();
			}
        }
    }
	return false;
}

void esp32_get_info_init(void){
	memset(&esp32_get_infomation, 0, sizeof(ESP32_GET));
	esp32_get_infomation.get_info_time = 10;
}

// 최소 data가 8byte이상일 경우
bool esp32_packet_parsing_bar_body(U8 *buff, int leng)
{
	U8 sync_packet[6] = {0xFF, 0x81, '9', 'B', '1', 'C'};
	U16 crc16 = 0;
	int i = 0;
	int tmp;
	while(1)
	{// Start sync
		if(memcmp(&buff[i], sync_packet, sizeof(sync_packet)))
		{// Start sync가 맞으면 한단계씩 전진하면서 찾기.
			debugprintf("\n\r sync byte err i[%d]%x", i, buff[i]);
			i++;
			esp32_uart.rx_read_pointer++;
			if(i >= leng)
			{
				PRINTLINE;
				return false;
			}
		}
		else 
		{// Packet start sync ok
			//tmp_leng = buff[i+LENGTH];// 길이
			if((buff[i+LENGTH]  + LENGTH + 3) > leng)
			{// data가 덜 왔다.
				PRINTLINE;
				return false;
			}
			// 한 패킷만 처리;
			// CRC16 make
			//memcpy(&crc16, buff[buff[i+LENGTH] + LENGTH + 1],sizeof(crc16));// CRC Copy
			//compare_checksum(esp32_pack_buff, (esp32_uart.rx_buff[esp32_uart.rx_read_pointer + LENGTH_TH] + LENGTH_TH), crc16);
		
			switch(buff[CMD_ID + i])
			{
				case CMD1_GET_BAR_INFO:	// bar information
					// [디버그 출력 제거] 모터 이동 시 high-frequency 패킷 → debug print 폭주 방지
					memcpy(bar, &buff[i+LENGTH+1], buff[LENGTH]);
					smart_bed_display.display_refresh = true;
					break;
				case CMD1_GET_PRESSURE_MAP:	// pressure map (11x7 = 77 bytes)
					bed_status_silence = 0;	// 연결 확인 리셋 (link_lost 방지)
					if(buff[i+LENGTH] >= PRESSURE_MAP_SIZE) {
						U8 *new_data = &buff[i+LENGTH+1];
						U8 *old_data = (U8*)pressure_map;
						int n;
						bool changed = false;
						for(n = 0; n < PRESSURE_MAP_SIZE; n++) {
							int d = (int)new_data[n] - (int)old_data[n];
							if(d < 0) d = -d;
							if(d >= 8) { changed = true; break; }
						}
						memcpy(pressure_map, new_data, PRESSURE_MAP_SIZE);
						if(changed) smart_bed_display.display_refresh = true;
					}
					break;
				case CMD1_GET_MOTOR_POSITION:	// 11 모터 위치 (22 bytes, int16 LE)
					bed_status_silence = 0;	// 연결 확인 리셋
					if(buff[i+LENGTH] >= 22) {
						int k;
						bool changed = false;
						for(k = 0; k < 11; k++) {
							U8 lo = buff[i+LENGTH+1 + k*2];
							U8 hi = buff[i+LENGTH+1 + k*2 + 1];
							S16 newp = (S16)(((U16)hi << 8) | lo);
							S16 diff = newp - motor_positions[k];
							if(diff < 0) diff = -diff;
							if(diff >= 5) {
								motor_positions[k] = newp;
								changed = true;
							} else if(newp != motor_positions[k]) {
								motor_positions[k] = newp;
							}
						}
						if(changed) smart_bed_display.display_refresh = true;
					}
					break;
				case CMD1_GET_BODY_INFO:	// body information
					if(buff[i+LENGTH+1] == 0x30)
						body_info = true;
					else body_info = false;
					smart_bed_display.display_refresh = true;
					break;
				case CMD1_SEND_RUN_ST:	// 동작상태 에코 ACK
					if(buff[ACTI_D + i] == CMD2_STDBY){
						if(stdby_in_progress) {
							stdby_complete = true;
							debugprintf("\n\r ACK: STDBY COMPLETE (legitimate)");
							// 진짜 종료(전원키/포어스 리셋)일 때만 running_massage_type 클리어.
							// 마사지 시작 시 ESP32가 init(호밍) 후 보내는 orphan STDBY ACK에서는
							// 클리어하면 안 됨 → 나중에 마사지 화면 재진입 시 커서 자동 선택 실패.
							running_massage_type = -1;
						} else {
							debugprintf("\n\r ACK: STDBY (orphan, ignored)");
							// 마사지 시작 시 나오는 orphan STDBY ACK — running_massage_type 유지
						}
					}
					else if(buff[ACTI_D + i] >= CMD2_MASSAGE && buff[ACTI_D + i] < (CMD2_MASSAGE + MASSAGE_MAX)){
						// 마사지 시작 ACK — 즉시 bed_status 반영
						running_massage_type = buff[ACTI_D + i] - CMD2_MASSAGE;
						bed_status.current_mode = buff[ACTI_D + i];
						bed_status.run_state = 1;
						external_stopping = false;
						pending_mode_start = true;	// state=3 transient를 stopping으로 잘못 감지하지 않도록
						pending_mode_start_timeout = 150;	// 15초
						smart_bed_display.display_refresh = true;
						debugprintf("\n\r ACK: MASSAGE %d (pending_mode_start=on)", running_massage_type + 1);
					}
					// 다른 모드 시작 ACK들도 즉시 bed_status 반영
					else if(buff[ACTI_D + i] == CMD2_DISPERSION ||
					        buff[ACTI_D + i] == CMD2_VENTIL_NORMAL ||
					        buff[ACTI_D + i] == CMD2_VENTIL_FOCUR ||
					        buff[ACTI_D + i] == CMD2_VENTIL_SLEEP ||
					        buff[ACTI_D + i] == CMD2_HEAR ||
					        buff[ACTI_D + i] == CMD2_MEAL ||
					        buff[ACTI_D + i] == CMD2_TILT_CARE ||
					        buff[ACTI_D + i] == CMD2_TREND) {
						bed_status.current_mode = buff[ACTI_D + i];
						bed_status.run_state = 1;
						external_stopping = false;
						pending_mode_start = true;
						pending_mode_start_timeout = 150;
						smart_bed_display.display_refresh = true;
						debugprintf("\n\r ACK: MODE 0x%02x (pending_mode_start=on)", buff[ACTI_D + i]);
					}
					else if(buff[ACTI_D + i] == CMD2_TILT_READY) {
						// ESP32 tilt_care[] routine 완료 통지 → 리모컨 UI 좌/우 조그 활성화
						tilt_care_ready = true;
						smart_bed_display.display_refresh = true;
						debugprintf("\n\r ACK: TILT_READY (jog UI enabled)");
					}
					else if(buff[ACTI_D + i] == CMD2_PAUSE) {
						// 일시정지 ACK
						if(bed_status.current_mode != 0) {
							bed_status.run_state = 2;
							smart_bed_display.display_refresh = true;
							debugprintf("\n\r ACK: PAUSE");
						}
					}
					else if(buff[ACTI_D + i] == CMD2_RESTART) {
						// 재시작 ACK
						if(bed_status.current_mode != 0) {
							bed_status.run_state = 1;
							smart_bed_display.display_refresh = true;
							debugprintf("\n\r ACK: RESTART");
						}
					}
					else if(buff[ACTI_D + i] == CMD2_FALL_ALERT){
#if FALL_ALERT_UI_ENABLED
						smart_bed_status.status = MODE_FALL_ALERT;
						smart_bed_display.status = MODE_FALL_ALERT;
						smart_bed_display.display_refresh = true;
						debugprintf("\n\r *** FALL ALERT ***");
#else
						// 낙상 기능 미동작 — 화면 전환 없이 로그만 남긴다 (protocol.h: FALL_ALERT_UI_ENABLED)
						debugprintf("\n\r *** FALL ALERT (UI disabled) ***");
#endif
					}
					else if(buff[ACTI_D + i] == CMD2_FALL_CLEAR){
						// 낙상 경고 화면일 때만 반응한다. 그 외 모드에서 화면을 뺏으면 안 된다.
						// (FALL_ALERT_UI_ENABLED=0이면 이 조건이 성립하지 않아 무시된다.)
						if(smart_bed_status.status == MODE_FALL_ALERT){
							// 낙상 경고 중 확인창이 보류되어 있었으면 그쪽으로 복귀 (spec T8)
							if(startup_confirm_active){
								smart_bed_status.status = MODE_STARTUP_CONFIRM;
								smart_bed_display.status = MODE_STARTUP_CONFIRM;
								conform_hold_cnt = 0;
								debugprintf("\n\r FALL ALERT CLEARED -> back to startup confirm");
							} else {
								smart_bed_status.status = MODE_HOME;
								smart_bed_display.status = MODE_HOME;
								debugprintf("\n\r FALL ALERT CLEARED");
							}
							smart_bed_display.display_refresh = true;
						} else {
							debugprintf("\n\r FALL CLEAR ignored (not in fall alert)");
						}
					}
					break;
				case CMD1_DATA_SYNC:	// ESP32 → 리모컨 설정 동기화
				{
					U8 act = buff[ACTI_D + i];
					U8 dlen = buff[i + LENGTH];
					U8 *data = &buff[i + LENGTH + 1];
					// data[0]의 상위 nibble로 부위 index 결정 (0x1x=Head, 0x2x=Upper, 0x3x=Lower, 0x4x=Leg)
					int body_idx = (data[0] >> 4) - 1;  // 0~3
					if(body_idx < 0 || body_idx >= BODY_MAX) break;

					if(act == CMD2_VENTIL_NORMAL || act == CMD2_VENTIL_FOCUR || act == CMD2_VENTIL_SLEEP)
					{	// 교대부양
						int type = act - CMD2_VENTIL_NORMAL;  // 0=일반, 1=집중, 2=수면
						if(type >= 0 && type < LEVIT_MAX && dlen <= (BODY_LEVIT_MAX + 1))
							memcpy(levitate[type].body[body_idx], data, dlen);
					}
					else if(act == CMD2_DISPERSION)
					{	// 체압분산
						if(dlen <= (BODY_LEVIT_MAX + 1))
							memcpy(dispersion.body[body_idx], data, dlen);
					}
					else if(act >= CMD2_MASSAGE && act < (CMD2_MASSAGE + MASSAGE_MAX))
					{	// 마사지
						int type = act - CMD2_MASSAGE;
						if(type >= 0 && type < MASSAGE_MAX && dlen <= (BODY_LEVIT_MAX + 1))
							memcpy(massage[type].body[body_idx], data, dlen);
					}
					debugprintf("\n\r SYNC: act=0x%02x body[%d]", act, body_idx);
					break;
				}
				case CMD1_BED_STATUS:	// 침대 상태 (2초 주기 수신)
				{
					U8 dlen = buff[i + LENGTH];
					if(dlen >= sizeof(BED_STATUS_DATA)){
						BED_STATUS_DATA new_status;
						U8 esp_mode = buff[i+LENGTH+1];
						U8 esp_state = buff[i+LENGTH+2];
						U8 final_mode, final_state;
						U8 prev_mode = bed_status.current_mode;
						U8 prev_state = bed_status.run_state;
						U8 prev_pending = prev_startup_pending;
						U8 prev_powered = bed_status.powered_on;
						// 통신 끊김 감지: 이 패킷 도착 직전까지 얼마나 조용했는지 (100ms 틱)
						// > 30 (3초) 이면 ESP32가 잠시 offline이었다고 판단
						bool esp32_was_silent = (bed_status_silence > 30);
						memcpy(&new_status, &buff[i + LENGTH + 1], sizeof(BED_STATUS_DATA));

						// GetBedStatus 응답 or 브로드캐스트 — 둘 다 여기로 온다 (spec §3)
						boot_status_done = true;
						master_booted = true;		// 마스터 부팅 완료 — 부팅 스피너 해제 신호
						bed_status_silence = 0;		// 끊김 감지 리셋 (§7)

						// [init_home_pending 클리어] state=1 packet 개수 카운트.
						// ESP32는 CONFORM 직후 (mode,1) 한 번, BarsInitialized에서 (mode,1) 또 한 번.
						// 2번째 도착 = post-homing → 클리어. 홈 시간 무관.
						// (lock override 전 raw esp_state/esp_mode 사용 — 실제 ESP32 상태 반영)
						if(init_home_pending && esp_state == 1 && esp_mode != 0){
							init_home_state1_count++;
							if(init_home_state1_count >= 2){
								init_home_pending = false;
								init_home_pending_timeout = 0;
								smart_bed_display.display_refresh = true;
								debugprintf("\n\r [INIT] home complete (state1 count=2, mode=0x%02x)", esp_mode);
							}
						}

						// 로컬 상태 잠금 — ESP32가 로컬 상태 확인할 때까지 mode/state 보호
						if(bed_state_lock_remain > 0){
							if(new_status.current_mode == bed_status.current_mode &&
							   new_status.run_state == bed_status.run_state){
								bed_state_lock_remain = 0;
							} else {
								new_status.current_mode = bed_status.current_mode;
								new_status.run_state = bed_status.run_state;
							}
						}
						final_mode = new_status.current_mode;
						final_state = new_status.run_state;
						debugprintf("\n\r [RX] ESP32: mode=0x%02x state=%d | applied: mode=0x%02x state=%d (lock=%d)",
							esp_mode, esp_state, final_mode, final_state, bed_state_lock_remain);
						memcpy(&bed_status, &new_status, sizeof(BED_STATUS_DATA));

						// ---- 볼륨(음량) 동기화 ----
						// ESP32가 보고하는 실제 볼륨(NVS 유지값)으로 리모컨 LED/카운터를 맞춘다.
						// 전원 재부팅/초기화 후에도 저장된 음량이 LED에 그대로 반영된다.
						// 단, 사용자가 방금 조절 중(vol_lock_remain>0)이면 왕복 지연 중 되돌림 방지를 위해 건너뛴다.
						if(vol_lock_remain == 0 && bed_status.volume_level <= 3 &&
						   volume.body[0][0] != bed_status.volume_level){
							volume.body[0][0] = bed_status.volume_level;
							volume_led_ctrl(bed_status.volume_level);
						}

						// ================= 부팅 확인창 관련 전이 (spec §5.3 / §5.4) =================
						// 주의: bed_state_lock_remain은 mode/state만 덮어쓰고 startup_pending은 통과시킨다.
						//
						// [1] pending 0→1 : Master watchdog 복구 또는 Master 재부팅.
						//     watchdog 복구는 run_state 3→0 과 pending 0→1 을 같은 패킷에 실어 보낸다.
						//     그래서 반드시 Path B보다 먼저 평가해야 한다. 순서를 바꾸면
						//     watchdog 복구를 홈 완료로 오인하여 확인창 대신 일반 UI로 들어간다.
						// [침대 재부팅 감지] 다음 중 하나라도 성립하면 ESP32가 재부팅한 것으로 판정.
						//   (1) startup_pending 0→1 전환 (기존 케이스 — 정상 상황)
						//   (2) 3초 이상 통신 끊김 후 startup_pending=1 수신
						//       (예: prev가 이미 1이었는데 ESP32가 재부팅. transition은 안 뜨지만 silence로 감지)
						//   (3) powered_on 1→0 + startup_pending=1 (ESP32 부팅 직후 상태)
						bool bed_rebooted =
							(bed_status.startup_pending == 1) &&
							((prev_pending == 0) || esp32_was_silent ||
							 (prev_powered == 1 && bed_status.powered_on == 0));

						if(bed_rebooted){
							debugprintf("\n\r [REBOOT] master reboot detected (prev_pending=%d, silence=%d, prev_pwr=%d->%d)",
								prev_pending, esp32_was_silent, prev_powered, bed_status.powered_on);
							stdby_in_progress = false;
							stdby_complete = false;
							stdby_timeout = 0;
							// ESP32의 heat_level/volume은 0으로 초기화됨 → 리모컨도 동기화
							// 내부 상태 + LED 모두 리셋 (사용자가 이전 상태로 오해하지 않도록)
							heat.body[0][0] = 0;
							heat_led_ctrl(0);
							if(power){
								startup_confirm_active = true;
								conform_hold_cnt = 0;
								// 낙상 경고가 떠 있으면 그 화면이 우선 (spec §9, T8).
								// 확인창은 낙상 해제(CMD2_FALL_CLEAR) 후에 표시된다.
								if(smart_bed_status.status != MODE_FALL_ALERT){
									smart_bed_status.status = MODE_STARTUP_CONFIRM;
									smart_bed_display.status = MODE_STARTUP_CONFIRM;
								}
							}
							// power==false(LCD OFF)면 플래그만 유지. 다음 전원키 누름에서 확인창 진입.
						}
						// [2] Path B — 브로드캐스트로 홈 완료 감지.
						//     stdby_in_progress를 가드에 넣지 말 것. 이 경로가 필요한 두 경우 모두
						//     그 값이 false다: (a) 리모컨이 STDBY를 보내지 않음(홈 중 재부팅),
						//     (b) 돌봄케어 모드 중 홈 — Master가 ActionEcho를 아예 보내지 않는다.
						else if(prev_state == 3 && final_state == 0 && bed_status.startup_pending == 0){
							if(stdby_in_progress){
								debugprintf("\n\r [PATH B] run_state 3->0 -> homing done");
								stdby_in_progress = false;
								stdby_complete = false;
								stdby_timeout = 0;
							}
							if(smart_bed_status.status == MODE_STARTUP_CONFIRM){
								startup_confirm_active = false;
								smart_bed_status.status = MODE_HOME;
								smart_bed_display.status = MODE_HOME;
							}
						}
						prev_startup_pending = bed_status.startup_pending;

						// pending_pwr_on_check: REMO_PWR_ON에서 bed_status 기반으로 즉시 결정 (불필요)

						// 외부(태블릿) 종료 전환 감지
						// pending_mode_start 중에는 모드 시작 시의 state=3 (init)을 stopping으로 잘못 감지하지 않도록 skip
						if(!stdby_in_progress && !pending_mode_start &&
						   (prev_state == 1 || prev_state == 2) &&
						   (final_state == 3 || (final_state == 0 && prev_mode != 0))) {
							if(!external_stopping){
								external_stopping = true;
								external_stopping_timeout = 200;	// 20초 안전 timeout
								external_stopping_min_remain = 20;	// 최소 2초 표시 (직접 1→0 전환에도 보이게)
								debugprintf("\n\r [EXT STOPPING] state %d->%d, mode 0x%02x->0x%02x",
									prev_state, final_state, prev_mode, final_mode);
							}
						}
						// 안정 상태 (mode=0, state=0) → 최소 표시 시간 후 clear
						if(final_mode == 0 && final_state == 0 && external_stopping &&
						   external_stopping_min_remain == 0) {
							external_stopping = false;
							debugprintf("\n\r [EXT STOPPING] cleared (stable idle)");
						}

						smart_bed_display.display_refresh = true;
					}
					break;
				}
			}
			esp32_uart.rx_read_pointer += buff[i+LENGTH] + LENGTH+ 3;
			if(esp32_uart.rx_read_pointer == esp32_uart.rx_write_pointer)
				esp32_uart.rx_read_pointer = esp32_uart.rx_write_pointer = 0;
			return true;
		}
	}
	return false;
}

 void esp32_get_info_bar_body(void)
 {
	int rx_leng = 0;
	U8 tmp_buff[RX_MAX_NUM/2] = {0,};
    U16 cur_time = 0;
	U16 pack_leng = 0;
	int i;
	
	rx_leng = uart_getdata(ESP32_UART_CH, tmp_buff, RX_MAX_NUM/2);// uart data get
	if(rx_leng)
	{// 수신된 data가 있으면
		// [디버그 출력 제거] 매 바이트 debugprintf가 고주파 패킷 환경에서
		// UART 블로킹을 일으켜 RX 버퍼 오버플로 가속 — 제거함.
		esp32_uart.rx_flag = true;

		if((esp32_uart.rx_write_pointer + rx_leng) >  RX_MAX_NUM)
        {// [버퍼 오버플로] 기존 코드의 ring 버퍼 wrap memcpy 길이가
			// 잘못 계산되어 큰 영역을 덮어쓰는 버그 — 안전하게 reset 처리
			esp32_uart.rx_read_pointer = 0;
			esp32_uart.rx_write_pointer = 0;
			memcpy(&esp32_uart.rx_buff[0], tmp_buff, rx_leng);
			esp32_uart.rx_write_pointer = rx_leng;
			pack_leng = rx_leng;
        }
        else
        {
            memcpy(&esp32_uart.rx_buff[esp32_uart.rx_write_pointer], tmp_buff, rx_leng);
            esp32_uart.rx_write_pointer += rx_leng;
			pack_leng = esp32_uart.rx_write_pointer - esp32_uart.rx_read_pointer;
        }
		if(pack_leng >= LENGTH)
		{
			esp32_packet_parsing_bar_body(&esp32_uart.rx_buff[esp32_uart.rx_read_pointer], pack_leng);
		}
		esp32_uart.rx_time_count = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		return;
	}
	else
	{// 수신된 data가 없다
		if(time_10msec_interval_get(esp32_uart.rx_time_count) > 100)
		{
			esp32_uart.rx_read_pointer = esp32_uart.rx_write_pointer = 0;
			PRINTLINE;
		}
		else
		{
		}
	}
 }

