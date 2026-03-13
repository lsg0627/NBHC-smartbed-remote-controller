/******************************************************************************
 Copyright (C) 2011      Advanced Digital Chips Inc. 
						http://www.adc.co.kr
 Author : Software Team.

******************************************************************************/

#include "adstar.h"
#include <time.h>
#include "etc_driver/DS1307.h"

#ifndef DS1307_SET_HEX
#define DS1307_SET_DEC
#else
#warning DS1307_SET_HEX
#endif

S32 DS1307_init(void)
{
	S32 wCnt;
	
	U8 wbuf[8]={0,0,0,0,0,0,0,0};
	
	wCnt = DS1307_Write(DS1307_ADDR_SEC,wbuf,8);
	
	if(wCnt != 8)
	{
		return -1;
	}
	
	return 0;
}
S32 DS1307_SetTime(DS1307_Time* pDT)
{
	S32 wCnt;
	// U8 wbuf[3]={0,0,0};
	U8 wbuf[7]={0,0,0,0,0,0,0};
	
#ifdef DS1307_SET_HEX
	wbuf[0] = pDT->sec&0x7F;	
	wbuf[1] = pDT->min&0x7F;	
	wbuf[2] = pDT->hour&0x7f;
	
	wbuf[3] = pDT->day&0x7f;
	wbuf[4] = pDT->date&0x7f;
	wbuf[5] = pDT->month&0x7f;
	wbuf[6] = pDT->year&0x7f;
	
#endif
	
#ifdef DS1307_SET_DEC
	wbuf[0] = (pDT->sec/10)<<4;
	wbuf[0] += (pDT->sec%10);
	
	wbuf[1] = (pDT->min/10)<<4;
	wbuf[1] += (pDT->min%10);
	
	wbuf[2] = (pDT->hour/10)<<4;
	wbuf[2] += (pDT->hour%10);
	
	
	wbuf[3] = (pDT->day/10)<<4;
	wbuf[3] += (pDT->day%10);
	
	wbuf[4] = (pDT->date/10)<<4;
	wbuf[4] += (pDT->date%10);
	
	wbuf[5] = (pDT->month/10)<<4;
	wbuf[5] += (pDT->month%10);
	
	wbuf[6] = (pDT->year/10)<<4;
	wbuf[6] += (pDT->year%10);
	
#endif
	
	wCnt = DS1307_Write(DS1307_ADDR_SEC,wbuf,7);
	if(wCnt != 7)
	{
		return -1;
	}
	
	return 0;
}
S32 DS1307_GetTime(DS1307_Time* pDT)
{
	S32 rCnt;
	// U8 rbuf[3];
	U8 rbuf[7];
	rCnt = DS1307_Read(DS1307_ADDR_SEC,rbuf,7);
	if(rCnt != 7)
	{
		return -1;
	}
	
#ifdef DS1307_SET_HEX
	pDT->sec = rbuf[0]&0x7F;
	pDT->min = rbuf[1]&0x7F;
	pDT->hour =rbuf[2]&0x7F;
	
	pDT->day = rbuf[3]&0x7F;
	pDT->date = rbuf[4]&0x7F;
	pDT->month = rbuf[5]&0x7F;
	pDT->year = rbuf[6]&0x7F;
#endif
	
#ifdef DS1307_SET_DEC
	pDT->sec = rbuf[0]&0xF;
	pDT->sec += (rbuf[0]>>4)*10;
	
	pDT->min = rbuf[1]&0xF;
	pDT->min += (rbuf[1]>>4)*10;
	
	pDT->hour = rbuf[2]&0xF;
	pDT->hour += (rbuf[2]>>4)*10;
	
	pDT->day = rbuf[3]&0xF;
	pDT->day += (rbuf[3]>>4)*10;
	
	pDT->date = rbuf[4]&0xF;
	pDT->date += (rbuf[4]>>4)*10;
	
	pDT->month = rbuf[5]&0xF;
	pDT->month += (rbuf[5]>>4)*10;
	
	pDT->year = rbuf[6]&0xF;
	pDT->year += (rbuf[6]>>4)*10;
#endif	
	return 0;
}

S32 DS1307_Write(U8 addr,U8 *buffer, S32 length)
{
	// Write Pointer
	U32 i;	
    U32 wCnt=0;
	
	// twi_wait_busy();	// failed 4 off
	
	*R_TWIDATA = DS1307_DEVICE_ADDR;
	*R_TWICTRL = F_TWICTRL_TWIEN | F_TWICTRL_TWIMOD_MSTART | F_TWICTRL_TWITR_MTRM;	// Start Condition
    if(FALSE == twi_trans_complete())
	{
		return -1;
	}

	*R_TWIDATA = addr;
    if(FALSE == twi_trans_complete())
	{
		return -1;
	}

	// Write Data + N
    for (i=0; i<length; i++)
	{
		*R_TWIDATA = buffer[i];
		// PRINTVAR(buffer[i]);
        if(FALSE == twi_trans_complete())
		{
			return -1;
		}
		
        wCnt++;
    }

	*R_TWICTRL = F_TWICTRL_TWIEN;	// Stop Condtion
    
	if(FALSE == twi_wait_busy())	// on
	{
		return -1;
	}
	
    return wCnt;
}
S32 DS1307_Read(U8 addr,U8 *buffer, S32 length)
{
	// Write Pointer, Then Read
	
    U32 i;
    U32 rCnt=0;
    U16 status;
	
	// Write Pointer
	*R_TWIDATA = DS1307_DEVICE_ADDR;
	*R_TWICTRL = F_TWICTRL_TWIEN | F_TWICTRL_TWIMOD_MSTART | F_TWICTRL_TWITR_MTRM;
    if(FALSE == twi_trans_complete())
	{
		return -1;
	}
	
	*R_TWIDATA = addr;
    if(FALSE == twi_trans_complete())
	{
		return -1;
	}
	
	// Read Data
	*R_TWIDATA = DS1307_DEVICE_ADDR+1;
	*R_TWICTRL = F_TWICTRL_TWIEN | F_TWICTRL_TWIMOD_MSTART | F_TWICTRL_REPST_RSTART;
	if(FALSE == twi_trans_complete())
	{
		return -1;
	}

	status  = *R_TWIDATA;

	for ( i = 0; i < length-1; i++ )
	{
		if(FALSE == twi_trans_complete())
		{
			return -1;
		}
		buffer[i] = *R_TWIDATA;
		rCnt++;
	}

	*R_TWICTRL = F_TWICTRL_TWIEN | F_TWICTRL_TWIMOD_MSTART | F_TWICTRL_TWIAK_NACK;
	
	if(FALSE == twi_trans_complete())
	{
		return -1;
	}
    buffer[i] = *R_TWIDATA;
    rCnt++;
	
	*R_TWICTRL = F_TWICTRL_TWIEN;
	
    if(FALSE == twi_wait_busy())
	{
		return -1;
	}
	
	return rCnt;
}

