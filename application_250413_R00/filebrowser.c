/******************************************************************************
 Copyright (C) 2011      Advanced Digital Chips Inc. 
						http://www.adc.co.kr
 Author : Software Team.

******************************************************************************/
#include "main.h"

static SURFACE* surf_bg;
static int viewstart=0;
static int viewend=0;
static int cursel=0;

#define MAX_FILE_NUM 128
static struct fileinfo
{
	char fname[FF_LFN_BUF+1];
	int fsize;
	bool bDir;
}fileinfolist[MAX_FILE_NUM ];

static int compareName( const void *arg1, const void *arg2 )
{
	struct fileinfo* f1;
	struct fileinfo* f2;
	f1 = (struct fileinfo *)arg1;
	f2 = (struct fileinfo *)arg2;
	if( f1->bDir ^ f2->bDir)
	{
		if(f1->bDir)
			return -1;
		else
			return 1;
	}
	else
	{
		return strcmp( f1->fname, f2->fname);
	}
}

static int totalitemcnt=0;
static int totaldircnt=0;

static void insertfile(char* fname, int size)
{
	if(totalitemcnt>=MAX_FILE_NUM)
		return;
	strcpy(fileinfolist[totalitemcnt].fname,fname);
	fileinfolist[totalitemcnt].fsize = size;
	fileinfolist[totalitemcnt].bDir = false;
	totalitemcnt ++;
	//debugprintf("%s,%d\r\n",fname,size);
}

static void insertdir(char* fname)
{
	if(totalitemcnt>=MAX_FILE_NUM)
		return;
	if(strcmp(fname,".")==0)
		return;
	strcpy(fileinfolist[totalitemcnt].fname,fname);
	fileinfolist[totalitemcnt].bDir = true;
	totalitemcnt ++;
	totaldircnt++;
	//debugprintf("directory:%s\r\n",fname);
}

static FRESULT popluate (char* path)
{
	FRESULT res;
	FILINFO fno;
	DIR dir;

	totaldircnt=totalitemcnt=cursel=viewstart=0;
	res = f_opendir(&dir, path);
	if (res != FR_OK) 
		return res;
	
	for (;;) {
		res = f_readdir(&dir, &fno);
		if (res != FR_OK || fno.fname[0] == 0) break;
		if (fno.fattrib & AM_DIR) {
			insertdir(fno.fname);
		} else {
			insertfile(fno.fname,fno.fsize);
		}
	}
	f_closedir(&dir);
	qsort(&fileinfolist[0],totalitemcnt,sizeof(struct fileinfo),compareName);
	return res;
}

extern EGL_FONT* g_pFont;

static void gui_draw()
{
	int i;
	int y;
	char str[32];
	set_draw_target(getbackframe());	
	draw_surface(surf_bg,0,0);	
	
	for(y=80,i=viewstart;i<totalitemcnt;i++,y+=30)
	{
		if(cursel == i)
		{			
			draw_roundrect(15,y-2,450,30,5,MAKE_COLORREF(100,0,100));			
		}
		bmpfont_draw(g_pFont,20,y,fileinfolist[i].fname);		
		if(fileinfolist[i].bDir == false)
		{
			sprintf(str,"%d byte",fileinfolist[i].fsize);
			bmpfont_draw(g_pFont,500,y,str);			
		}
		else
		{
			bmpfont_draw(g_pFont,500,y,"[D]");						
		}
		if((y+40)>=getscreenheight())
			break;
	}
	viewend = i;

}

static int keyproc(int key)
{
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
	case KEY_ENTER:
		if(fileinfolist[cursel].bDir)
		{
			if(strcmp(fileinfolist[cursel].fname,".")!=0)
			{
				f_chdir(fileinfolist[cursel].fname);
				popluate("");
			}
		}
		break;
	case KEY_EXIT:
		return -1;
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
		//debugprintf("X:%03d, Y:%03d \r\n",x,y);
		if(check_collision(690,90,780,145,x,y))	
		{
			return keyproc(KEY_EXIT);
		}
		else if(check_collision(685,200,776,280,x,y))	
		{
			return keyproc(KEY_UP);
		}
		else if(check_collision(685,312,776,400,x,y))	
		{
			return keyproc(KEY_DOWN);
		}
		else
			valid = false;
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
		else if(ch=='3')
		{
			return keyproc(KEY_ENTER);
		}
		else if(ch=='0')
			return keyproc(KEY_EXIT);
	}
	return 0;
}

void filebrowser_init()
{
	static bool binit=false;
	if(binit)
		return;
	
	binit = true;
	totalitemcnt = 0;
	f_chdir("/icon");
	popluate("");
	surf_bg = loadbmp("filebrowser.bmp");
}

void filebrowser()
{
	filebrowser_init();
	while(1)
	{
		if(proc_key()==-1)
			break;
		gui_draw();
		flip();
	}
	f_chdir("/");
}
