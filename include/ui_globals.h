#pragma once


typedef struct {	
	u16 tmout_10msec;
	u16 tmout_100msec;
	u16 tmout_250msec;
	u16 tmout_1sec;
	u16 tmout_1min;
	
}target_time_t;

typedef struct{
	//device_md_t		device_md;
	target_time_t	target_time;

 } device_control_t;


extern device_control_t DC;
extern device_control_t *pDC;

extern void init_value(void);

