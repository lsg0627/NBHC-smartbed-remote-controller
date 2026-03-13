/******************************************************************************
 Copyright (C) 2011      Advanced Digital Chips Inc. 
						http://www.adc.co.kr
 Author : Software Team.

******************************************************************************/
#include "main.h"

#define SCREEN_X_MIN 52
#define SCREEN_X_MAX 750
#define SCREEN_Y_MIN 32
#define SCREEN_Y_MAX 410

#define RED_X_MIN 60
#define RED_X_MAX 90
#define RED_Y_MIN 430
#define RED_Y_MAX 465

#define GREEN_X_MIN 100
#define GREEN_X_MAX 130
#define GREEN_Y_MIN RED_Y_MIN
#define GREEN_Y_MAX RED_Y_MAX

#define BLUE_X_MIN 160
#define BLUE_X_MAX 190
#define BLUE_Y_MIN RED_Y_MIN
#define BLUE_Y_MAX RED_Y_MAX

#define BLACK_X_MIN 220
#define BLACK_X_MAX 250
#define BLACK_Y_MIN RED_Y_MIN
#define BLACK_Y_MAX RED_Y_MAX

#define sCOLOR_X_MIN 280
#define sCOLOR_X_MAX 310
#define sCOLOR_Y_MIN RED_Y_MIN
#define sCOLOR_Y_MAX RED_Y_MAX

#define WHITE_X_MIN 330
#define WHITE_X_MAX 360
#define WHITE_Y_MIN RED_Y_MIN
#define WHITE_Y_MAX RED_Y_MAX

#define BIG_X_MIN 390
#define BIG_X_MAX 420
#define BIG_Y_MIN RED_Y_MIN
#define BIG_Y_MAX RED_Y_MAX

#define SMALL_X_MIN 440
#define SMALL_X_MAX 470
#define SMALL_Y_MIN RED_Y_MIN
#define SMALL_Y_MAX RED_Y_MAX

#define CLEAR_X_MIN 500
#define CLEAR_X_MAX 530
#define CLEAR_Y_MIN RED_Y_MIN
#define CLEAR_Y_MAX RED_Y_MAX

#define BACK_X_MIN 550
#define BACK_X_MAX 580
#define BACK_Y_MIN RED_Y_MIN
#define BACK_Y_MAX RED_Y_MAX

#define TEST_SIZE 10
U16 paint_size = TEST_SIZE;

extern SURFACE* surf_home;
SURFACE* surf_bg;
static volatile int tick10m=0;
static U16 Sign_cR,Sign_cG,Sign_cB;

void paint_init();
void linedrawex(int x1, int y1, int x2, int y2, int thick);

void TIMER0_ISR_TOUCH()
{
	tick10m++;
}

void linedrawex(int x1, int y1, int x2, int y2, int thick)
{
	int		x, y;
	int		dx, dy;
	int		incx, incy;
	int		balance;
	int pbuf;
	SURFACE* surf = get_draw_target();
	pbuf = (U32)surf->pixels;
	if (x2 >= x1)
	{
		dx = x2 - x1;
		incx = 1;
	}
	else
	{
		dx = x1 - x2;
		incx = -1;
	}

	if (y2 >= y1)
	{
		dy = y2 - y1;
		incy = 1;
	}
	else
	{
		dy = y1 - y2;
		incy = -1;
	}

	x = x1;
	y = y1;

	if(surf->bpp==16)
	{
		draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
	}
	else//32
	{
		draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
	}

	if (dx >= dy)
	{
		dy <<= 1;
		balance = dy - dx;
		dx <<= 1;

		if(surf->bpp==16)
		{
			while (x != x2)
			{

				draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
				if (balance >= 0)
				{
					y += incy;
					balance -= dx;
				}
				balance += dy;
				x += incx;
			} 
			draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
		}
		else //32
		{
			while (x != x2)
			{

				draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
				if (balance >= 0)
				{
					y += incy;
					balance -= dx;
				}
				balance += dy;
				x += incx;
			} 
			draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));

		}
	}
	else
	{
		dx <<= 1;
		balance = dx - dy;
		dy <<= 1;
		if(surf->bpp==16)
		{
			while (y != y2)
			{
				draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
				if (balance >= 0)
				{
					x += incx;
					balance -= dy;
				}
				balance += dx;
				y += incy;
			} 
			draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
		}
		else //32
		{
			while (y != y2)
			{
				draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
				if (balance >= 0)
				{
					x += incx;
					balance -= dy;
				}
				balance += dx;
				y += incy;
			} 
			draw_rectfill(x,y,thick,thick,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
		}
	}
	dcache_invalidate_way();
}

void paint_init()
{
	static int binit=0;
	if(binit)
		return;
	binit=1;	
	surf_bg  = loadbmp("/icon/Signpad.bmp");

	Sign_cR=0;
	Sign_cG=0;
	Sign_cB=0;	
}
void paint_run()
{
	set_draw_target(getbackframe());

	paint_init();
	draw_surface(surf_bg,0,0);
	flip();

	U32 sx,sy;
	U32 sx_past=0,sy_past=0;	

	//	U32 sx_max=0,sx_min=800;
	//	U32 sy_max=0,sy_min=800;

	set_interrupt(INTNUM_TIMER0,TIMER0_ISR_TOUCH);
	set_timer(0,10);

	bool newpath=true;
	int lastdrawtick=0;
	while(1)
	{
		if(get_touchxy(&sx,&sy))
		{
			lastdrawtick = tick10m;

#if 1	// Draw Pad ?

#if 1	// SCREEN DRAW
			if(sx > (SCREEN_X_MIN+paint_size) && sx < (SCREEN_X_MAX-paint_size))
			{
				if(sy > (SCREEN_Y_MIN+paint_size) && sy < (SCREEN_Y_MAX-paint_size))
				{
					if(newpath)
					{
						draw_circlefill(sx,sy,paint_size,MAKE_COLORREF(Sign_cR,Sign_cG,Sign_cB));
						newpath=false;
					}
					else
					{
						linedrawex(sx_past,sy_past,sx,sy,paint_size);
					}

					sx_past = sx;
					sy_past = sy;
				}
			}

#endif	// Draw Pad ?

#if 1	// CONTROL CHECK

			if(sx > RED_X_MIN && sx < RED_X_MAX)
			{
				if(sy > RED_Y_MIN && sy < RED_Y_MAX)
				{
					Sign_cR = 255;
					Sign_cG = 0;
					Sign_cB = 0;

					delayms(250);
					snd_touch();
				}
			}

			if(sx > GREEN_X_MIN && sx < GREEN_X_MAX)
			{
				if(sy > GREEN_Y_MIN && sy < GREEN_Y_MAX)
				{
					Sign_cR = 0;
					Sign_cG = 255;
					Sign_cB = 0;
					
					delayms(250);
					snd_touch();
				}
			}

			if(sx > BLUE_X_MIN && sx < BLUE_X_MAX)
			{
				if(sy > BLUE_Y_MIN && sy < BLUE_Y_MAX)
				{
					Sign_cR = 0;
					Sign_cG = 0;
					Sign_cB = 255;
					
					delayms(250);
					snd_touch();
				}
			}

			if(sx > BLACK_X_MIN && sx < BLACK_X_MAX)
			{
				if(sy > BLACK_Y_MIN && sy < BLACK_Y_MAX)
				{
					Sign_cR = 0;
					Sign_cG = 0;
					Sign_cB = 0;
					
					delayms(250);
					snd_touch();
				}
			}

			if(sx > WHITE_X_MIN && sx < WHITE_X_MAX)
			{
				if(sy > WHITE_Y_MIN && sy < WHITE_Y_MAX)
				{
					Sign_cR = 255;
					Sign_cG = 255;
					Sign_cB = 255;
					
					delayms(250);
					snd_touch();
				}
			}

			if(sx > BIG_X_MIN && sx < BIG_X_MAX)
			{
				if(sy > BIG_Y_MIN && sy < BIG_Y_MAX)
				{
					if(paint_size < 30)
					{
						paint_size+=2;
						delayms(250);
						snd_touch();
					}
				}
			}

			if(sx > SMALL_X_MIN && sx < SMALL_X_MAX)
			{
				if(sy > SMALL_Y_MIN && sy < SMALL_Y_MAX)
				{
					if(paint_size > 10)
					{
						paint_size-=2;
						delayms(250);
						snd_touch();
					}
				}
			}

			if(sx > CLEAR_X_MIN && sx < CLEAR_X_MAX)
			{
				if(sy > CLEAR_Y_MIN && sy < CLEAR_Y_MAX)
				{
					draw_surface(surf_bg,0,0);
					debugstring("Clear LCD\r\n");					
					delayms(250);
					snd_touch();
				}
			}

			if(sx > BACK_X_MIN && sx < BACK_X_MAX)
			{
				if(sy > BACK_Y_MIN && sy < BACK_Y_MAX)
				{
					break;
				}
			}
#endif	// CONTROL CHECK

			// debugprintf("%d,%d\r\n\r\n",sx,sy);

#else	// PRINT - MAX/MIN

			if(sx <= sx_min)
			{
				sx_min = sx;
			}
			if(sy <= sy_min)
			{
				sy_min = sy;
			}

			if(sx >= sx_max)
			{
				sx_max = sx;
			}
			if(sy >= sy_max)
			{
				sy_max = sy;
			}

			debugprintf("S(X,Y) %d,%d  ",sx,sy);
			debugprintf("SX(MIN,MAX) %d,%d  ",sx_min,sx_max);
			debugprintf("SY(MIN,MAX) %d,%d\r\n",sy_min,sy_max);			
#endif
		}

		if(lastdrawtick+10 < tick10m)
		{
			newpath = true;
		}

		char ch;
		if(uart_getch(0,&ch))
		{
			if(ch=='0')
			{
				break;
			}
		}
	}
	f_chdir("/");
	stop_timer(0);
}
