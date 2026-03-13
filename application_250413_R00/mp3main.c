/******************************************************************************
 Copyright (C) 2011      Advanced Digital Chips Inc. 
						http://www.adc.co.kr
 Author : Software Team.

******************************************************************************/
#include "main.h"

extern SURFACE* surf_home;

#define MAX_FILE_NUM 128

static SURFACE* mybg ;
static SURFACE* surf_ipod ;
static SURFACE* surf_vol[4] ;
SURFACE* surf_play ; //used by voice record 
SURFACE* surf_play_dis ;//used by voice record 
static SURFACE* surf_pause ;
static SURFACE* surf_pause_dis ;
SURFACE* surf_stop ;//used by voice record 
SURFACE* surf_stop_dis ;//used by voice record 
static SURFACE* surf_up ;
static SURFACE* surf_down ;
static SURFACE* surf_temp ;
static WAVE* mp3=0;
static int curvol= 3;

static RECT rect_up={280,100,64,64};
static RECT rect_down={280,230,64,64};
static RECT rect_vol={40,380,64,64};
static RECT rect_play={160,350,128,128};
static RECT rect_play_dis={160,350,128,128};
static RECT rect_pause={310,350,128,128};
static RECT rect_pause_dis={310,350,128,128};
static RECT rect_stop={460,350,128,128};
static RECT rect_stop_dis={460,350,128,128};

static RECT rect_home={660,0,128,128};
char fnames[MAX_FILE_NUM ][FF_LFN_BUF+1];
static bool bintro=true;

extern EGL_FONT* g_pFont;
extern EGL_FONT* g_pFontHan;

static int compareName( const void *arg1, const void *arg2 )
{
	return strcmp( (const char*)arg1, (const char*)arg2);
}

static int totalitemcnt=0;

static void insertfile(char* fname)
{
	if(totalitemcnt+1>MAX_FILE_NUM)
		return;
	strcpy(&fnames[totalitemcnt][0],fname);
	totalitemcnt ++;
}

static FRESULT popluate (char* path)
{
	FRESULT res;
	FILINFO fno;
	DIR dir;
	int i;
	char *fn;
	totalitemcnt=0;
	res = f_opendir(&dir, path);
	if (res == FR_OK) {
		i = strlen(path);
		for (;;) {
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0) break;
			fn = fno.fname;
			if (fno.fattrib & AM_DIR) {
				//insertdir(fn);
			} else {
				insertfile(fn);
			}
		}
	}
	f_closedir(&dir);
	qsort(fnames,totalitemcnt,FF_LFN_BUF+1,compareName);
	return res;
}

static int viewstart=0;
static int viewend=0;
static int cursel=0;
#define LIST_START_X 80
#define LIST_START_Y 100
static void gui_draw()
{
	int i;
	int y;
	char shortname[32];
	draw_surface(mybg,0,0);
	draw_surface(surf_ipod,0,50);
	for(y=LIST_START_Y,i=viewstart;i<totalitemcnt;i++,y+=30)
	{
		if((cursel == i)&& (bintro==false))
		{
			draw_roundrectfill(LIST_START_X-1,y+1,160,30-6,5,MAKE_COLORREF(100,0,100));			
		}
		
		if(strlen(&fnames[i][0])>12)
		{
			strncpy(shortname,&fnames[i][0],7);
			strcpy(&shortname[7],"~.mp3");
			bmpfont_draw(g_pFontHan,LIST_START_X,y,shortname);			
		}
		else
			bmpfont_draw(g_pFontHan,LIST_START_X,y,&fnames[i][0]);			
		if(y>=(LIST_START_Y+6*30))
			break;
	}
	viewend = i;
	draw_surface(surf_home,rect_home.x,rect_home.y);
	draw_surface(surf_up,rect_up.x,rect_up.y);
	draw_surface(surf_down,rect_down.x,rect_down.y);
	draw_surface(surf_vol[curvol],rect_vol.x,rect_vol.y);
	if((sound_isplay(mp3)==TRUE)||(sound_ispause(mp3)==TRUE))
		draw_surface(surf_stop,rect_stop.x,rect_stop.y);
	else
		draw_surface(surf_stop_dis,rect_stop_dis.x,rect_stop_dis.y);
		
	if(sound_isplay(mp3)==TRUE)
	{
		draw_surface(surf_play_dis,rect_play_dis.x,rect_play_dis.y);
		draw_surface(surf_pause,rect_pause.x,rect_pause.y);
	}
	else
	{
		draw_surface(surf_play,rect_play.x,rect_play.y);
		draw_surface(surf_pause_dis,rect_pause_dis.x,rect_pause_dis.y);
	}
}

static void setvol(U8 lev)
{
	switch(lev)
	{
	case 0:
		sound_vol(0);
		break;
	case 1:
		sound_vol(210);
		break;
	case 2:
		sound_vol(230);
		break;
	case 3:
		sound_vol(255);
		break;
	}
}
static int keyproc(int key)
{
	static int direct=1;//incre

	switch(key)
	{
	case KEY_DOWN:
		if(cursel<(totalitemcnt-1))
		{
			cursel++;
			if(cursel>=viewend)
				viewstart++;
		}
		break;
	case KEY_UP:
		if(cursel>0)
		{
			cursel--;
			if(viewstart>cursel)
				viewstart--;
		}
		break;
	case KEY_STOP:
		if((sound_isplay(mp3)==TRUE)||(sound_ispause(mp3)==TRUE))
		{
			debugstring("stop...\r\n");
			sound_stop(mp3);
			sound_release(mp3);
			mp3=0;
		}
		break;
	case KEY_PLAY:
		if((sound_isplay(mp3)==false)&&(sound_ispause(mp3)==false))
		{
			if(mp3)
			{
				sound_release(mp3);
				mp3=0;
			}
			debugstring("play...\r\n");
			mp3 = sound_loadmp3(&fnames[cursel][0]);
			if(mp3)
				sound_play(mp3);
		}
		else if(sound_ispause(mp3)==TRUE)
		{
			sound_resume(mp3);
		}
		break;
	case KEY_PAUSE:
		if(sound_isplay(mp3))
		{
			debugstring("pause...\r\n");
			sound_pause(mp3);
		}
		break;
	case KEY_VOL:
		
		if(direct)
		{
			if(curvol>=3)
			{
				direct=0;
				curvol=2;
			}
			else
				curvol++;
		}
		else
		{
			if(curvol==0)
			{
				direct=1;
				curvol=1;
			}
			else
				curvol--;

		}
		setvol(curvol);
		break;
	case KEY_EXIT:
		if(mp3)
		{
			sound_stop(mp3);
			sound_release(mp3);
			mp3=0;
		}
		return -1;
		break;
	}
	return 0;
}
static int proc_key()
{
	char ch;
	int x,y;
	if(get_touchxy(&x,&y))
	{
		bool valid = true;
		debugprintf("X:%03d, Y:%03d \r\n",x,y);
		if(check_collisionrect(&rect_down,x,y))	
			return keyproc(KEY_DOWN);
		else if(check_collisionrect(&rect_up,x,y))	
			return keyproc(KEY_UP);
		else if(check_collisionrect(&rect_play,x,y))	
			return keyproc(KEY_PLAY);
		else if(check_collisionrect(&rect_pause,x,y))	
			return keyproc(KEY_PAUSE);
		else if(check_collisionrect(&rect_stop,x,y))	
			return keyproc(KEY_STOP);
		else if(check_collisionrect(&rect_vol,x,y))	
			return keyproc(KEY_VOL);
		else if(check_collisionrect(&rect_home,x,y))	
			return keyproc(KEY_EXIT);
		else
		{
			valid = false;

		}
		if(valid)
			snd_ok();
		else
			snd_fail();
	}
	if(uart_getch(0,&ch))
	{
		if(ch=='1')
		{
			return keyproc(KEY_DOWN);
		}
		else if(ch=='2')
		{
			return keyproc(KEY_UP);
		}
		else if(ch=='3') //select
		{
			return keyproc(KEY_PLAY);
		}
		else if(ch=='4')
		{
			return keyproc(KEY_PAUSE);
		}
		else if(ch=='5')
		{
			return keyproc(KEY_STOP);
		}
		else if(ch=='6')
		{
			return keyproc(KEY_VOL);
		}
		else if(ch=='0')
		{
			return keyproc(KEY_EXIT);
		}
			
	}
	else 
		return 0;
	return 1;
}

void mp3run_init()
{
	static bool binit=false;
	f_chdir("/mp3");
	if(binit)
		return;
	surf_temp = create_surface(getscreenwidth(),getscreenheight(),getscreenbpp());
	binit = true;
	mybg= loadbmp("/mp3bg.bmp");
	f_chdir("/icon");
	surf_ipod = loadtga("ipod-touch.tga");
	surf_vol[0]= loadtga("vol0.tga");
	surf_vol[1]= loadtga("vol1.tga");
	surf_vol[2]= loadtga("vol2.tga");
	surf_vol[3]= loadtga("vol3.tga");

	surf_play = loadtga("Button-Play-icon.tga");
	surf_play_dis = loadtga("Button-Play-icon-disable.tga");
	surf_pause = loadtga("Button-Pause-icon.tga");
	surf_pause_dis = loadtga("Button-Pause-icon-disable.tga");
	surf_stop = loadtga("Button-Stop-icon.tga");
	surf_stop_dis = loadtga("Button-Stop-icon-disable.tga");
	surf_down= loadtga("Button-Download-icon.tga");
	surf_up = loadtga("Button-Upload-icon.tga");

	f_chdir("/mp3");
	popluate("");
	f_chdir("..");
}

static U32 myframe=0;

static bool intro_effect()
{
	if(myframe==0)
	{
		SURFACE* front = getfrontframe();
		dma_memcpy(surf_temp->pixels,front->pixels,front->h*front->pitch);
	}
	int k;
	CLIP_RECT rect;
	k = myframe*60;
	if(k<surf_temp->h)
	{
		rect.x = 0; rect.y = 0; rect.endx = getscreenwidth(); rect.endy = getscreenheight();
		draw_set_clip_window(getbackframe(),&rect);
		draw_surface_rect(surf_temp,0,0,0,k,surf_temp->w,surf_temp->h-k);
		rect.x = 0; rect.y = getscreenheight()-k; rect.endx = getscreenwidth(); rect.endy = rect.y + k;
		draw_set_clip_window(getbackframe(),&rect);		
		return true;
	}
	else
	{		
		rect.x = 0; rect.y = 0; rect.endx = getscreenwidth(); rect.endy = getscreenheight();
		draw_set_clip_window(getbackframe(),&rect);
		draw_set_clip_window(getfrontframe(),&rect);
		return false;
	}
}

void mp3run()
{
	myframe=0;
	
	egl_font_set_color(g_pFontHan,MAKE_COLORREF(255,255,255));

	mp3run_init();
	while(1)
	{
		set_draw_target(getbackframe());

		if(bintro==false)
		{
			if(proc_key()==-1)
				break;
		}
		if(bintro)
			bintro = intro_effect();
		gui_draw();
		flip();
		myframe++;
	}
	f_chdir("/");
	bintro=true;	
}

