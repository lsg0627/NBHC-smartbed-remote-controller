
// self test
1. motor board
2. main board
3. remocon board


void selftest(void)
{
    selftest_motor_drive();
    selftest_esp32();
    selftest_remocon();
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

U8 cmd_id
U8 acd_id
U8

U8 esp32_packet_parsing(U8 *pack_buff, U8 length, U8 *cmd, U8 *data_buff)
{
    int i = 0;
    U16 stx;
    U32 pcode;  // "9B1C"

    while(1)
    {
        memcpy(&stx, &buff[i], 2);
        if(stx == STX_CMD)
            break;
        i++;
        
        if(i >= length)
            return 0xFF;
        if((length-i) < ESP32_PACK_NUM)
            return 0xFF;
    }
}
//
// packet receive
// If no data is received for 50 msec, the value is discarded and the pointer is initialized.
//
bool esp32_packet_receive(void)
{
    int rx_leng;
    U8 tmp_buff[RX_MAX_NUM/2] = {0,};
    U16 cur_time = 0;

    rx_leng = uart_getdata(ESP32_UART_CH, tmp_buff, RX_MAX_NUM/2);
    if(rx_leng)
    {
        esp32_uart.rx_time_count = *((volatile unsigned int*)TMCNT_ADDR(SYS_TIMER_CH));// get timer count
		esp32_uart.rx_flag = true;
        if((esp32_uart.rx_write_pointer + rx_len) >  RX_MAX_NUM)
        {
            memcpy( &esp32_uart.rx_buff[esp32_uart.rx_write_pointer], tmp_buff, (RX_MAX_NUM - esp32_uart.rx_write_pointer) );
            memcpy( esp32_uart.rx_buff, &tmp_buff[RX_MAX_NUM - esp32_uart.rx_write_pointer], (esp32_uart.rx_write_pointer- rx_leng) );
            esp32_uart.rx_write_pointer = (esp32_uart.rx_write_pointer- rx_leng);
        }
        else
        {
            memcpy(&sp32_uart.rx_buff[rx_write_pointer], tmp_buff, rx_leng);
            esp32_uart.rx_write_pointer = rx_leng;
        }

        if(esp32_uart.packet_status == 0)
        {// command
            if(esp32_uart.rx_write_pointer > esp32_uart.rx_read_pointer)
            {
                if((esp32_uart.rx_write_pointer > esp32_uart.rx_read_pointer) >= (ESP32_PACK_NUM -2))
                    esp32_packet_cmd_parsing(&esp32_uart.rx_buff[rx_read_pointer], &cmd_id, &act_id, &data_leng);
            }
            else
            {

            }
        }
        else
        {// data

        }
        
        if(esp32_uart.rx_write_pointer > RX_MAX_NUM)
		if(esp32_uart.rx_pointer >=  ESP32_PACK_NUM)
			esp32_packet_parsing(&esp32_uart.rx_buff[rx_read_pointer],  
    }
    else
    {// If no data is received for 50 msec, the value is discarded and the pointer is initialized.
        if(rx_flag)
        {// If received data?
            cur_time = time_1msec_interval_get();
            if(cur_time > RX_MAX_TIME)
            {
                esp32_uart.rx_pointer = 0;
                esp32_uart.rx_flag = flase;
            }
        }
    }
}