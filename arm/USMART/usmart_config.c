#include "usmart.h"
#include "usmart_str.h"
////////////////////////////user configuration///////////////////////////////////////////////
//Include below the headers that declare the functions you use (add your own) 
#include "delay.h"	
#include "usart.h"		
#include "sys.h"
#include "lcd.h"
#include "flash.h"
#include "vmalloc.h"
#include "fattester.h"
												 
extern void led_set(u8 sta);
extern void test_fun(void(*ledset)(u8),u8 sta);
//Function name list (add your own)
//Put the function names and their search strings here
struct _m_usmart_nametab usmart_nametab[]=
{
#if USMART_USE_WRFUNS==1 	//If the read/write operations are enabled
	(void*)read_addr,"u32 read_addr(u32 addr)",
	(void*)write_addr,"void write_addr(u32 addr,u32 val)",	 
#endif		   
	(void*)delay_ms,"void delay_ms(u16 nms)",
 	(void*)delay_us,"void delay_us(u32 nus)",	 
	(void*)LCD_Clear,"void LCD_Clear(u16 Color)",
	(void*)LCD_Fill,"void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color)",
	(void*)LCD_DrawLine,"void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)",
	(void*)LCD_DrawRectangle,"void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)",
	(void*)Draw_Circle,"void Draw_Circle(u16 x0,u16 y0,u8 r)",
	(void*)LCD_ShowNum,"void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 size)",
	(void*)LCD_ShowString,"void LCD_ShowString(u16 x,u16 y,u16 width,u16 height,u8 size,char *p)",
	(void*)led_set,"void led_set(u8 sta)",
	(void*)test_fun,"void test_fun(void(*ledset)(u8),u8 sta)",				  	    
	(void*)LCD_ReadPoint,"u16 LCD_ReadPoint(u16 x,u16 y)",							 
	(void*)LCD_Display_Dir,"void LCD_Display_Dir(u8 dir)",
	(void*)LCD_ShowxNum,"void LCD_ShowxNum(u16 x,u16 y,u32 num,u8 len,u8 size,u8 mode)",
	(void*)SPI_Flash_Init,"void SPI_Flash_Init(void)",
		
	(void*)mymalloc,"void *mymalloc(u8 memx,u32 size)", 
	(void*)myfree,"void myfree(u8 memx,void *ptr)", 
	(void*)mem_perused,"u8 mem_perused(u8 memx) ", 	 

	(void*)mf_mount,"u8 mf_mount(u8 drv)", 
	(void*)mf_open,"u8 mf_open(u8*path,u8 mode)", 
	(void*)mf_close,"u8 mf_close(void)", 
	(void*)mf_read,"u8 mf_read(u16 len)", 
	(void*)mf_write,"u8 mf_write(u8*dat,u16 len)", 
	(void*)mf_opendir,"u8 mf_opendir(u8* path)", 
	(void*)mf_readdir,"u8 mf_readdir(void)", 
	(void*)mf_scan_files,"u8 mf_scan_files(char * path)", 
	(void*)mf_showfree,"u32 mf_showfree(u8 *drv)", 
	(void*)mf_lseek,"u8 mf_lseek(u32 offset)", 
	(void*)mf_tell,"u32 mf_tell(void)", 
	(void*)mf_size,"u32 mf_size(void)", 
	(void*)mf_mkdir,"u8 mf_mkdir(u8*pname)", 
	(void*)mf_fmkfs,"u8 mf_fmkfs(u8 drv,u8 mode,u16 au)", 
	(void*)mf_unlink,"u8 mf_unlink(u8 *pname)", 
	(void*)mf_rename,"u8 mf_rename(u8 *oldname,u8* newname)", 
	(void*)mf_gets,"void mf_gets(u16 size)", 
	(void*)mf_putc,"u8 mf_putc(u8 c)", 
	(void*)mf_puts,"u8 mf_puts(u8*c)", 
};						  
///////////////////////////////////END///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
//Initialise the function control manager
//Get the name of each controlled function
//Get the total number of functions
struct _m_usmart_dev usmart_dev=
{
	usmart_nametab,
	usmart_init,
	usmart_cmd_rec,
	usmart_exe,
	usmart_scan,
	sizeof(usmart_nametab)/sizeof(struct _m_usmart_nametab),//Number of functions
	0,	  	//Number of parameters
	0,	 	//Function ID
	1,		//Argument display type: 0 = decimal; 1 = hexadecimal
	0,		//Argument types; bit x: 0 = number, 1 = string	    
	0,	  	//Scratch table of argument lengths, initialised with MAX_PARM zeros
	0,		//The function's arguments, initialised with PARM_LEN zeros
};   



















