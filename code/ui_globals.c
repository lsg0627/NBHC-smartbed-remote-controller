
#include "../app/main.h"
#include "../include/ui_globals.h"

device_control_t DC;
device_control_t *pDC;

void init_value(void)
{
	pDC = &DC;
	memset(pDC, 0, sizeof(device_control_t));
	
	memset(&smart_bed_display, 0, sizeof(SMART_BED_DISP_STATUS));
	memset(&smart_bed_status, 0, sizeof(SMART_BED_STATUS));
	smart_bed_display.status = 0xFF;
}
 
