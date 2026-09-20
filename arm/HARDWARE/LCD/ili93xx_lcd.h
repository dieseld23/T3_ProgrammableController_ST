#ifndef __LCD_H
#define __LCD_H		

#include "bitmap.h"
#include "stdlib.h"


//V1.2 change log
//Added the SPFD5408 driver, and the LCD ID is now printed in hex so the driver IC is easy to read off.
//V1.3
//Added fast IO support
//Changed the backlight control polarity (for board revision V1.8 and later)
//For LCD modules before version 1.8 (not including 1.8), change LCD_LED=1; in LCD_Init to LCD_LED=1;
//V1.4
//Changed LCD_ShowChar to draw characters with the point-drawing function.
//Added landscape and portrait display support
//V1.5 20110730
//1, Fixed the wrong colour readback on the B505 LCD.
//2, Changed how fast IO and landscape/portrait are configured.
//V1.6 20111116
//1, Added driver support for the LGDP4535 LCD
//V1.7 20120713
//1, Added the LCD_RD_DATA function
//2, Added ILI9341 support
//3, Added standalone driver code for the ILI9325
//4, Added the LCD_Scan_Dir function (use with care)	  
//6, Also changed some existing functions to suit the 9341
//V1.8 20120905
//1, Added the lcddev structure holding the key LCD parameters
//2, Added LCD_Display_Dir, which switches between landscape and portrait at run time
//V1.9 20120911
//1, Added the RM68042 driver (ID:6804), but the 6804 does not support landscape!! Reason: changing the scan direction
//breaks coordinate setting on the 6804. Several approaches were tried and none worked, so there is no fix for now.
//V2.0 20120924
//Without a hardware reset the ILI9341 ID reads back as 9300. LCD_Init was changed so that an
//unrecognised case (ID 9300 or an invalid ID) forces the driver IC to ILI9341 and runs the 9341 init.
//V2.1 20120930
//Fixed the ILI9325 colour readback bug.
//V2.2 20121007
//Fixed a bug in LCD_Scan_Dir.
//V2.3 20130120
//Added landscape support for the 6804
//V2.4 20131120
//1, Added support for the NT35310 (ID:5310) controller
//2, Added LCD_Set_Window for setting a window, which helps with fast fills, but it does not support the 6804 in landscape.
//////////////////////////////////////////////////////////////////////////////////	 

 
  
//LCD重要参数集
typedef struct  
{										    
	u16 width;			//LCD 宽度
	u16 height;			//LCD 高度
	u16 id;				//LCD ID
	u8  dir;			//横屏还是竖屏控制：0，竖屏；1，横屏。	
	u8	wramcmd;		//开始写gram指令
	u8  setxcmd;		//设置x坐标指令
	u8  setycmd;		//设置y坐标指令	 
}_lcd_dev; 	  

//LCD参数
extern _lcd_dev lcddev;	//Holds the key LCD parameters
//LCD pen colour and background colour	   
extern u16  POINT_COLOR;//默认红色    
extern u16  BACK_COLOR; //背景颜色.默认为白色


//////////////////////////////////////////////////////////////////////////////////	 
//-----------------LCD端口定义---------------- 
#define	LCD_LED PDout(13) //LCD背光    		 PD13
#define LCD_RST	PEout(1)  //LCD复位			 PE1 	    
//LCD地址结构体
typedef struct
{
	u16 LCD_REG;
	u16 LCD_RAM;
} LCD_TypeDef;
//使用NOR/SRAM的 Bank1.sector4,地址位HADDR[27,26]=11 A10作为数据命令区分线 
//注意设置时STM32内部会右移一位对其! 111110=0X3E			    
#define LCD_BASE        ((u32)(0x60000000 | 0x0001FFFE))
#define LCD             ((LCD_TypeDef *) LCD_BASE)
//////////////////////////////////////////////////////////////////////////////////
	 
//扫描方向定义
#define L2R_U2D  0 //Left to right, top to bottom
#define L2R_D2U  1 //Left to right, bottom to top
#define R2L_U2D  2 //Right to left, top to bottom
#define R2L_D2U  3 //Right to left, bottom to top

#define U2D_L2R  4 //Top to bottom, left to right
#define U2D_R2L  5 //Top to bottom, right to left
#define D2U_L2R  6 //Bottom to top, left to right
#define D2U_R2L  7 //Bottom to top, right to left	 

#define DFT_SCAN_DIR  L2R_U2D  //默认的扫描方向

//画笔颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE         	 0x001F  
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //棕色
#define BRRED 			 0XFC07 //棕红色
#define GRAY  			 0X8430 //Grey
//GUI颜色

#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  
#define GRAYBLUE       	 0X5458 //灰蓝色
//以上三色为PANEL的颜色 
 
#define LIGHTGREEN     	 0X841F //浅绿色
//#define LIGHTGRAY        0XEF5B //浅灰色(PANNEL)
#define LGRAY 			 0XC618 //浅灰色(PANNEL),窗体背景色

#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)
	    															  
void LCD_Init(void);													   	//Initialise
void LCD_DisplayOn(void);													//开显示
void LCD_DisplayOff(void);													//关显示
void LCD_Clear(u16 Color);	 												//Clear the screen
void LCD_SetCursor(u16 Xpos, u16 Ypos);										//设置光标
void LCD_DrawPoint(u16 x, u16 y);											//Draw a point
void LCD_Fast_DrawPoint(u16 x, u16 y, u16 color);							//Fast point draw
u16  LCD_ReadPoint(u16 x, u16 y); 											//读点 
void Draw_Circle(u16 x0, u16 y0, u8 r);										//画圆
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);							//Draw a line
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);		   				//Draw a rectangle
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color);		   			//填充单色
void LCD_Color_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 *color);			//填充指定颜色
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u8 mode);					//显示一个字符
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size);  					//显示一个数字
void LCD_ShowxNum(u16 x, u16 y, u32 num, u8 len, u8 size, u8 mode);			//显示 数字
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, char *p);	//显示一个字符串,12/16字体

void LCD_WriteReg(u8 LCD_Reg, u16 LCD_RegValue);
u16 LCD_ReadReg(u8 LCD_Reg);
void LCD_WriteRAM_Prepare(void);
void LCD_WriteRAM(u16 RGB_Code);		  
void LCD_Scan_Dir(u8 dir);													//设置屏扫描方向
void LCD_Display_Dir(u8 dir);												//设置屏幕显示方向
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height);					//Set the window

					   																			 
//9320/9325 LCD寄存器  
#define R0             0x00
#define R1             0x01
#define R2             0x02
#define R3             0x03
#define R4             0x04
#define R5             0x05
#define R6             0x06
#define R7             0x07
#define R8             0x08
#define R9             0x09
#define R10            0x0A
#define R12            0x0C
#define R13            0x0D
#define R14            0x0E
#define R15            0x0F
#define R16            0x10
#define R17            0x11
#define R18            0x12
#define R19            0x13
#define R20            0x14
#define R21            0x15
#define R22            0x16
#define R23            0x17
#define R24            0x18
#define R25            0x19
#define R26            0x1A
#define R27            0x1B
#define R28            0x1C
#define R29            0x1D
#define R30            0x1E
#define R31            0x1F
#define R32            0x20
#define R33            0x21
#define R34            0x22
#define R36            0x24
#define R37            0x25
#define R40            0x28
#define R41            0x29
#define R43            0x2B
#define R45            0x2D
#define R48            0x30
#define R49            0x31
#define R50            0x32
#define R51            0x33
#define R52            0x34
#define R53            0x35
#define R54            0x36
#define R55            0x37
#define R56            0x38
#define R57            0x39
#define R59            0x3B
#define R60            0x3C
#define R61            0x3D
#define R62            0x3E
#define R63            0x3F
#define R64            0x40
#define R65            0x41
#define R66            0x42
#define R67            0x43
#define R68            0x44
#define R69            0x45
#define R70            0x46
#define R71            0x47
#define R72            0x48
#define R73            0x49
#define R74            0x4A
#define R75            0x4B
#define R76            0x4C
#define R77            0x4D
#define R78            0x4E
#define R79            0x4F
#define R80            0x50
#define R81            0x51
#define R82            0x52
#define R83            0x53
#define R96            0x60
#define R97            0x61
#define R106           0x6A
#define R118           0x76
#define R128           0x80
#define R129           0x81
#define R130           0x82
#define R131           0x83
#define R132           0x84
#define R133           0x85
#define R134           0x86
#define R135           0x87
#define R136           0x88
#define R137           0x89
#define R139           0x8B
#define R140           0x8C
#define R141           0x8D
#define R143           0x8F
#define R144           0x90
#define R145           0x91
#define R146           0x92
#define R147           0x93
#define R148           0x94
#define R149           0x95
#define R150           0x96
#define R151           0x97
#define R152           0x98
#define R153           0x99
#define R154           0x9A
#define R157           0x9D
#define R192           0xC0
#define R193           0xC1
#define R229           0xE5							  		 
#endif
