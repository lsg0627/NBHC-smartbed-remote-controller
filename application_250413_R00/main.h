#pragma  once
#include "adstar.h"
#include "etc_driver/DS1307.h"
#include "etc_driver/AK4183.h"

bool check_collision(int startx, int starty,int endx, int endy,int x, int y);
bool check_collisionrect(RECT* pRect,int x, int y);

void filebrowser_init();
void filebrowser();

void mp3run_init();
void mp3run();

void timeview_init();
void timeview_run();

void paint_init();
void paint_run();

void soundrec_init();
void soundrec_run();

void weather_init();
void weather_run();

enum 
{
	KEY_NONE=0,
	KEY_EXIT,
	KEY_DOWN,
	KEY_UP,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_ENTER,
	KEY_PAUSE,
	KEY_VOL,
	KEY_PLAY,
	KEY_STOP,
};


void snd_fail();
void snd_ok();
void snd_touch();
void snd_systemon();


