#include "adstar.h"

#define RSP_SUPPORT_NAND 1
//#define UART_MODE //default is USB

#define DBG_LOG debugprintf

#define RSPVERSION 100
#define R_CMD_RSPV "rspv"
#define MAX_PACKET_SIZE (1024*64)

#ifndef UART_MODE

const U8 Cfg_Descp[] = {
	0x09,		        // bLength: Size of descriptor
	0x02,				// bDescriptorType: Configuration	
	0x24,0x00,		// wTotalLength: Cfg+Ifc+Class+Ep = 46 bytes
	0x01,				// bNumInterfaces: 1 interface
	0x01,				// bConfigurationValue: 1
	0x00,				// iConfiguration: none
	0x40,				// bmAttributes: Self powered 
	0x32,				// MaxPower: 100mA
	//----------------------------------------------------------------------
	// INTERFACE DESCRIPTOR
	//----------------------------------------------------------------------
	0x09,				// bLength: Size of descriptor
	0x04,				// bDescriptorType: Interface
	0x00,				// bInterfaceNumber: #1
	0x00,				// bAlternateSetting: #0
	0x02,				// bNumEndpoints: 4
	0x00,				// bInterfaceClass: HID-class
	0x00,				// bInterfaceSubClass: none
	0x00,				// bInterfaceProtocol: none
	0x00,				// iInterface: none
	//----------------------------------------------------------------------
	// ENDPOINT 1 DESCRIPTOR  (BULK OUT)
	//----------------------------------------------------------------------
	0x09,				// bLength: Size of descriptor
	0x05,				// bDescriptorType: Endpoint
	0x01,				// bEndpointAddress: OUT, EP1
	0x02,				// bmAttributes: Bulk
	EP1_LEN,0x00,		// wMaxPacketSize: 
	0x01,	            // bInterval: 1ms  , ignored for Bulk
	0x00,0x00,			// Ü§??? ?? dummy L?. WriteEP0Ö®ß¡Ü­ Ã³Ø® ?Ò¢ Ý±Õµ? Õµ?Ø¦ ?ßº?. 

	//----------------------------------------------------------------------
	// ENDPOINT 2 DESCRIPTOR  (BULK IN)
	//----------------------------------------------------------------------
	0x09,				// bLength: Size of descriptor
	0x05,				// bDescriptorType: Endpoint
	0x82,				// bEndpointAddress: IN, EP2
	0x02,				// bmAttributes: Bulk
	EP2_LEN,0x00,		// wMaxPacketSize: 
	0x01,	            // bInterval: 1ms, ignored for Bulk 
	0x00,0x00
};
const U8 Dev_Descp[] = {
	DEV_LEN,	        // bLength: Size of descriptor
	0x01,				// bDescriptorType: Device	
	0x00,0x01,			// bcdUSB: USB 1.1
	0x00,				// bDeviceClass: none
	0x00,				// bDeviceSubClass: none
	0x00,				// bDeviceProtocol: none
	EP0_LEN,			// bMaxPacketSize0: 16 bytes
	0xDC,0x0A,			// idVendor: 0x0ADC 
	0x21,0x00,  // idProduct: 
	0x01,0x00,			// bcdDevice: device release
	0x00,				// iManufacturer: 
	0x00,				// iProduct: 
	0x00,				// iSerialNumber: none
	0x01				// bnumconfigurations: 1
};

static void usb_off()
{
	*(volatile unsigned int*)0x80023c00= 0x7fff;//write enable
	*(volatile unsigned int*)0x80023C20 &= ~(1<<3);//USB clock off
	*(volatile unsigned int*)0x80023C34= 0x0;//off
	*(volatile unsigned int*)0x80023c00= 0;
}

int WaitUSBBulkData(int timeout)
{
	BYTE ep_irq ;
	BYTE usb_irq ;
	int i=0;
	int re=-1;
	while(1)
	{    
		ep_irq  = *(volatile U8*)(__USBEI_REG);
		usb_irq = *(volatile U8*)(__USBUI_REG);


		if(ep_irq & 1)//EP0_IRQ
		{
			ep0_isr();
		}
		if (ep_irq & (1<<1))//EP1_IRQ
		{
			re = 1;
			break;
		}

		if(usb_irq & USB_RESET_IRQ)
		{
			usb_reset();
		}
		else if ( usb_irq & USB_RESUME_IRQ )
		{
			usb_resume();
		}
		else if ( usb_irq & USB_SUSPEND_IRQ )
		{
			usb_suspend();
		}
		if(timeout != -1)
			i++;
		if(i>timeout)
			break;	
		*(volatile U8*)__USBEI_REG = ep_irq;   // Clear Interrupt 
		*(volatile U8*)__USBUI_REG = usb_irq;  // Clear Interrupt

	}
	*(volatile U8*)__USBEI_REG = ep_irq;   // Clear Interrupt 
	*(volatile U8*)__USBUI_REG = usb_irq;  // Clear Interrupt

	return re;
}
#endif
bool rsp_channel_init()
{
#ifdef UART_MODE
	uart_config(1,115200,DATABITS_8,STOPBITS_1,UART_PARNONE);
	return true;
#else
	usb_clock_init();
	usb_set_description((BYTE*)Cfg_Descp,(BYTE*)Dev_Descp);
	usb_init();
	return true;
#endif
}


int get_char(char* ch)
{
#ifdef UART_MODE	
	while(1)
	{
		if(uart_getch(1,ch))
			break;
	}
	return 1;
#else
	static int usbdatalen=0;
	static int usbdatacurp=0;
	static U8 usbdatabuf[MAX_PACKET_SIZE];
	if(usbdatacurp<usbdatalen)
	{
		*ch = usbdatabuf[usbdatacurp];
		usbdatacurp++;
	}
	else
	{
		while(WaitUSBBulkData(-1)==-1);
		usbdatalen = read_usb(usbdatabuf);
		*ch = usbdatabuf[0];
		usbdatacurp=1;
	}
	return 1;
#endif
}

int put_char(char ch)
{
#ifdef UART_MODE	
	uart_putch(1,ch);
	return 1;
#else
	write_usb(&ch,1);
	return 1;
#endif
}

int put_string(char* str,int len)
{
#ifdef UART_MODE	
	int i;
	for(i=0;i<len;i++)
		put_char(str[i]);
#else
	write_usb(str,len);
#endif	
	return 0;
}



static const char *DIGITS = "0123456789abcdef";

#define ER_NUM_MEM_READ 1
#define ER_NUM_MEM_WRITE 2
#define ER_NUM_PACKET 3
#define ER_NUM_HBREAK_WRITE 4
#define ER_NUM_HBREAK_READ 5
#define ER_NUM_HWATCH_WRITE 6
#define ER_NUM_HWATCH_READ 7

#define BRKPT_INTRCUTION 0xe0c0

enum GDB_REG
{
	GDB_USP=16,
	GDB_ISP,
	GDB_SSP,
	GDB_ML,
	GDB_MH,
	GDB_MRE,
	GDB_ER,
	GDB_LR,
	GDB_PC,
	GDB_SR,
	GDB_CR0,
	GDB_CR1
};

static int hex (char ch)
{
	if ((ch >= 'a') && (ch <= 'f'))
		return (ch - 'a' + 10);
	if ((ch >= '0') && (ch <= '9'))
		return (ch - '0');
	if ((ch >= 'A') && (ch <= 'F'))
		return (ch - 'A' + 10);
	return (-1);
}
/*
static int hex2string(char *ptr, char* dest)
{
int i;
int len;
len = strlen(ptr);
if(len&1)
return 0;
for(i=0;i<len;)
{
dest[i/2]=(hex(ptr[i])<<4)+hex(ptr[i+1]);
i+=2;
}
dest[i/2]=0;//null terminate
return len/2;
}

static int hex2int(char *ptr, int *intValue)
{
int numChars = 0;
int hexValue;

*intValue = 0;

while (*ptr)
{
hexValue = hex(*ptr);
if (hexValue >=0)
{
*intValue = (*intValue <<4) | hexValue;
numChars ++;
}
else
break;

ptr++;
}

return (numChars);
}
*/
static char* mem2hex(BYTE* mem,char* buf,int count)
{
	int i;
	BYTE ch;
	for (i=0;i<count;i++) {
		ch = *mem;
		*buf++ = DIGITS[ch >> 4];
		*buf++ = DIGITS[ch & 0xf];
		mem++;
	}
	return(buf);
}

static BYTE* hex2mem(char *buf,BYTE *mem,int count)
{
	int i;
	unsigned char ch;
	int x;

	for (i=0;i<count;i++) {
		ch = hex(*buf++) << 4; 
		x  =  hex(*buf++);	
		ch = ch + x;
		*mem = ch;
		mem++;
	}
	return(mem);
}


/* Convert BUFFER, binary data at least LEN bytes long, into escaped
binary data in OUT_BUF.  Set *OUT_LEN to the length of the data
encoded in OUT_BUF, and return the number of bytes in OUT_BUF
(which may be more than *OUT_LEN due to escape characters).  The
total number of bytes in the output buffer will be at most
OUT_MAXLEN.  */

static int remote_escape_output (const BYTE *buffer, int len,
	BYTE *out_buf, int *out_len,
	int out_maxlen)
{
	int input_index, output_index;

	output_index = 0;
	for (input_index = 0; input_index < len; input_index++)
	{
		BYTE b = buffer[input_index];

		if (b == '$' || b == '#' || b == '}' || b == '*')
		{
			/* These must be escaped.  */
			if (output_index + 2 > out_maxlen)
				break;
			out_buf[output_index++] = '}';
			out_buf[output_index++] = b ^ 0x20;
		}
		else
		{
			if (output_index + 1 > out_maxlen)
				break;
			out_buf[output_index++] = b;
		}
	}

	*out_len = input_index;
	return output_index;
}



/* Convert BUFFER, escaped data LEN bytes long, into binary data
in OUT_BUF.  Return the number of bytes written to OUT_BUF.
Raise an error if the total number of bytes exceeds OUT_MAXLEN.

This function reverses remote_escape_output.  It allows more
escaped characters than that function does, in particular because
'*' must be escaped to avoid the run-length encoding processing
in reading packets.  */

static int	remote_unescape_input (const BYTE *buffer, int len,	BYTE *out_buf, int out_maxlen)
{
	int input_index, output_index;
	int escaped;

	output_index = 0;
	escaped = 0;
	for (input_index = 0; input_index < len; input_index++)
	{
		BYTE b = buffer[input_index];

		if (output_index + 1 > out_maxlen)
			printf("Received too much data from the target.");

		if (escaped)
		{
			out_buf[output_index++] = b ^ 0x20;
			escaped = 0;
		}
		else if (b == '}')
			escaped = 1;
		else
			out_buf[output_index++] = b;
	}

	if (escaped)
		printf("Unmatched escape character in target response.");

	return output_index;
}


int getpkt(char* packet_buffer)
{
	char checksum;
	char xmitcsum;
	int  count;
	char ch;

	do {
		/* wait around for the start character, ignore all other characters */
		while(1)
		{
			int rev = get_char(&ch);
			if(rev < 0)
				return -1;
			else if(rev ==0)
				return 0;
			if( (ch&0x7f) == '$')
				break;
			else if((ch&0x7f)=='\003')//remote interrupt
			{
				return 0;
			}
		}
		checksum = 0;
		xmitcsum = 0;
		count = 0;

		/* now, read until a # or end of buffer is found */
		while (count < MAX_PACKET_SIZE) {
			if(get_char(&ch)==-1)
				return 0;
			if (ch == '#') break;
			checksum += ch;

			packet_buffer[count] = ch;
			count = count + 1;
		}
		packet_buffer[count] = 0;
		if (ch == '#') {
			if(get_char(&ch)==-1)
				return 0;
			xmitcsum = (char)hex(ch & 0x7f) << 4;
			if(get_char(&ch)==-1)
				return 0;
			xmitcsum += (char)hex(ch& 0x7f);
			if (checksum != xmitcsum ) {
				put_char('-');  /* failed checksum *///bug?
			}
			else
			{
				put_char('+');  /* successful transfer */
			}
		}
	} while (checksum != xmitcsum);
	return count;
}

int putpkt(char* packetdata,int len)
{
	char reply;
	BYTE checksum=0;
	int i;
	for(i=0;i<len;i++)
	{
		checksum += packetdata[i];
	}
	while(1)
	{
		put_char('$');
		if(len>0)
			put_string( packetdata,len);
		put_char('#');
		put_char(DIGITS[(checksum>>4) & 0xf]);
		put_char(DIGITS[checksum & 0xf]);
		if(get_char(&reply)==-1)
			return 0;
		if(reply == '+')
			break;
		else if(reply == '-')
			continue;
		else if(reply == 0x3) //ctrl+c
			break;
		else if(reply == '$')
			break;//new packet?
		else
			return 0;//connection closed?
		PRINTLINE;

	}
	return 1;
}


static int send_error(int err)
{
	char str[128];
	sprintf(str,"E%02d",err);
	return putpkt(str,strlen(str));
}


void rsp_proc_file_io(char* packetbuf)
{

}

//HW description

static const int dram_size=1024*1024*16;
static const U32 dram_start_addr=0x20000000;


int memorywrite(U32 addr,BYTE* buf,int len)
{
	memcpy((void*)addr,buf,len);
	return 1;
}
int memoryread(U32 addr,BYTE* buf,int len)
{
	memcpy(buf,(void*)addr,len);
	return 1;
}




#define VCMD_FLASH_ERASE "FlashErase:"
#define VCMD_FLASH_WRITE "FlashWrite:"
#define VCMD_FLASH_READ "FlashRead:"
#define VCMD_FLASH_DONE "FlashDone"
#define VCMD_RUNAT	"RunAt"
#define VCMD_NAND_INIT "NandInit:" 
#define VCMD_NAND_ERASEBLOCK "NandErase:" //startblock,blockcnt
#define VCMD_NAND_WRITEPAGE "NandWritePage:"//startpage,pagecnt
#define VCMD_NAND_READPAGE "NandReadPage:"//startpage,pagecnt

void rsp_proc_v(char* packet,int packetsize)
{
	int sectorcnt;
	int startsector;
	int sectorsize;
	//static int flashwritelen;//initialize at flash_erase command and used by flash_write
	//FlashErase
	packet++;//skip 'v'
	BYTE buf[MAX_PACKET_SIZE];
	if(strncmp(packet,VCMD_FLASH_ERASE,strlen(VCMD_FLASH_ERASE))==0)
	{
		char *separator;
		U32 addr = 0;
		packet+=strlen(VCMD_FLASH_ERASE);
		addr = strtoul(packet, &separator, 16);

		if (*separator != ',')
		{
			DBG_LOG("incomplete flasherase packet received, dropping connection");
			send_error(ER_NUM_PACKET);
			return ;
		}

		int len = strtoul(separator + 1, &separator, 16);
		sectorsize = flash_get_sectorsize();
		sectorcnt = len/sectorsize;
		if(len%sectorsize)
			sectorcnt++;
		startsector = addr/sectorsize;
		flash_erase_sector(startsector,sectorcnt);
		putpkt("OK",2);
	}
	else if(strncmp(packet,VCMD_FLASH_WRITE,strlen(VCMD_FLASH_WRITE))==0)
	{
		//flash controller supports
		char *separator;
		U32 addr = strtoul(packet+strlen(VCMD_FLASH_WRITE), &separator, 16);

		// ?´ë?ë¶„ì? GDB??RSP ?€???¤ë¦„
		if (*separator != ',')
		{
			DBG_LOG("incomplete flashwrite packet received, dropping connection");
			send_error(ER_NUM_PACKET);
			return ;
		}
		separator++;
		int len = strtoul(separator, &separator, 16);

		if (*separator != ':')
		{
			DBG_LOG("incomplete flashwrite packet received, dropping connection");
			send_error(ER_NUM_PACKET);
			return ;
		}
		separator++;
		int rxlen = packetsize-(separator - packet)-1;
		len = remote_unescape_input((BYTE*)separator,rxlen,buf,MAX_PACKET_SIZE);
		flash_write(addr,(BYTE*)buf,len);
		putpkt("OK",2);
	}
	else if(strncmp(packet,VCMD_FLASH_READ,strlen(VCMD_FLASH_READ))==0)
	{
		//flash controller supports
		char *separator;
		U32 addr = 0;
		packet+=strlen(VCMD_FLASH_READ);
		addr = strtoul(packet, &separator, 16);

		if (*separator != ',')
		{
			DBG_LOG("incomplete flashreadpacket received, dropping connection");
			send_error(ER_NUM_PACKET);
			return ;
		}

		int len = strtoul(separator + 1, &separator, 16);
		flash_read(addr,buf,len);
		BYTE enbuf[MAX_PACKET_SIZE];
		int txlen;
		txlen= remote_escape_output((BYTE*)buf,len,enbuf,&len,MAX_PACKET_SIZE);
		putpkt((char*)enbuf,txlen);
	}
	else if(strncmp(packet,VCMD_RUNAT,strlen(VCMD_RUNAT))==0)
	{
		char *separator;
		U32 addr = 0;
		packet+=strlen(VCMD_RUNAT);
		packet++;//skip ':'
		addr = strtoul(packet, &separator, 16);
		void (*startfp)(void);
		startfp = (void (*)())addr;
#ifndef UART_MODE		
		usb_off();
#endif		
		dcache_invalidate_way();	
		icache_invalidate_way();	
		debugprintf("START at:%x\n",startfp);
		startfp();

	}
	else if(strncmp(packet,VCMD_FLASH_DONE,strlen(VCMD_FLASH_DONE))==0)
	{
		//do nothing
	}
#if RSP_SUPPORT_NAND==1	
	else if(strncmp(packet,VCMD_NAND_INIT,strlen(VCMD_NAND_INIT))==0)
	{
		if(nand_init()==FALSE)
			send_error(ER_NUM_MEM_READ);
		else
			putpkt("OK",2);
	}
	else if(strncmp(packet,VCMD_NAND_ERASEBLOCK,strlen(VCMD_NAND_ERASEBLOCK))==0)
	{
		char *separator;
		U32 startblock;
		startblock= strtoul(packet+strlen(VCMD_NAND_ERASEBLOCK), &separator, 16);

		if (*separator != ',')
		{
			DBG_LOG("incomplete nand_blockerase packet received, dropping connection");
			send_error(ER_NUM_PACKET);
			return ;
		}

		int blockcnt = strtoul(separator + 1, &separator, 16);
		int i;
		for(i=0;i<blockcnt;i++)
		{
			if(nand_eraseblock(startblock+i)==false)
			{
				send_error(ER_NUM_MEM_WRITE);
				return ;
			}
		}
		putpkt("OK",2);
		
	}
	else if(strncmp(packet,VCMD_NAND_READPAGE,strlen(VCMD_NAND_READPAGE))==0)
	{
		char *separator;
		U32 startpage= strtoul(packet+strlen(VCMD_NAND_READPAGE), &separator, 16);
		U32 pagesize = nand_get_pagesize();		
		if (*separator != ',')
		{
			DBG_LOG("incomplete nand read packet received, dropping connection");
			send_error(ER_NUM_PACKET);
			return ;
		}

		int pagecnt = strtoul(separator + 1, &separator, 16);
		int i;
		for(i=0;i<pagecnt;i++)
		{
			if(nand_readpage(startpage+i,buf+(i*pagesize))==false)
			{
				send_error(ER_NUM_MEM_READ);
				return ;
			}
		}
		int len = pagecnt*pagesize;
		BYTE enbuf[MAX_PACKET_SIZE];
		int txlen;
		txlen= remote_escape_output((BYTE*)buf,len,enbuf,&len,MAX_PACKET_SIZE);
		putpkt((char*)enbuf,txlen);
	}
	else if(strncmp(packet,VCMD_NAND_WRITEPAGE,strlen(VCMD_NAND_WRITEPAGE))==0)
	{
		//flash controller supports
		char *separator;
		U32 startpage = strtoul(packet+strlen(VCMD_FLASH_WRITE), &separator, 16);

		// ?´ë?ë¶„ì? GDB??RSP ?€???¤ë¦„
		if (*separator != ',')
		{
			DBG_LOG("incomplete nand write packet received, dropping connection");
			send_error(ER_NUM_PACKET);
			return ;
		}
		separator++;
		int len = strtoul(separator, &separator, 16);

		if (*separator != ':')
		{
			DBG_LOG("incomplete nand write packet received, dropping connection");
			send_error(ER_NUM_PACKET);
			return ;
		}
		separator++;
		int rxlen = packetsize-(separator - packet)-1;
		len = remote_unescape_input((BYTE*)separator,rxlen,buf,MAX_PACKET_SIZE);
		nand_writepage(startpage,buf);
		putpkt("OK",2);
	}
#endif	
}

/*
Not yet implemented
*/
/*
void rsp_proc_reason(char* packetbuf)
{
putpkt("S05",3);
}

void rsp_proc_read_registers()
{
char hex_buffer[REGISTER_MAX_NUM * 8 + 1];
int i;
for (i = 0; i < REGISTER_MAX_NUM; i++)
{
mem2hex((BYTE*)&registers[i],hex_buffer+(i*8),4);
}
putpkt(hex_buffer, REGISTER_MAX_NUM*8);
}

void rsp_proc_read_register(char* packetbuf)
{
char buf[MAX_PACKET_SIZE];
mem2hex((BYTE*)registers,buf,REGISTER_MAX_NUM*4);
putpkt(buf,REGISTER_MAX_NUM*4*2);
}

void rsp_proc_write_register(char* packet)
{
char *separator;
int regnum=0;
int regval;
regnum = strtoul(packet, &separator, 16);

if(regnum<0 || regnum > 27 || *separator!='=')
{
putpkt(NULL,0);
return;
}
regval = strtoul(separator+1,&separator,16);

registers[regnum]=regval;
putpkt("OK",2);
}

void rsp_proc_write_registers(char* packetbuf)
{
int i;
for(i=0;i<REGISTER_MAX_NUM;i++)
{
hex2mem(packetbuf,(BYTE*)registers[i],4);
}
putpkt("OK",2);
}

void rsp_proc_breakpoint(char* packetbuf)
{

}

void rsp_proc_continue(char* packetbuf)
{

}

void rsp_proc_singlestep(char* packetbuf)
{

}

void rsp_proc_kill(char* packetbuf)
{

}
*/
void rsp_proc_read_memory(char* packet)
{
	char *separator;
	U32 addr = 0;
	int len = 0;

	BYTE buffer[MAX_PACKET_SIZE];
	char *hex_buffer;
	packet++;//skip 'M'
	addr = strtoul(packet, &separator, 16);

	if (*separator != ',')
	{
		DBG_LOG("incomplete read memory packet received, dropping connection");
		return ;
	}

	len = strtoul(separator + 1, NULL, 16);
	memoryread(addr,buffer,len);
	hex_buffer = (char*)malloc(len * 2 + 1);

	int i;
	for (i = 0; i < len; i++)
	{
		BYTE t = buffer[i];
		hex_buffer[2 * i] = DIGITS[(t >> 4) & 0xf];
		hex_buffer[2 * i + 1] = DIGITS[t & 0xf];
	}
	putpkt(hex_buffer, len * 2);
	free(hex_buffer);
}

void rsp_proc_write_memory(char* packet)
{
	char *separator;
	U32 addr = 0;
	int len = 0;
	packet++;//skip 'm'
	addr = strtoul(packet, &separator, 16);

	if (*separator != ',')
	{
		DBG_LOG("incomplete write memory packet received, dropping connection");
		send_error(ER_NUM_PACKET);
		return ;
	}

	len = strtoul(separator + 1, &separator, 16);

	if (*(separator++) != ':')
	{
		DBG_LOG("incomplete write memory packet received, dropping connection");
		send_error(ER_NUM_PACKET);
		return ;
	}
	hex2mem(separator,(BYTE*)addr,len);
	putpkt("OK",2);
}


void sendmemorymap()
{
	char xml[1024];
	int flash_size=flash_get_size();
	int ram_size = 0x1000000;
	int sector_size = flash_get_sectorsize();
#if RSP_SUPPORT_NAND == 1
	int nand_size = nand_get_memsize_kbyte();
	int nand_page_size = nand_get_pagesize();
	int nand_block_size = nand_get_blocksize();
#else
	int nand_size = 0;
	int nand_page_size = 0;
	int nand_block_size = 0;
#endif
	
	sprintf(xml,"<?xml version=\"1.0\"?>\
				<memory-map>\
				<memory type=\"flash\" start=\"0\" length=\"%d\" sectorsize=\"%d\" />\
				<memory type=\"ram\" start=\"0x20000000\" length=\"%d\" />\
				<memory type=\"nand\" length=\"%d\" pagesize=\"%d\" blocksize=\"%d\"  />\
				</memory-map>",flash_size,sector_size,ram_size,nand_size/1024,nand_page_size,nand_block_size);
	debugstring(xml);
	debugstring("\r\n");
	putpkt(xml,strlen(xml));
}

void rsp_proc_query(char* packetbuf)
{
	packetbuf++;//skip 'q'
	if(strncmp(packetbuf,"Rcmd,",5)==0)
	{
		//remote command
		packetbuf+=5;
		if(strncmp(packetbuf,"rspv",4)==0)
		{
			int ver = RSPVERSION;
			mem2hex((BYTE*)&ver,packetbuf,4);
			putpkt(packetbuf,8);
		}
		else if(strncmp(packetbuf,"whoareyou",9)==0)
		{
			strcpy(packetbuf,"adStar");
			putpkt(packetbuf,8);
		}
	}
	else if(strncmp(packetbuf,"Xfer:",5)==0)
	{
		packetbuf+=5;
		if(strncmp(packetbuf,"memory-map:read",strlen("memory-map:read"))==0)
		{
			sendmemorymap();
		}
	}
}

void rsp_proc_write_binary(char* packet,int packetsize)		
{
	char *separator;
	U32 addr = 0;
	int len = 0;
	packet++;//skip 'X'
	addr = strtoul(packet, &separator, 16);

	if (*separator != ',')
	{
		DBG_LOG("incomplete write memory binary packet received, dropping connection");
		return ;
	}

	strtoul(separator + 1, &separator, 16);

	if (*separator != ':')
	{
		DBG_LOG("incomplete write memory binary packet received, dropping connection");
		return;
	}
	separator++;
	len = packetsize- (separator - packet)-1;
	int datalen = remote_unescape_input((BYTE*)separator,len,(BYTE*)separator,MAX_PACKET_SIZE);
	memorywrite(addr,(BYTE*)separator,datalen);
	putpkt("OK",2);
	return ;
}


void rsp_proc_read_binary(char* packet)
{
	char *separator;
	U32 addr = 0;
	int len = 0;

	BYTE buffer[MAX_PACKET_SIZE];
	BYTE buffer2[MAX_PACKET_SIZE];
	packet++;//skip 'M'
	addr = strtoul(packet, &separator, 16);

	if (*separator != ',')
	{
		DBG_LOG("incomplete read memory packet received, dropping connection");
		return ;
	}

	len = strtoul(separator + 1, NULL, 16);
	//debugprintf("%s,addr(%#x),len(%#x)\r\n",__FUNCTION__,addr,len);
	memoryread(addr,buffer,len);
	len = remote_escape_output(buffer,len,buffer2,&len,MAX_PACKET_SIZE);
	putpkt((char*)buffer2, len );
}


void RSP_Run()
{
	debugprintf("Remote Communication Mode(ver %d)\r\n",RSPVERSION);
	if(rsp_channel_init()==false)
		return;
	
	#if RSP_SUPPORT_NAND == 1
	nand_init();
	#endif
	
	int packetlen;
	char packetbuf[MAX_PACKET_SIZE+1];
	while(1)
	{
		//debugstring("wait command...\r\n");
		packetlen = getpkt(packetbuf);
		//debugstring(packetbuf);		debugstring("\r\n");

		switch(packetbuf[0])
		{

		case 'm':
			rsp_proc_read_memory(packetbuf);
			break;
		case 'M':
			rsp_proc_write_memory(packetbuf);
			break;
		case 'q':
			rsp_proc_query(packetbuf);
			break;
		case 'X':
			rsp_proc_write_binary(packetbuf,packetlen);
			break;
		case 'x':
			rsp_proc_read_binary(packetbuf);
			break;
		case 'F':
			rsp_proc_file_io(packetbuf);
			break;
		case 'v':
			rsp_proc_v(packetbuf,packetlen);
			break;
		/*
		case '?':
		rsp_proc_reason(packetbuf);
		break;
		case 'g':
		rsp_proc_read_registers();
		break;
		case 'p':
		rsp_proc_read_register(packetbuf);
		break;
		case 'G':
		rsp_proc_write_registers(packetbuf);
		break;
		case 'P':
		rsp_proc_write_register(packetbuf);
		break;
		case 'c':
		rsp_proc_continue(packetbuf);
		break;
		case 'z':
		case 'Z':
		rsp_proc_breakpoint(packetbuf);
		break;
		*/
		default:
			putpkt(NULL,0);
			break;

		}
	}
}
