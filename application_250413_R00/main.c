/******************************************************************************
 Copyright (C) 2011      Advanced Digital Chips Inc. 
						http://www.adc.co.kr
 Author : Software Team.

******************************************************************************/
#include "main.h"
#include "etc_driver/misc.h"
#include "../drive/smart_bed_remocon.h"
#include "../drive/lcd.h"
#include "../drive/key.h"

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
	//set_draw_target(getbackframe());
	//set_draw_target(frame);
	SURFACE *bg1 = loadbmp("bt1.bmp");
	SURFACE *bg2 = loadbmp("bt2.bmp");

	LCD_ON();
	led_on(1);
	while(1){
		// Timer event handler
		process_target_time_handler();
		// UART even handler
		
		// KEY event handler
		key_read();
		// IMAGE event handler
		
	}
	set_draw_target(getbackframe());
	draw_rectfill(0,0,320, 480, MAKE_COLORREF(0,0,0));
	draw_surface(bg1,0,0);
	// draw_rect(0,0,320, 480, MAKE_COLORREF(200,0,0));
	// draw_rectfill(0,0,100, 100, MAKE_COLORREF(255,10,10));
	// draw_rectfill(100,0,100, 100, MAKE_COLORREF(2,255,10));
	// draw_rectfill(0,100,100, 100, MAKE_COLORREF(255,255,10));
	// draw_rectfill(100,100,100, 100, MAKE_COLORREF(255,0,255));
	flip();
	while(1){
		key_read();
	}
	delayms(1000);

	set_draw_target(getbackframe());
	draw_surface(bg2,0,0);
	// draw_rectfill(0,0,320, 480, MAKE_COLORREF(0,0,0));
	// draw_rect(0,0,320, 480, MAKE_COLORREF(200,0,0));
	// draw_rectfill(0,0,100, 100, MAKE_COLORREF(0,10,255));
	// draw_rectfill(100,0,100, 100, MAKE_COLORREF(255,20,255));
	// draw_rectfill(0,100,100, 100, MAKE_COLORREF(255,0,0));
	// draw_rectfill(100,100,100, 100, MAKE_COLORREF(2,255,2));
		flip();
	delayms(1000);
	}
	while(1);
#if 0
	crtc_clock_init();
	
	PRINTVAR(get_pll(0));
	PRINTVAR(get_pll(1));

	twi_set_freq (100000);

	touchinit();
	FATFS fs;
	f_mount(&fs,"0:",1);
		
	f_chdir("font");	
	g_pFont = create_bmpfont("nanum_outline.fnt");
	bmpfont_setautokerning(g_pFont, true);
	if(!g_pFont)	
	{
		debugprintf("Error BitMap Font \r\n");
	}
	bmpfont_settextencoding(g_pFont, NONE);
	f_chdir("..");

	lcdon();

	sound_init();
	//AMP ON
	AMP_ON;

	SURFACE* frame ;
	SURFACE* frame2 ;
	/*	
	frame = createframe(800,480,32);
	frame2 = createframe(800,480,32);
	setscreenex(800, 480, SCREENMODE_RGB888, 862, (16 << 16) | 40, (62 << 16) | 862, 510, (7 << 16) | 16, (30 << 16) | 510);
	*/
	frame = createframe(800,480,16);
	frame2 = createframe(800,480,16);
	//setscreenex(800, 480, SCREENMODE_RGB565, 862, (16 << 16) | 40, (62 << 16) | 862, 510, (7 << 16) | 16, (30 << 16) | 510);
	setscreenex(800, 480, SCREENMODE_RGB565, 862, (16 << 16) | 40, (62 << 16) | 862, 510, (7 << 16) | 16, (30 << 16) | 510);
	setdoubleframebuffer(frame,frame2);
	set_draw_target(frame);
	
	SURFACE* bg = loadbmp("bg.bmp");
	if(bg==0)
	{
		debugprintf("Load bg.jpg error\r\n");
		while(1);
	}

	drawsurface(bg,0,0);
	bmpfont_draw(g_pFont, 100, 100, "Loading...");
	bmpfont_draw(g_pFont, 100, 150, "Without Any OS, You can do it with adStar...");
	bmpfont_draw(g_pFont, 100, 400, "Advanced Digital Chips Inc. http://www.adc.co.kr");
	
	f_chdir("font");
	g_pFontHan = create_bmpfont("batang_20.fnt");
	bmpfont_setautokerning(g_pFontHan, true);
	egl_font_set_color(g_pFontHan, MAKE_COLORREF(255, 255, 255));	
	f_chdir("..");

	wav_systemon = sound_loadwav("wav/System_On.wav");
	wav_fail= sound_loadwav("wav/Fail.wav");
	wav_ok= sound_loadwav("wav/OK.wav");
	wav_touch= sound_loadwav("wav/Touch.wav");
	wav_key = sound_loadwav("weather/key.wav");

	if(getscreenbpp()!=16)
	{
		WAVE* mp3test = sound_loadmp3("mp3/1 Bubble Pop!.mp3");
		if(mp3test)
		{
			mp3test->bLoop=1;
			sound_play(mp3test);
		}
		bmpfont_draw(g_pFontHan, 100, 50, "RGB888 Mode 에서는 이 프로그램을 실행 할 수 없습니다. (메모리 부족)");		
		while(1);

	}
	
	SURFACE* surf_mp3 = loadtga("icon/ipod-64.tga");
	mp3_rect.w = surf_mp3->w;
	mp3_rect.h = surf_mp3->h;
	SURFACE* surf_control = loadtga("icon/Control-Panel-icon.tga");
	SURFACE* surf_exp = loadtga("icon/Explorer.tga");
	SURFACE* surf_paint = loadtga("icon/Paint-icon.tga");
	surf_home = loadtga("icon/Button-Reload-icon.tga");
	SURFACE* surf_box = loadtga("icon/box-icon.tga");

	SURFACE* surf_con_sub[5];
	surf_con_sub[0]=loadtga("icon/con_1.tga");
	surf_con_sub[1]=loadtga("icon/con_2.tga");
	surf_con_sub[2]=loadtga("icon/con_3.tga");
	surf_con_sub[3]=loadtga("icon/con_4.tga");
	surf_con_sub[4]=loadtga("icon/con_5.tga");
	
	filebrowser_init();
	mp3run_init();
	paint_init();
	
#if 0  // weather API missing.
	weather_init();
#endif
	
	SURFACE* ver_surf = bmpfont_makesurface(g_pFontHan, "Version: "DEMO_VERSION);
	egl_font_set_color(g_pFontHan,MAKE_COLORREF(255,255,0));
	SURFACE* marquee_surf = bmpfont_makesurface(g_pFontHan,"adStar는 SDRAM, Flash, LCD Controller가 내장되어 정보를 LCD에 Display하고자 하는 Application에 최적화된 General MCU제품입니다.");

	snd_systemon();

	int x=0;
	int y=0;
#define DD 2
	int ix=DD;
	int iy=DD;
	int marqueex=800;
	
    CLIP_RECT rect;		
	
	while(1)
	{
		proc_key();
		set_draw_target(getbackframe());
		if(bcontrol_mode || (control_mode_frame>0)) //open mode
		{
			if(bcontrol_mode==false)
			{

				if(control_mode_frame>0)
				{
					control_mode_frame-=10;
					if(control_mode_frame<0)
						control_mode_frame=0;
				}
			}
			else
			{
				if(control_mode_frame<90)
					control_mode_frame+=10;
			}
			draw_surface_rect(bg,0,0,0,0,bg->w,300);;			
			draw_rectfill(0,300,800,control_mode_frame,MAKE_COLORREF(33,151,201));			
			rect.x = 0;	rect.y = 300; rect.endx = 800; rect.endy = rect.y+control_mode_frame;
			draw_set_clip_window(getbackframe(), &rect);			
			draw_surface(surf_box,enabled_sub_item*100,300);
			draw_surface(surf_con_sub[0],20,318);
			draw_surface(surf_con_sub[1],117,318);
			draw_surface(surf_con_sub[2],217,318);
			draw_surface(surf_con_sub[3],317,318);
			draw_surface(surf_con_sub[4],417,318);
			rect.x = 0;	rect.y = 0; rect.endx = getscreenwidth(); rect.endy = getscreenheight();
			draw_set_clip_window(getbackframe(), &rect);
			draw_set_clip_window(getfrontframe(), &rect);
			draw_surface_rect(bg,0,300+control_mode_frame,0,300,bg->w,180-control_mode_frame);;
			
		}
		else if(control_mode_frame>0) //closed mode
		{
			draw_surface_rect(bg,0,0,0,0,bg->w,300);;			
			draw_rectfill(0,300,800,control_mode_frame,MAKE_COLORREF(0,0,0));
			draw_surface_rect(bg,0,300+control_mode_frame,0,300,bg->w,180-control_mode_frame);;
			control_mode_frame-=10;
			if(control_mode_frame<0)
				control_mode_frame=0;
		}
		else
		{
			draw_surface(bg,0,0);
		}
		draw_surface(surf_control,control_rect.x,control_rect.y);
		draw_surface(surf_exp,exploer_rect.x,exploer_rect.y);
		draw_surface(surf_paint,paint_rect.x,paint_rect.y);

		bmpfont_draw(g_pFont,100,400,"Advanced Digital Chips Inc, http://www.adc.co.kr");

		char strtime[128];
		DS1307_Time curtime;
		DS1307_GetTime(&curtime);
		sprintf(strtime,"현재시간 %2d 시 %d 분 %d 초",curtime.hour,curtime.min, curtime.sec);
		
		egl_font_set_color(g_pFontHan,MAKE_COLORREF(255,255,255));
		bmpfont_draw(g_pFontHan, 500, 50, strtime);
	
		draw_surface(ver_surf,620,455);
		
		draw_surface(marquee_surf,marqueex,430);
		marqueex-=5;
		if(marqueex+marquee_surf->w  < 0)
		{
			marqueex=800;
		}
		
		mp3_rect.x=x;
		mp3_rect.y=y;
		draw_surface(surf_mp3,x,y);

		flip();
		x+=ix;
		y+=iy;
		if((x+surf_mp3->w)>=getscreenwidth())
		{
			x = getscreenwidth()-surf_mp3->w;
			ix=-DD;
		}
		else if(x<0)
		{
			ix=DD;
			x = 0;
		}
		if((y+surf_mp3->h)>=getscreenheight())
		{
			y = getscreenheight()-surf_mp3->h;
			iy=-DD;
		}
		else if(y<0)
		{
			y=0;
			iy=DD;
		}
	}
#endif
	return 0;

}

