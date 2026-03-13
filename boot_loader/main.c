#include "adstar.h"
#include "etc_driver/misc.h"
#include "../drive/smart_bed_remocon.h"
#include "../drive/lcd.h"

#define FAT_APP_TYPE DRIVE_NAND
//#define FAT_APP_TYPE DRIVE_SDCARD

#undef CONFIG_USBHOST 
#define CONFIG_USBHOST  0

//for code size
#undef CONFIG_CODE_PAGE
#define CONFIG_CODE_PAGE 437
//#include "../../lib_src/fatfs/option/ccsbcs.c"
#include "../../lib_src/fatfs/diskio.c"
#include "../../lib_src/fatfs/ff.c"
#include "../../lib_src/fatfs/ffunicode.c"
#include "../../lib_src/sdcard.c"

#if CONFIG_USBHOST == 1

#include "../../lib_src/nandctrl.c"
#include "../../lib_src/usb_host/usb_host.c"
#include "../../lib_src/usb_host/usb_ohci.c"
#include "../../lib_src/usb_host/usb_storage.c"

#else

#include "../../lib_src/nandctrl.c"

#endif

EGL_FONT* g_pFontbit;

INCBIN(dwn_image, "dwn.png");
INCBIN(mass_image, "mass.png");
SURFACE* dwn_image;
SURFACE* mass_image;

SURFACE *frame ;
SURFACE *frame2;

extern unsigned int __firmware_version;

void RSP_Run();
void mass_storage_main();

static void bin_execute_fat()
{
	FATFS fs;
	FIL f;
#if CONFIG_USBHOST == 1
	//f_mount(DRIVE_USBHOST,&fs);	
	f_mount(&fs, "2:", 1);
	f_chdrive("2:");	
	usb_clock_hostinit();		
	if(!UsbHostInit())
		debugprintf("USB Host Init Fail~~\r\n");		
	while(1)
	{
		if(GetUsbStorageDetected()) 
		{
			debugprintf("USB Memory Detected!\r\n");								
			break;
		}
	}
	debugprintf("USB Memory Detected!\r\n");								
	debugstring("Execute Mode,Run boot.bin from USB FAT \r\n");	
#else
#if	FAT_APP_TYPE == DRIVE_NAND
	f_mount(&fs, "0:", 1);
	debugstring("Execute Mode,Run boot.bin from NAND FAT \r\n");
#else
	f_mount(&fs, "1:", 1);
	f_chdrive("1:");
	debugstring("Execute Mode,Run boot.bin from SDCARD FAT \r\n");
#endif
#endif
	U32* buf = (U32* )0x20000000;
	if(FR_OK!= f_open(&f,"boot.bin",FA_READ|FA_OPEN_EXISTING))
	{
		debugstring("failed to found boot.bin\r\n");
		while(1);
	}
	int fsize = f_size(&f);
	U32 nRead;
	f_read(&f,buf,fsize,&nRead);
	f_close(&f);
	dcache_invalidate_way();
	void (*entryfp)();
	entryfp = (void(*)())buf[0]; 
	debugprintf("startfp : %#x\r\n",entryfp);
	entryfp();
}

#if CONFIG_USBHOST == 1                                     // use USB Host.
	#define FLASH_APP_OFFSET  (1024*4*28)     // download address 0x1C000
#else
	#define FLASH_APP_OFFSET  (1024*4*20)     // download address 0x14000
#endif

static void bin_execute()
{
	void (*entryfp)();
	debugstring("Execute Mode,Run boot.bin from FLASH \r\n");
	memcpy((void*)0x20000000,(void*)FLASH_APP_OFFSET,(512*1024)-FLASH_APP_OFFSET);
	dcache_invalidate_way();
	entryfp = (void(*)())*(U32*)0x20000000; 
	debugprintf("startfp : %#x\r\n",entryfp);
	entryfp();
}

#define NOR_FLASH_MODE	0x04
#define NAND_FLASH_MODE	0x0C
#define DEVELOPER_MODE		0x08
int main()
{
	U8 boot_mode;
	SURFACE *frame ;
	SURFACE *frame2;

	int dwn_image_size = dwn_image_end - dwn_image_data;
	dwn_image = loadpngp((U8*)dwn_image_data, dwn_image_size);

	int mass_image_size = mass_image_end - mass_image_data;
	mass_image = loadpngp(mass_image_data, mass_image_size);
	
	smart_bed_remocon_port_init();
	uart_config(0,115200,DATABITS_8,STOPBITS_1,UART_PARNONE);
	boot_mode = get_smart_bed_remocon_boot_mode();
	debugstring("\n\r================================================\r\n");
	debugprintf("= Smart Bed Remocon [ %s %s ]\r\n",__DATE__, __TIME__);
	debugprintf("= boot program version[%s]\n\r", (char *)&__firmware_version);
	debugprintf("= boot mode[0x%02x]\n\r", boot_mode);
	debugstring("==================================================\r\n");
	

	if(usb_get_detection() == true){// usb detected
		switch(boot_mode){
			case DEVELOPER_MODE:
				SPI_init();
				LCD_Init();
				crtc_clock_init();//12MHz
				frame = createframe(320,480,16);
				frame2 = createframe(320,480,16);
				setscreenex(320, 480, SCREENMODE_RGB565, (320+80), (3 << 16) | 8, (16 << 16) | (320+16), (480+20), (3 << 16) | 5, (15<< 16) | (480+15));
				setdoubleframebuffer(frame,frame2);
				set_draw_target(getbackframe());
				draw_rectfill(0,0,320, 480, MAKE_COLORREF(0,0,0));
				draw_surface(dwn_image, 0,0);
				flip();
				LCD_ON();
				
				_USB_DISABLE();
				delayms(100);
				_USB_ENABLE();
				RSP_Run();// develop mode
				break;
			default:// mass storage mode
				SPI_init();
				LCD_Init();
				crtc_clock_init();//12MHz
				frame = createframe(320,480,16);
				frame2 = createframe(320,480,16);
				setscreenex(320, 480, SCREENMODE_RGB565, (320+80), (3 << 16) | 8, (16 << 16) | (320+16), (480+20), (3 << 16) | 5, (15<< 16) | (480+15));
				setdoubleframebuffer(frame,frame2);
				set_draw_target(getbackframe());
				draw_rectfill(0,0,320, 480, MAKE_COLORREF(0,0,0));
				draw_surface(mass_image, 0, 0);
				flip();
				LCD_ON();
				
				_USB_DISABLE();
				delayms(100);
				_USB_ENABLE();
				delayms(100);
				usb_clock_init();
				mass_storage_main();
		}
	}
	switch(boot_mode){
		case NAND_FLASH_MODE:
			bin_execute_fat();// nand flash 
			break;
		case NOR_FLASH_MODE:
			bin_execute();
	}
	while(1);
	return 0;
}
