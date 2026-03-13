#include "adstar.h"

#define LCD_RESET_LOW() *R_GPOLOW(5) = (1<<3) // GP5.3
#define LCD_RESET_HIGH() *R_GPOHIGH(5) = (1<<3)	// GP5.3
#define LCD_SPI_CS_ENABLE() *R_GPOLOW(4) = (1<<7)	// GP4.7
#define LCD_SPI_CS_DISABLE() *R_GPOHIGH(4) = (1<<7)	// GP4.7
#define LCD_SPI_CMD() *R_GPOLOW(5) = (1<<2)	// GP5.2
#define LCD_SPI_DATA() *R_GPOHIGH(5) = (1<<2)	// GP5.2

bool spi_init_flag = false;
//DISPLAY lcd_screen;;

bool SPI_init(void){
	U8 tmp;
	
	if(spi_init_flag == true) return true;
	
	//spi_master_init(1);
	if(spi_set_freq(1, SPI_MASTER, 500000) == false) PRINTLINE;
	*R_SPI0CON(1) = SPICON_EN | SPICON_MSTR | SPICON_MSBF; // MODE 0
	LCD_SPI_CS_DISABLE();//__--
	
	tmp = *R_SPI0DATA(1);
	spi_init_flag = true;
	return true;
}

void spi_wait_finish (int ch)
{
	U8 stat;
	do {
		stat = *R_SPI0STAT(ch);
	} while (!(stat & SPISTAT_SPIF));
}

void LCD_SPI_Write_CMD(U8 lcd_cmd){
	U8 tmp;
	
	LCD_SPI_CMD();//--__
	delayms(5);
	LCD_SPI_CS_ENABLE();//--__
	delayms(5);
	*R_SPI0DATA(1) = lcd_cmd;
	spi_wait_finish(1);
	tmp = *R_SPI0DATA(1);
	LCD_SPI_CS_DISABLE();//__--
	delayms(5);
}


void LCD_SPI_Write_DATA(U8 lcd_data){
	U8 tmp;
	
	LCD_SPI_DATA();//__--
	delayms(5);
	LCD_SPI_CS_ENABLE();//--__
	delayms(5);
	*R_SPI0DATA(1) = lcd_data;
	spi_wait_finish(1);
	tmp = *R_SPI0DATA(1);
	LCD_SPI_CS_DISABLE();//__--
		delayms(5);
	
}

// DRIVER IC : ILI9488
// CONTORL I/F : SPI(4wire)
// GRAPHIC I/F : RGB565(16bit)
// LCD RESULTION : 320 x 480
void LCD_Init(void){
	
	LCD_RESET_LOW();//--__
	delayms(200);
	LCD_RESET_HIGH();//__--
	delayms(100);
	LCD_SPI_Write_CMD(0xC0);
	LCD_SPI_Write_DATA(0x18);
	LCD_SPI_Write_DATA(0x16);
	
	LCD_SPI_Write_CMD(0xC1);
	LCD_SPI_Write_DATA(0x41);
	
	LCD_SPI_Write_CMD(0xC5);
	LCD_SPI_Write_DATA(0x00);
	LCD_SPI_Write_DATA(0x21);
	LCD_SPI_Write_DATA(0x80);
	
	LCD_SPI_Write_CMD(0x36);
	LCD_SPI_Write_DATA(0x8);
	
	LCD_SPI_Write_CMD(0x3A);// Interface Mode Control
	LCD_SPI_Write_DATA(0x55); //16BIT 0x66 18bit
	
	LCD_SPI_Write_CMD(0XB0); //Interface Mode Control
	LCD_SPI_Write_DATA(0x02);
	LCD_SPI_Write_CMD(0xB1); //Frame rate 70HZ
	LCD_SPI_Write_DATA(0xA0);
	
	LCD_SPI_Write_CMD(0xB4);
	LCD_SPI_Write_DATA(0x00);
	
	LCD_SPI_Write_CMD(0xB6); //RGB/MCU Interface Control
	//LCD_SPI_Write_DATA(0xB0); //0x32 RGB 0x02 MCU
	LCD_SPI_Write_DATA(0xB2); //0x32 RGB 0x02 MCU
	LCD_SPI_Write_DATA(0x20);
	
	
	LCD_SPI_Write_CMD(0xE9);
	LCD_SPI_Write_DATA(0x00);
	
	LCD_SPI_Write_CMD(0XF7);
	LCD_SPI_Write_DATA(0xA9);
	LCD_SPI_Write_DATA(0x51);
	LCD_SPI_Write_DATA(0x2C);
	LCD_SPI_Write_DATA(0x82);
	
	LCD_SPI_Write_CMD(0x11);
	delayms(50);
	LCD_SPI_Write_CMD(0x13);
	LCD_SPI_Write_CMD(0x29);
	delayms(120);
}


