/******************************************************************************
 Copyright (C) 2011      Advanced Digital Chips Inc. 
						http://www.adc.co.kr
 Author : Software Team.

******************************************************************************/
#include "main.h"


static SURFACE* surf_rec;
static SURFACE* surf_rec_dis;
static SURFACE* surf_bg;
extern SURFACE* surf_play ; //mp3main.c
extern SURFACE* surf_play_dis ;//mp3main.c
extern SURFACE* surf_home ;
static RECT rect_play={10,300,128,128};
static RECT rect_rec={150,300,128,128};

static RECT rect_home={660,0,128,128};

#define SAMPLE_RATE	(101250000/2/256/13)//SAMPLE per SEC
#define SAMPLE_COUNT (SAMPLE_RATE*5) // 5sec 

extern EGL_FONT* g_pFont;

int samplecnt=0;
U16* samples ;
enum state
{
	STATUS_READY=0,
	STATUS_REC_START,
	STATUS_REC,
	STATUS_REC_DONE,
	STATUS_PLAY_START,
	STATUS_PLAY,
};
volatile static enum state stage = STATUS_READY;
static WAVE *pWav = 0;
void adc_isr()
{
	while(1)
	{
		U32 status = *R_ADCSTAT;
		if((status & (1<<4))==0)//fifo not empty
		{
			U16 ADC_Result;
			ADC_Result = *R_ADCFIFO;
			ADC_Result &= 0x3ff;
			samples[samplecnt]=(ADC_Result<<6);
			samplecnt++;
			if(samplecnt>SAMPLE_COUNT-10)
			{
				*R_ADCCTRL = 0;
				stage = STATUS_REC_DONE;
				debugstring("Record done \r\n");
				break;
			}
		}
		else
			break;
	}
}

static int proc_key()
{
	int x;
	int y;
	if(get_touchxy(&x,&y))
	{
		debugprintf("X:%03d, Y:%03d \r\n",x,y);
		if(check_collisionrect(&rect_rec,x,y))	
		{
			if( stage == STATUS_READY)
			{
				samplecnt = 0;
				debugstring("Record ....\r\n");
				*R_ADCCTRL = *R_ADCCTRL = F_ADCCTRL_CH3 | F_ADCCTRL_PMS |F_ADCCTRL_APB256 | F_ADCCTRL_EN | F_ADCCTRL_FIFO;;
				stage = STATUS_REC_START;
			}
		}
		else if(check_collisionrect(&rect_play,x,y))	
		{
			if(stage == STATUS_READY)
			{
				if(samplecnt>0)
				{
					pWav = sound_loadrawp(samples,SAMPLE_RATE,PCMTYPE_UM16,samplecnt*2);
					if(pWav)
					{
						debugstring("play....\r\n");
						sound_play(pWav);
						stage = STATUS_PLAY_START;
					}
				}
			}
		}
		else if(check_collisionrect(&rect_home,x,y))				
		{			
			return -1;
		}

	}
	return 0;
}

void soundrec_init()
{
	static bool bInit=false;
	samplecnt=0;
	if(bInit)
		return;
	bInit = true;
	set_interrupt(INTNUM_ADC,adc_isr);
	enable_interrupt(INTNUM_ADC,TRUE);
	samples = (U16*)malloc(SAMPLE_COUNT * 2);
	//load image
	f_chdir("icon");
	surf_rec = loadtga("Actions-media-record-icon.tga");
	surf_rec_dis = loadtga("Actions-media-record-icon-disable.tga");
	surf_bg = loadbmp("/Microphone-bg.bmp");
	f_chdir("..");
	rect_rec.w=surf_rec->w;
	rect_rec.h=surf_rec->h;
}

#define WAVE_X 10
#define WAVE_W 280

static void wav_draw_bg()
{
	draw_rectfill(WAVE_X,34,WAVE_W,260,MAKE_COLORREF(128,128,128));//erase wave	
	int y;
	y = 34+65;
	draw_line(WAVE_X,y,WAVE_X+WAVE_W-1,y,MAKE_COLORREF(0,0,255));
	y=34+130;
	draw_line(WAVE_X,y-1,WAVE_X+WAVE_W-1,y-1,MAKE_COLORREF(0,0,255));
	draw_line(WAVE_X,y,WAVE_X+WAVE_W-1,y,MAKE_COLORREF(0,0,255));
	draw_line(WAVE_X,y+1,WAVE_X+WAVE_W-1,y+1,MAKE_COLORREF(0,0,255));
	y = 34+130 + 65;
	draw_line(WAVE_X,y,WAVE_X+WAVE_W-1,y,MAKE_COLORREF(0,0,255));

}
static void wav_draw()
{
	SURFACE* targetsurface = getfrontframe();
	int x;
	U8 y;
	int i;
	static U8 olddata[WAVE_W]={0,};
	static U16 oldcolor[WAVE_W];
	if(samplecnt>WAVE_W+1)
	{
		for(x=0,i=samplecnt-WAVE_W;x<WAVE_W;i++,x++)
		{
			y = olddata[x];
			*(U16*)(((U32)targetsurface->pixels)+(WAVE_X*2)+ (x*2)+(targetsurface->pitch*y))=oldcolor[x];
			U16 data  =samples[i];
			y = data>>8;
			y+=37;
			oldcolor[x] = *(U16*)(((U32)targetsurface->pixels)+(WAVE_X*2)+ (x*2)+(targetsurface->pitch*y));
			*(U16*)(((U32)targetsurface->pixels)+(WAVE_X*2)+ (x*2)+(targetsurface->pitch*y))=MAKE_RGB565(255,255,255);
			olddata[x]=y;
		}
	}
	dcache_invalidate_way();
}

void soundrec_run()
{
	soundrec_init();
	
	set_draw_target(getbackframe());
	
	draw_rectfill(0,0,getscreenwidth(),getscreenheight(),MAKE_COLORREF(255,255,255));
	draw_surface(surf_bg,300,130);
	
	bmpfont_draw(g_pFont, 400, 40, "Voice Recoder");	
	draw_surface(surf_home,rect_home.x,rect_home.y);
	wav_draw_bg();
	bmpfont_draw(g_pFont, 50, 430, "Recording for 5 sec");	
	flip();
	set_draw_target(getfrontframe());
	stage = STATUS_READY;
	while(1)
	{
		if(stage == STATUS_READY)
		{
			if(proc_key()==-1)
				break;
		}

		switch(stage)
		{
		case STATUS_READY:
			if(samplecnt>0)
				draw_surface(surf_play,rect_play.x,rect_play.y);
			else
				draw_surface(surf_play_dis,rect_play.x,rect_play.y);
			draw_surface(surf_rec,rect_rec.x,rect_rec.y);			
			break;
		case STATUS_PLAY_START:
			bmpfont_draw(g_pFont, 50, 430, "Recording for 5 sec");				
			draw_surface(surf_rec_dis,rect_rec.x,rect_rec.y);
			draw_surface(surf_play_dis,rect_play.x,rect_play.y);
			stage = STATUS_PLAY;
			break;
		case STATUS_PLAY:
			if(sound_isplay(pWav)==false)
			{
				debugstring("Play Done\r\n");
				free(pWav);//samples was allocated by user, don't use sound_release
				pWav = 0;
				stage = STATUS_READY;
			}
			break;
		case STATUS_REC_START:
			bmpfont_draw(g_pFont, 50, 430, "Recording for 5 sec");				
			draw_surface(surf_rec_dis,rect_rec.x,rect_rec.y);
			draw_surface(surf_play_dis,rect_play.x,rect_play.y);
			stage = STATUS_REC;
			break;
		case STATUS_REC:
			wav_draw();
			break;
		case STATUS_REC_DONE:
			wav_draw_bg();
			stage = STATUS_READY;
			break;
		}
	}
}
