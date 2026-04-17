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

        // FAN
        buff = 0;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_VENTIL, 1, &buff);
        delayms(10000);// 10sec delay 
        buff = 1;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_VENTIL, 1, &buff);
        delayms(10000);// 10sec delay 
        buff = 2;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_VENTIL, 1, &buff);
        delayms(10000);// 10sec delay 
        buff = 3;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_VENTIL, 1, &buff);
        delayms(10000);// 10sec delay 
        buff = 0;
        esp32_packet_send(CMD1_SEND_RUN_ST,CMD2_VENTIL, 1, &buff);

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
					
					memcpy(bar, &buff[i+LENGTH+1], buff[LENGTH]);
					//esp32_uart.rx_read_pointer += buff[i+LENGTH] + LENGTH+ 3;
					smart_bed_display.display_refresh = true;
					for(tmp=0; tmp<buff[LENGTH]; tmp++)
						debugprintf("->0x%x ", bar[tmp]);
					debugprintf("\n\r r%d  w%d", esp32_uart.rx_read_pointer, esp32_uart.rx_write_pointer);
					PRINTLINE;
					break;
				case CMD1_GET_PRESSURE_MAP:	// pressure map (7x10) - 임시 비활성
#if 0
					if(buff[i+LENGTH] >= PRESSURE_MAP_SIZE)
						memcpy(pressure_map, &buff[i+LENGTH+1], PRESSURE_MAP_SIZE);
					debugprintf("\n\r pressure_map r%d w%d", esp32_uart.rx_read_pointer, esp32_uart.rx_write_pointer);
					PRINTLINE;
					smart_bed_display.display_refresh = true;
#endif
					break;
				case CMD1_GET_BODY_INFO:	// body information
					if(buff[i+LENGTH+1] == 0x30)
						body_info = true;
					else body_info = false;
					//esp32_uart.rx_read_pointer += buff[i+LENGTH] + LENGTH+ 3;
					for(tmp=0; tmp<buff[LENGTH]; tmp++)
						debugprintf("->0x%x ", bar[tmp]);
					debugprintf("\n\r r%d  w%d", esp32_uart.rx_read_pointer, esp32_uart.rx_write_pointer);
					PRINTLINE;
					smart_bed_display.display_refresh = true;
					break;
				case CMD1_SEND_RUN_ST:	// 동작상태 에코 ACK
					if(buff[ACTI_D + i] == CMD2_STDBY){
						stdby_complete = true;
						running_massage_type = -1;
						debugprintf("\n\r ACK: STDBY COMPLETE");
					}
					else if(buff[ACTI_D + i] >= CMD2_MASSAGE && buff[ACTI_D + i] < (CMD2_MASSAGE + MASSAGE_MAX)){
						running_massage_type = buff[ACTI_D + i] - CMD2_MASSAGE;
						debugprintf("\n\r ACK: MASSAGE %d", running_massage_type + 1);
					}
					else if(buff[ACTI_D + i] == CMD2_FALL_ALERT){
						smart_bed_status.status = MODE_FALL_ALERT;
						smart_bed_display.status = MODE_FALL_ALERT;
						smart_bed_display.display_refresh = true;
						debugprintf("\n\r *** FALL ALERT ***");
					}
					else if(buff[ACTI_D + i] == CMD2_FALL_CLEAR){
						smart_bed_status.status = MODE_HOME;
						smart_bed_display.status = MODE_HOME;
						smart_bed_display.display_refresh = true;
						debugprintf("\n\r FALL ALERT CLEARED");
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
					debugprintf("\n\r [BED_STATUS] len=%d mode=0x%02x state=%d pwr=%d",
						dlen, buff[i+LENGTH+1], buff[i+LENGTH+2], buff[i+LENGTH+8]);
					if(dlen >= sizeof(BED_STATUS_DATA)){
						memcpy(&bed_status, &buff[i + LENGTH + 1], sizeof(BED_STATUS_DATA));
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
		for(i=0; i<rx_leng; i++) debugprintf("\n\r 0x%x", tmp_buff[i]);
		esp32_uart.rx_flag = true;
		//rx_cnt += rx_leng;
		
		if((esp32_uart.rx_write_pointer + rx_leng) >  RX_MAX_NUM)
        {// max buffer size over
			pack_leng = (esp32_uart.rx_write_pointer + rx_leng)  - esp32_uart.rx_read_pointer;
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
			pack_leng = esp32_uart.rx_write_pointer - esp32_uart.rx_read_pointer;
			debugprintf("\n\r pack_leng[%d] w[%d] r[%d]", pack_leng, esp32_uart.rx_write_pointer , esp32_uart.rx_read_pointer);
			PRINTLINE;
        }
		if(pack_leng >= LENGTH)
		{
			PRINTLINE;
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

