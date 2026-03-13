/******************************************************************************
 Copyright (C) 2011      Advanced Digital Chips Inc. 
						http://www.adc.co.kr
 Author : Software Team.

******************************************************************************/
#include "main.h"
#include "etc_driver/misc.h"
#include "../drive/smart_bed_remocon.h"
#include "../drive/lcd.h"

extern unsigned int __firmware_version;

SURFACE *frame ;
SURFACE *frame2;

int main()
{
U16 k;
	
	smart_bed_remocon_port_init();
	uart_config(0,115200,DATABITS_8,STOPBITS_1,UART_PARNONE);
	debugstring("\n\r================================================\r\n");
	debugprintf("= Smart Bed Remocon [ %s %s ]\r\n",__DATE__, __TIME__);
	debugprintf("= [%s]\n\r", (char *)&__firmware_version);
	debugstring("==================================================\r\n");
	
// LCD Set
	SPI_init();
	LCD_Init();
	crtc_clock_init();//12MHz
	key_init();

	FATFS fs;
	f_mount(&fs,"0:",1);
	
	frame = createframe(320,480,16);
	frame2 = createframe(320,480,16);

	// H Total : Syn Start : Sync End : Act Start : Act End
	//setscreenex(320, 480, SCREENMODE_RGB565, (320+80), (3 << 16) | 4, (7 << 16) | (320+80), (480+20), (3 << 16) | 5, (7<< 16) | (480+20));// HVSYNC 60Hz(12MHz)
		setscreenex(320, 480, SCREENMODE_RGB565, (320+80), (3 << 16) | 8, (16 << 16) | (320+16), (480+20), (3 << 16) | 5, (15<< 16) | (480+15));// HVSYNC 60Hz(12MHz)
	//setscreenex(320, 480, SCREENMODE_RGB565, (320+80), (5 << 16) | 30, (80 << 16) | (320+80), (480+20), (5 << 16) | 15, (17<< 16) | (480+20));// DE
	
	setdoubleframebuffer(frame,frame2);
	
	//set_draw_target(frame);
	image_load();
	load_font();
	//LCD_ON();

	esp32_com_init();
	init_value();
	levitate_value_power_init();
	dispersion_value_power_init();
	massage_value_power_init();
	patient_care_value_power_init();
	heat_value_power_init();
	ventilation_value_power_init();
	esp32_get_info_init();
	sys_timer_set();
	while(1){
		// Timer event handler
		process_target_time_handler();
		// UART even handler
		
		// KEY event handler
		key_read();
		// key input & uart input 
		process_analy_data();
		
		esp32_get_info_bar_body();
		
		// IMAGE event handler
		progress_lcd_display();
		
	}
	return 0;
}

