#include "lcd.h"
#include "stdlib.h"
#include "font.h" 
#include "usart.h"	 
#include "delay.h"   

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
				 
//LCD pen colour and background colour	   
u16 POINT_COLOR = BRRED;//0x0000;	//pen colour
u16 BACK_COLOR = 0xFFFF;  //Background colour 

//Holds the key LCD parameters
//Portrait by default
_lcd_dev lcddev;
	 
//Register write function
//regval: the register value
void LCD_WR_REG(u16 regval)
{ 
	LCD->LCD_REG = regval;//Write the register number to be written	 
}
//Write LCD data
//data: the value to write
void LCD_WR_DATA(u16 data)
{										    	   
	LCD->LCD_RAM = data;		 
}
//Read LCD data
//Return: the value read
u16 LCD_RD_DATA(void)
{										    	   
	return LCD->LCD_RAM;		 
}					   
//Write a register
//LCD_Reg: register address
//LCD_RegValue: the data to write
void LCD_WriteReg(u8 LCD_Reg, u16 LCD_RegValue)
{	
	LCD->LCD_REG = LCD_Reg;		//Write the register number to be written	 
	LCD->LCD_RAM = LCD_RegValue;//Write data	    		 
}	   
//Read a register
//LCD_Reg: register address
//Return: the data read
u16 LCD_ReadReg(u8 LCD_Reg)
{										   
	LCD_WR_REG(LCD_Reg);		//Write the number of the register to read
	delay_us(6);		  
	return LCD_RD_DATA();		//Returns the value read
}   
//Start writing to GRAM
void LCD_WriteRAM_Prepare(void)
{
 	LCD->LCD_REG = lcddev.wramcmd;	  
}	 
//Write GRAM on the LCD
//RGB_Code: the colour value
void LCD_WriteRAM(u16 RGB_Code)
{							    
	LCD->LCD_RAM = RGB_Code;//Write 16-bit GRAM
}
//Data read back from the ILI93xx is in GBR order, while we write in RGB order.
//This function converts between them
//c: the colour value in GBR order
//Return: the colour value in RGB order
u16 LCD_BGR2RGB(u16 c)
{
	u16  r, g, b, rgb;   
	b = (c >> 0) & 0x1f;
	g = (c >> 5) & 0x3f;
	r = (c >> 11) & 0x1f;	 
	rgb = (b << 11) | (g << 5) | (r << 0);		 
	return(rgb);
} 
//Needed when MDK is optimising for time at -O1
//Delay i
void opt_delay(u8 i)
{
	while(i--);
}
//Read the colour of a point	 
//x,y: coordinates
//Return: the colour of that point
u16 LCD_ReadPoint(u16 x, u16 y)
{
 	u16 r = 0, g = 0, b = 0;
	if(x >= lcddev.width || y >= lcddev.height)return 0;	//Out of range, so return straight away		   

	LCD_SetCursor(x, y);	    
	if(lcddev.id == 0X9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310)
		LCD_WR_REG(0X2E);									//9341/6804/3510: send the read GRAM command
	else
		LCD_WR_REG(R34);      		 						//Other ICs: send the read GRAM command

 	if(lcddev.id == 0X9320)
		opt_delay(2);										//FOR 9320, delay 2us	
		    
	if(LCD->LCD_RAM)
		r = 0;												//dummy Read	
		   
	opt_delay(2);	  
 	r = LCD->LCD_RAM;		  		  						//The colour at the actual coordinates
 	if(lcddev.id == 0X9341 || lcddev.id == 0X5310)			//The 9341 and NT35310 need two reads
 	{
		opt_delay(2);	  
		b = LCD->LCD_RAM; 
		g = r & 0XFF;										//On the 9341/5310 the first read gives R and G, R first then G, 8 bits each
		g <<= 8;
	}
	else if(lcddev.id == 0X6804)
	{
		r = LCD->LCD_RAM;									//On the 6804 only the second read gives the real value 
	}

	if(lcddev.id == 0X9325 || lcddev.id == 0X4535 || lcddev.id == 0X4531 || lcddev.id == 0X8989 || lcddev.id == 0XB505)
		return r;											//These ICs return the colour value directly
	else if(lcddev.id == 0X9341 || lcddev.id == 0X5310)
		return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));	//The ILI9341 and NT35310 need a conversion
	else
		return LCD_BGR2RGB(r);								//Other ICs
}

//Turn the LCD display on
void LCD_DisplayOn(void)
{					   
	if(lcddev.id == 0X9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310)
		LCD_WR_REG(0X29);									//Turn the display on
	else
		LCD_WriteReg(R7, 0x0173); 							//Turn the display on
}

//Turn the LCD display off
void LCD_DisplayOff(void)
{	   
	if(lcddev.id == 0X9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310)
		LCD_WR_REG(0X28);									//Turn the display off
	else
		LCD_WriteReg(R7, 0x0);								//Turn the display off 
}   

//Set the cursor position
//Xpos: x coordinate
//Ypos: y coordinate
void LCD_SetCursor(u16 Xpos, u16 Ypos)
{	 
 	if(lcddev.id == 0X9341 || lcddev.id == 0X5310)
	{		    
		LCD_WR_REG(lcddev.setxcmd); 
		LCD_WR_DATA(Xpos >> 8); 
		LCD_WR_DATA(Xpos & 0XFF);	 
		LCD_WR_REG(lcddev.setycmd); 
		LCD_WR_DATA(Ypos >> 8); 
		LCD_WR_DATA(Ypos & 0XFF);
	}
	else if(lcddev.id == 0X6804)
	{
		if(lcddev.dir == 1)
			Xpos = lcddev.width - 1 - Xpos;					//Handling for landscape

		LCD_WR_REG(lcddev.setxcmd); 
		LCD_WR_DATA(Xpos >> 8); 
		LCD_WR_DATA(Xpos & 0XFF);	 
		LCD_WR_REG(lcddev.setycmd); 
		LCD_WR_DATA(Ypos >> 8); 
		LCD_WR_DATA(Ypos & 0XFF);
	}
	else
	{
		if(lcddev.dir == 1)
			Xpos = lcddev.width - 1 - Xpos;					//Landscape simply swaps the x and y coordinates

		LCD_WriteReg(lcddev.setxcmd, Xpos);
		LCD_WriteReg(lcddev.setycmd, Ypos);
	}	 
}
		 
//Set the LCD auto scan direction
//Note: other functions can be affected by what this sets (especially the odd 9341 and 6804),
//so normally just use L2R_U2D; other scan directions can make the display come out wrong.
//dir: 0~7 for the eight directions (defined in lcd.h)
//Tested on the 9320/9325/9328/4531/4535/1505/b505/8989/5408/9341/5310 and others	   	   
void LCD_Scan_Dir(u8 dir)
{
	u16 regval = 0;
	u8 dirreg = 0;
	u16 temp;  
	if(lcddev.dir == 1 && lcddev.id != 0X6804)		//In landscape the scan direction is left alone on the 6804!
	{			   
		switch(dir)//Direction conversion
		{
			case 0:dir=6;break;
			case 1:dir=7;break;
			case 2:dir=4;break;
			case 3:dir=5;break;
			case 4:dir=1;break;
			case 5:dir=0;break;
			case 6:dir=3;break;
			case 7:dir=2;break;	     
		}
	}

	if(lcddev.id == 0x9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310)	//The 9341/6804/5310 are special cases
	{
		switch(dir)
		{
			case L2R_U2D://Left to right, top to bottom
				regval |= (0 << 7) | (0 << 6) | (0 << 5); 
				break;
			case L2R_D2U://Left to right, bottom to top
				regval |= (1 << 7) | (0 << 6) | (0 << 5); 
				break;
			case R2L_U2D://Right to left, top to bottom
				regval |= (0 << 7) | (1 << 6) | (0 << 5); 
				break;
			case R2L_D2U://Right to left, bottom to top
				regval |= (1 << 7) | (1 << 6) | (0 << 5); 
				break;	 
			case U2D_L2R://Top to bottom, left to right
				regval |= (0 << 7) | (0 << 6) | (1 << 5); 
				break;
			case U2D_R2L://Top to bottom, right to left
				regval |= (0 << 7) | (1 << 6) | (1 << 5); 
				break;
			case D2U_L2R://Bottom to top, left to right
				regval |= (1 << 7) | (0 << 6) | (1 << 5); 
				break;
			case D2U_R2L://Bottom to top, right to left
				regval |= (1 << 7) | (1 << 6) | (1 << 5); 
				break;	 
		}

		dirreg = 0X36;
 		if(lcddev.id != 0X5310)
			regval |= 0X08;						//The 5310 does not need BGR

		if(lcddev.id == 0X6804)
			regval |= 0x02;						//Bit 6 on the 6804 is inverted relative to the 9341	   

		LCD_WriteReg(dirreg, regval);

 		if((regval & 0X20) || lcddev.dir == 1)
		{
			if(lcddev.width < lcddev.height)	//Swap X and Y
			{
				temp = lcddev.width;
				lcddev.width = lcddev.height;
				lcddev.height = temp;
 			}
		}
		else  
		{
			if(lcddev.width > lcddev.height)	//Swap X and Y
			{
				temp = lcddev.width;
				lcddev.width = lcddev.height;
				lcddev.height = temp;
 			}
		}
		  
		LCD_WR_REG(lcddev.setxcmd); 
		LCD_WR_DATA(0);
		LCD_WR_DATA(0);
		LCD_WR_DATA((lcddev.width - 1) >> 8);
		LCD_WR_DATA((lcddev.width - 1) & 0XFF);
		LCD_WR_REG(lcddev.setycmd); 
		LCD_WR_DATA(0);
		LCD_WR_DATA(0);
		LCD_WR_DATA((lcddev.height - 1) >> 8);
		LCD_WR_DATA((lcddev.height - 1) & 0XFF);  
  	}
	else 
	{
		switch(dir)
		{
			case L2R_U2D://Left to right, top to bottom
				regval |= (1 << 5) | (1 << 4) | (0 << 3); 
				break;
			case L2R_D2U://Left to right, bottom to top
				regval |= (0 << 5) | (1 << 4) | (0 << 3); 
				break;
			case R2L_U2D://Right to left, top to bottom
				regval |= (1 << 5) | (0 << 4) | (0 << 3);
				break;
			case R2L_D2U://Right to left, bottom to top
				regval |= (0 << 5) | (0 << 4) | (0 << 3); 
				break;	 
			case U2D_L2R://Top to bottom, left to right
				regval |= (1 << 5) | (1 << 4) | (1 << 3); 
				break;
			case U2D_R2L://Top to bottom, right to left
				regval |= (1 << 5) | (0 << 4) | (1 << 3); 
				break;
			case D2U_L2R://Bottom to top, left to right
				regval |= (0 << 5) | (1 << 4) | (1 << 3); 
				break;
			case D2U_R2L://Bottom to top, right to left
				regval |= (0 << 5) | (0 << 4) | (1 << 3); 
				break;	 
		}

		if(lcddev.id == 0x8989)					//8989 IC
		{
			dirreg = 0X11;
			regval |= 0X6040;					//65K   
	 	}
		else									//Other driver ICs		  
		{
			dirreg = 0X03;
			regval |= 1 << 12;  
		}

		LCD_WriteReg(dirreg, regval);
	}
}
  
//Draw a point
//x,y: coordinates
//POINT_COLOR: the colour of the point
void LCD_DrawPoint(u16 x, u16 y)
{
	LCD_SetCursor(x, y);		//Set the cursor position 
	LCD_WriteRAM_Prepare();	//Start writing to GRAM
	LCD->LCD_RAM = POINT_COLOR; 
}

//Fast point draw
//x,y: coordinates
//color: colour
void LCD_Fast_DrawPoint(u16 x, u16 y, u16 color)
{	   
	if(lcddev.id == 0X9341 || lcddev.id == 0X5310)
	{
		LCD_WR_REG(lcddev.setxcmd); 
		LCD_WR_DATA(x >> 8); 
		LCD_WR_DATA(x & 0XFF);	 
		LCD_WR_REG(lcddev.setycmd); 
		LCD_WR_DATA(y >> 8); 
		LCD_WR_DATA(y & 0XFF);
	}
	else if(lcddev.id == 0X6804)
	{		    
		if(lcddev.dir == 1)
			x = lcddev.width - 1 - x;		//Handling for landscape

		LCD_WR_REG(lcddev.setxcmd); 
		LCD_WR_DATA(x >> 8); 
		LCD_WR_DATA(x & 0XFF);	 
		LCD_WR_REG(lcddev.setycmd); 
		LCD_WR_DATA(y >> 8); 
		LCD_WR_DATA(y & 0XFF);
	}
	else
	{
 		if(lcddev.dir == 1)
			x = lcddev.width - 1 - x;		//Landscape simply swaps the x and y coordinates

		LCD_WriteReg(lcddev.setxcmd, x);
		LCD_WriteReg(lcddev.setycmd, y);
	}			 

	LCD->LCD_REG = lcddev.wramcmd; 
	LCD->LCD_RAM = color; 
}

//Set the LCD display orientation
//dir: 0 = portrait; 1 = landscape
void LCD_Display_Dir(u8 dir)
{
	if(dir == 0)			//Portrait
	{
		lcddev.dir = 0;		//Portrait
		lcddev.width = 240;
		lcddev.height = 320;

		if(lcddev.id == 0X9341 || lcddev.id == 0X6804 || lcddev.id == 0X5310)
		{
			lcddev.wramcmd = 0X2C;
	 		lcddev.setxcmd = 0X2A;
			lcddev.setycmd = 0X2B;  	 
			if(lcddev.id == 0X6804 || lcddev.id == 0X5310)
			{
				lcddev.width = 320;
				lcddev.height = 480;
			}
		}
		else if(lcddev.id == 0X8989)
		{
			lcddev.wramcmd = R34;
	 		lcddev.setxcmd = 0X4E;
			lcddev.setycmd = 0X4F;  
		}
		else
		{
			lcddev.wramcmd = R34;
	 		lcddev.setxcmd = R32;
			lcddev.setycmd = R33;  
		}
	}
	else	 				//Landscape
	{	  				
		lcddev.dir = 1;		//Landscape
		lcddev.width = 320;
		lcddev.height = 240;
		
		if(lcddev.id == 0X9341 || lcddev.id == 0X5310)
		{
			lcddev.wramcmd = 0X2C;
	 		lcddev.setxcmd = 0X2A;
			lcddev.setycmd = 0X2B;  	 
		}
		else if(lcddev.id == 0X6804)	 
		{
 			lcddev.wramcmd = 0X2C;
	 		lcddev.setxcmd = 0X2B;
			lcddev.setycmd = 0X2A; 
		}
		else if(lcddev.id == 0X8989)
		{
			lcddev.wramcmd = R34;
	 		lcddev.setxcmd = 0X4F;
			lcddev.setycmd = 0X4E;   
		}
		else
		{
			lcddev.wramcmd = R34;
	 		lcddev.setxcmd = R33;
			lcddev.setycmd = R32;  
		}

		if(lcddev.id == 0X6804 || lcddev.id == 0X5310)
		{ 	 
			lcddev.width = 480;
			lcddev.height = 320; 			
		}
	}
	 
	LCD_Scan_Dir(DFT_SCAN_DIR);		//Default scan direction
}
	 
//Set the window and move the drawing position to its top-left corner (sx,sy).
//sx,sy: the window start coordinates (top left)
//width,height: the window width and height, which must be greater than 0!!
//The window size is width*height.
//The 68042 does not support window setting in landscape!! 
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height)
{   
	u8 hsareg, heareg, vsareg, veareg;
	u16 hsaval, heaval, vsaval, veaval;
	 
	width = sx + width - 1;
	height = sy + height - 1;
	if(lcddev.id == 0X9341 || lcddev.id == 0X5310 || lcddev.id == 0X6804)	//Not supported on the 6804 in landscape
	{
		LCD_WR_REG(lcddev.setxcmd); 
		LCD_WR_DATA(sx >> 8); 
		LCD_WR_DATA(sx & 0XFF);	 
		LCD_WR_DATA(width >> 8); 
		LCD_WR_DATA(width & 0XFF);  
		LCD_WR_REG(lcddev.setycmd); 
		LCD_WR_DATA(sy >> 8); 
		LCD_WR_DATA(sy & 0XFF); 
		LCD_WR_DATA(height >> 8); 
		LCD_WR_DATA(height & 0XFF); 
	}
	else																	//Other driver ICs
	{
		if(lcddev.dir == 1)													//Landscape
		{
			//Window value
			hsaval = sy;				
			heaval = height;
			vsaval = lcddev.width - width - 1;
			veaval = lcddev.width - sx - 1;				
		}
		else
		{ 
			hsaval = sx;				
			heaval = width;
			vsaval = sy;
			veaval = height;
		}

	 	if(lcddev.id == 0X8989)				//8989 IC
		{
			hsareg = 0X44;
			heareg = 0X44;					//Horizontal window register (on the 1289 a single register controls it)
			hsaval |= (heaval << 8);		//Get the register value.
			heaval = hsaval;
			vsareg = 0X45;
			veareg = 0X46;					//Vertical window register	  
		}
		else  								//Other driver ICs
		{
			hsareg = 0X50;
			heareg = 0X51;					//Horizontal window register
			vsareg = 0X52;
			veareg = 0X53;					//Vertical window register	  
		}

		//Set the register value
		LCD_WriteReg(hsareg, hsaval);
		LCD_WriteReg(heareg, heaval);
		LCD_WriteReg(vsareg, vsaval);
		LCD_WriteReg(veareg, veaval);		
		LCD_SetCursor(sx, sy);				//Set the cursor position
	}
}
 
//Initialise the LCD
//This init handles the various ILI93XX panels, but the other functions are written around the ILI9320!!!
//Not tested on other controller parts! 
void LCD_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	FSMC_NORSRAMInitTypeDef FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef readWriteTiming; 
	FSMC_NORSRAMTimingInitTypeDef writeTiming;
	
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);													//Enable the FSMC clock
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO, ENABLE);	//Enable the PORTD, PORTE and AFIO alternate-function clocks

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7 | GPIO_Pin_8 \
								| GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 \
								| GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;	// PD13 drives the backlight
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	LCD_LED	= 1;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;	// PE1 drives the reset
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	
	LCD_RST = 0;
	delay_ms(10);
	LCD_RST = 1;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	readWriteTiming.FSMC_AddressSetupTime = 0x01;	 //Address setup time (ADDSET) of 2 HCLK, 1/36M=27ns
	readWriteTiming.FSMC_AddressHoldTime = 0x00;	 //Address hold time (ADDHLD) is unused in mode A	
	readWriteTiming.FSMC_DataSetupTime = 0x0f;		 //Data hold time of 16 HCLK, because LCD controllers cannot be read too fast, the 1289 especially.
	readWriteTiming.FSMC_BusTurnAroundDuration = 0x00;
	readWriteTiming.FSMC_CLKDivision = 0x00;
	readWriteTiming.FSMC_DataLatency = 0x00;
	readWriteTiming.FSMC_AccessMode = FSMC_AccessMode_A;	 //Mode A 


	writeTiming.FSMC_AddressSetupTime = 0x00;	 //Address setup time (ADDSET) of 1 HCLK  
	writeTiming.FSMC_AddressHoldTime = 0x00;	 //Address hold time (A		
	writeTiming.FSMC_DataSetupTime = 0x03;		 //Data hold time of 4 HCLK	
	writeTiming.FSMC_BusTurnAroundDuration = 0x00;
	writeTiming.FSMC_CLKDivision = 0x00;
	writeTiming.FSMC_DataLatency = 0x00;
	writeTiming.FSMC_AccessMode = FSMC_AccessMode_A;	 //Mode A 


	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM1;						// we use NE1 here, which maps to BTCR[0],[1].
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;	// data and address are not multiplexed
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;				// FSMC_MemoryType_SRAM;  //SRAM   
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;		// memory data width is 16 bits   
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode =FSMC_BurstAccessMode_Disable;	// FSMC_BurstAccessMode_Disable; 
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable; 
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;   
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;  
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;		// memory write enable
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;   
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Enable;			// reads and writes use different timings
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable; 
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &readWriteTiming; 		// read/write timing
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &writeTiming;  				// write timing

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);  	//Initialise the FSMC configuration

	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);	//Enable BANK1
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 	RCC->AHBENR |= 1 << 8;     	 	//enable the FSMC clock	  

// 	//clear the register
// 	//bank1 has NE1~4, each with a BCR and a TCR, so eight registers in all.
// 	//we use NE4 here, which maps to BTCR[6],[7].				    
// 	FSMC_Bank1->BTCR[0]=0X00000000;
// 	FSMC_Bank1->BTCR[1]=0X00000000;
// 	FSMC_Bank1E->BWTR[0]=0X00000000;

// 	//set up the BCR register	using asynchronous mode
// 	FSMC_Bank1->BTCR[0]|=1<<12;		//memory write enable
// 	FSMC_Bank1->BTCR[0]|=1<<14;		//reads and writes use different timings
// 	FSMC_Bank1->BTCR[0]|=1<<4; 		//memory data width is 16 bits
// 			
// 	//set up the BTR register	
// 	//read timing control register 							    
// 	FSMC_Bank1->BTCR[1]|=0<<28;		//mode A 	 							  	 
// 	FSMC_Bank1->BTCR[1]|=1<<0; 		//address setup time (ADDSET) of 2 HCLK, 1/36M=27ns	 	 
// 	//because LCD controllers cannot be read too fast, the 1289 especially.
// 	FSMC_Bank1->BTCR[1]|=0XF<<8;  	//data hold time of 16 HCLK
// 			 
// 	//write timing control register  
// 	FSMC_Bank1E->BWTR[0]|=0<<28; 	//mode A 	 							    
// 	FSMC_Bank1E->BWTR[0]|=0<<0;		//address setup time (ADDSET) of 1 HCLK
// 	 
// 	//4 HCLK (HCLK=72M) because the LCD controller needs a write pulse of at least 50ns. 72M/4=24M=55ns  	 
// 	FSMC_Bank1E->BWTR[0]|=3<<8; 	//data hold time of 4 HCLK	

// 	//enable BANK1, region 4
// 	FSMC_Bank1->BTCR[0]|=1<<0;		//enable BANK1, region 4	  
			 
	delay_ms(50); // delay 50 ms 
	LCD_WriteReg(0x0000, 0x0001);
	delay_ms(50); // delay 50 ms
	 
  	lcddev.id = LCD_ReadReg(0x0000);   
  	if(lcddev.id < 0XFF || lcddev.id == 0XFFFF || lcddev.id == 0X9300)//The ID read back is wrong; a check for lcddev.id==0X9300 was added because an unreset 9341 reads back as 9300
	{	
 		//Try reading the 9341 ID		
		LCD_WR_REG(0XD3);				   
		LCD_RD_DATA(); 					//dummy read 	
 		LCD_RD_DATA();   	    		//Reads back 0X00
  		lcddev.id = LCD_RD_DATA(); 		//Reads 93								   
 		lcddev.id <<= 8;
		lcddev.id |= LCD_RD_DATA();		//Reads 41 	   			   
 		if(lcddev.id != 0X9341)			//Not a 9341, so try the 6804
		{	
 			LCD_WR_REG(0XBF);				   
			LCD_RD_DATA(); 				//dummy read 	 
	 		LCD_RD_DATA();   	    	//Reads back 0X01			   
	 		LCD_RD_DATA(); 				//Reads back 0XD0 			  	
	  		lcddev.id = LCD_RD_DATA();	//This reads back 0X68 
			lcddev.id <<= 8;
	  		lcddev.id |= LCD_RD_DATA();	//This reads back 0X04	   	  
 		}
		 
		if(lcddev.id != 0X9341 && lcddev.id != 0X6804)				//Neither a 9341 nor a 6804, so try the NT35310
		{
			LCD_WR_REG(0XD4);				   
			LCD_RD_DATA(); 				//dummy read  
			LCD_RD_DATA();   			//Reads back 0X01	 
	 		lcddev.id = LCD_RD_DATA();	//Reads back 0X53	
			lcddev.id <<= 8;	 
	  		lcddev.id |= LCD_RD_DATA();	//This reads back 0X10	 
		}			
	}

 	printf("\r\nLCD ID: %x\r\n", lcddev.id); //Print the LCD ID 
 
	if(lcddev.id == 0X9341)	//9341 initialisation
	{	 
		LCD_WR_REG(0xCF);  
		LCD_WR_DATA(0x00); 
		LCD_WR_DATA(0xC1); 
		LCD_WR_DATA(0X30); 
		LCD_WR_REG(0xED);  
		LCD_WR_DATA(0x64); 
		LCD_WR_DATA(0x03); 
		LCD_WR_DATA(0X12); 
		LCD_WR_DATA(0X81); 
		LCD_WR_REG(0xE8);  
		LCD_WR_DATA(0x85); 
		LCD_WR_DATA(0x10); 
		LCD_WR_DATA(0x7A); 
		LCD_WR_REG(0xCB);  
		LCD_WR_DATA(0x39); 
		LCD_WR_DATA(0x2C); 
		LCD_WR_DATA(0x00); 
		LCD_WR_DATA(0x34); 
		LCD_WR_DATA(0x02); 
		LCD_WR_REG(0xF7);  
		LCD_WR_DATA(0x20); 
		LCD_WR_REG(0xEA);  
		LCD_WR_DATA(0x00); 
		LCD_WR_DATA(0x00); 
		LCD_WR_REG(0xC0);    //Power control 
		LCD_WR_DATA(0x1B);   //VRH[5:0] 
		LCD_WR_REG(0xC1);    //Power control 
		LCD_WR_DATA(0x01);   //SAP[2:0];BT[3:0] 
		LCD_WR_REG(0xC5);    //VCM control 
		LCD_WR_DATA(0x30); 	 //3F
		LCD_WR_DATA(0x30); 	 //3C
		LCD_WR_REG(0xC7);    //VCM control2 
		LCD_WR_DATA(0XB7); 
		LCD_WR_REG(0x36);    // Memory Access Control 
		LCD_WR_DATA(0x48); 
		LCD_WR_REG(0x3A);   
		LCD_WR_DATA(0x55); 
		LCD_WR_REG(0xB1);   
		LCD_WR_DATA(0x00);   
		LCD_WR_DATA(0x1A); 
		LCD_WR_REG(0xB6);    // Display Function Control 
		LCD_WR_DATA(0x0A); 
		LCD_WR_DATA(0xA2); 
		LCD_WR_REG(0xF2);    // 3Gamma Function Disable 
		LCD_WR_DATA(0x00); 
		LCD_WR_REG(0x26);    //Gamma curve selected 
		LCD_WR_DATA(0x01); 
		LCD_WR_REG(0xE0);    //Set Gamma 
		LCD_WR_DATA(0x0F); 
		LCD_WR_DATA(0x2A); 
		LCD_WR_DATA(0x28); 
		LCD_WR_DATA(0x08); 
		LCD_WR_DATA(0x0E); 
		LCD_WR_DATA(0x08); 
		LCD_WR_DATA(0x54); 
		LCD_WR_DATA(0XA9); 
		LCD_WR_DATA(0x43); 
		LCD_WR_DATA(0x0A); 
		LCD_WR_DATA(0x0F); 
		LCD_WR_DATA(0x00); 
		LCD_WR_DATA(0x00); 
		LCD_WR_DATA(0x00); 
		LCD_WR_DATA(0x00); 		 
		LCD_WR_REG(0XE1);    //Set Gamma 
		LCD_WR_DATA(0x00); 
		LCD_WR_DATA(0x15); 
		LCD_WR_DATA(0x17); 
		LCD_WR_DATA(0x07); 
		LCD_WR_DATA(0x11); 
		LCD_WR_DATA(0x06); 
		LCD_WR_DATA(0x2B); 
		LCD_WR_DATA(0x56); 
		LCD_WR_DATA(0x3C); 
		LCD_WR_DATA(0x05); 
		LCD_WR_DATA(0x10); 
		LCD_WR_DATA(0x0F); 
		LCD_WR_DATA(0x3F); 
		LCD_WR_DATA(0x3F); 
		LCD_WR_DATA(0x0F); 
		LCD_WR_REG(0x2B); 
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x01);
		LCD_WR_DATA(0x3f);
		LCD_WR_REG(0x2A); 
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xef);	 
		LCD_WR_REG(0x11); //Exit Sleep
		delay_ms(120);
		LCD_WR_REG(0x29); //display on	
	}
	else if(lcddev.id == 0x6804)	//6804 initialisation
	{
		LCD_WR_REG(0X11);
		delay_ms(20);
		LCD_WR_REG(0XD0);//VCI1  VCL  VGH  VGL DDVDH VREG1OUT power amplitude setting
		LCD_WR_DATA(0X07); 
		LCD_WR_DATA(0X42); 
		LCD_WR_DATA(0X1D); 
		LCD_WR_REG(0XD1);//VCOMH VCOM_AC amplitude setting
		LCD_WR_DATA(0X00);
		LCD_WR_DATA(0X1a);
		LCD_WR_DATA(0X09); 
		LCD_WR_REG(0XD2);//Operational Amplifier Circuit Constant Current Adjust , charge pump frequency setting
		LCD_WR_DATA(0X01);
		LCD_WR_DATA(0X22);
		LCD_WR_REG(0XC0);//REV SM GS 
		LCD_WR_DATA(0X10);
		LCD_WR_DATA(0X3B);
		LCD_WR_DATA(0X00);
		LCD_WR_DATA(0X02);
		LCD_WR_DATA(0X11);
		
		LCD_WR_REG(0XC5);// Frame rate setting = 72HZ  when setting 0x03
		LCD_WR_DATA(0X03);
		
		LCD_WR_REG(0XC8);//Gamma setting
		LCD_WR_DATA(0X00);
		LCD_WR_DATA(0X25);
		LCD_WR_DATA(0X21);
		LCD_WR_DATA(0X05);
		LCD_WR_DATA(0X00);
		LCD_WR_DATA(0X0a);
		LCD_WR_DATA(0X65);
		LCD_WR_DATA(0X25);
		LCD_WR_DATA(0X77);
		LCD_WR_DATA(0X50);
		LCD_WR_DATA(0X0f);
		LCD_WR_DATA(0X00);	  
						  
   		LCD_WR_REG(0XF8);
		LCD_WR_DATA(0X01);	  

 		LCD_WR_REG(0XFE);
 		LCD_WR_DATA(0X00);
 		LCD_WR_DATA(0X02);
		
		LCD_WR_REG(0X20);//Exit invert mode

		LCD_WR_REG(0X36);
		LCD_WR_DATA(0X08);//Was a
		
		LCD_WR_REG(0X3A);
		LCD_WR_DATA(0X55);//16-bit mode	  
		LCD_WR_REG(0X2B);
		LCD_WR_DATA(0X00);
		LCD_WR_DATA(0X00);
		LCD_WR_DATA(0X01);
		LCD_WR_DATA(0X3F);
		
		LCD_WR_REG(0X2A);
		LCD_WR_DATA(0X00);
		LCD_WR_DATA(0X00);
		LCD_WR_DATA(0X01);
		LCD_WR_DATA(0XDF);
		delay_ms(120);
		LCD_WR_REG(0X29); 	 
 	}
	else if(lcddev.id == 0x5310)
	{ 
		LCD_WR_REG(0xED);
		LCD_WR_DATA(0x01);
		LCD_WR_DATA(0xFE);

		LCD_WR_REG(0xEE);
		LCD_WR_DATA(0xDE);
		LCD_WR_DATA(0x21);

		LCD_WR_REG(0xF1);
		LCD_WR_DATA(0x01);
		LCD_WR_REG(0xDF);
		LCD_WR_DATA(0x10);

		//VCOMvoltage//
		LCD_WR_REG(0xC4);
		LCD_WR_DATA(0x8F);	  //5f

		LCD_WR_REG(0xC6);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xE2);
		LCD_WR_DATA(0xE2);
		LCD_WR_DATA(0xE2);
		LCD_WR_REG(0xBF);
		LCD_WR_DATA(0xAA);

		LCD_WR_REG(0xB0);
		LCD_WR_DATA(0x0D);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x0D);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x11);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x19);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x21);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x2D);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x3D);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x5D);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x5D);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xB1);
		LCD_WR_DATA(0x80);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x8B);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x96);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xB2);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x02);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x03);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xB3);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xB4);
		LCD_WR_DATA(0x8B);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x96);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA1);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xB5);
		LCD_WR_DATA(0x02);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x03);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x04);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xB6);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xB7);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x3F);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x5E);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x64);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x8C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xAC);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xDC);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x70);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x90);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xEB);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xDC);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xB8);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xBA);
		LCD_WR_DATA(0x24);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC1);
		LCD_WR_DATA(0x20);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x54);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xFF);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC2);
		LCD_WR_DATA(0x0A);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x04);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC3);
		LCD_WR_DATA(0x3C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x3A);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x39);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x37);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x3C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x36);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x32);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x2F);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x2C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x29);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x26);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x24);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x24);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x23);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x3C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x36);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x32);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x2F);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x2C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x29);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x26);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x24);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x24);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x23);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC4);
		LCD_WR_DATA(0x62);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x05);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x84);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xF0);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x18);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA4);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x18);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x50);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x0C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x17);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x95);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xF3);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xE6);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC5);
		LCD_WR_DATA(0x32);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x44);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x65);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x76);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x88);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC6);
		LCD_WR_DATA(0x20);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x17);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x01);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC7);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC8);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xC9);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xE0);
		LCD_WR_DATA(0x16);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x1C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x21);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x36);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x46);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x52);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x64);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x7A);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x8B);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x99);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA8);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xB9);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xC4);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xCA);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD2);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD9);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xE0);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xF3);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xE1);
		LCD_WR_DATA(0x16);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x1C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x22);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x36);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x45);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x52);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x64);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x7A);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x8B);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x99);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA8);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xB9);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xC4);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xCA);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD2);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD8);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xE0);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xF3);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xE2);
		LCD_WR_DATA(0x05);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x0B);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x1B);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x34);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x44);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x4F);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x61);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x79);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x88);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x97);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA6);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xB7);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xC2);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xC7);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD1);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD6);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xDD);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xF3);
		LCD_WR_DATA(0x00);
		LCD_WR_REG(0xE3);
		LCD_WR_DATA(0x05);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x1C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x33);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x44);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x50);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x62);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x78);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x88);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x97);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA6);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xB7);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xC2);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xC7);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD1);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD5);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xDD);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xF3);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xE4);
		LCD_WR_DATA(0x01);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x01);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x02);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x2A);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x3C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x4B);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x5D);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x74);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x84);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x93);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA2);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xB3);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xBE);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xC4);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xCD);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD3);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xDD);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xF3);
		LCD_WR_DATA(0x00);
		LCD_WR_REG(0xE5);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x02);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x29);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x3C);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x4B);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x5D);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x74);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x84);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x93);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xA2);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xB3);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xBE);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xC4);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xCD);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xD3);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xDC);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xF3);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xE6);
		LCD_WR_DATA(0x11);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x34);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x56);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x76);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x77);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x66);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x88);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x99);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xBB);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x99);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x66);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x55);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x55);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x45);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x43);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x44);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xE7);
		LCD_WR_DATA(0x32);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x55);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x76);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x66);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x67);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x67);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x87);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x99);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xBB);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x99);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x77);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x44);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x56);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x23); 
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x33);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x45);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xE8);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x99);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x87);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x88);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x77);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x66);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x88);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xAA);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0xBB);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x99);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x66);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x55);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x55);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x44);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x44);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x55);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xE9);
		LCD_WR_DATA(0xAA);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0x00);
		LCD_WR_DATA(0xAA);

		LCD_WR_REG(0xCF);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xF0);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x50);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xF3);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0xF9);
		LCD_WR_DATA(0x06);
		LCD_WR_DATA(0x10);
		LCD_WR_DATA(0x29);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0x3A);
		LCD_WR_DATA(0x55);	//66

		LCD_WR_REG(0x11);
		delay_ms(100);
		LCD_WR_REG(0x29);
		LCD_WR_REG(0x35);
		LCD_WR_DATA(0x00);

		LCD_WR_REG(0x51);
		LCD_WR_DATA(0xFF);
		LCD_WR_REG(0x53);
		LCD_WR_DATA(0x2C);
		LCD_WR_REG(0x55);
		LCD_WR_DATA(0x82);
		LCD_WR_REG(0x2c);
	}
	else if(lcddev.id == 0x9325)//9325
	{
		LCD_WriteReg(0x00E5, 0x78F0); 
		LCD_WriteReg(0x0001, 0x0100); 
		LCD_WriteReg(0x0002, 0x0700); 
		LCD_WriteReg(0x0003, 0x1030); 
		LCD_WriteReg(0x0004, 0x0000); 
		LCD_WriteReg(0x0008, 0x0202);  
		LCD_WriteReg(0x0009, 0x0000);
		LCD_WriteReg(0x000A, 0x0000); 
		LCD_WriteReg(0x000C, 0x0000); 
		LCD_WriteReg(0x000D, 0x0000);
		LCD_WriteReg(0x000F, 0x0000);
		//power on sequence VGHVGL
		LCD_WriteReg(0x0010, 0x0000);   
		LCD_WriteReg(0x0011, 0x0007);  
		LCD_WriteReg(0x0012, 0x0000);  
		LCD_WriteReg(0x0013, 0x0000); 
		LCD_WriteReg(0x0007, 0x0000); 
		//vgh 
		LCD_WriteReg(0x0010, 0x1690);   
		LCD_WriteReg(0x0011, 0x0227);
		//delayms(100);
		//vregiout 
		LCD_WriteReg(0x0012, 0x009D); //0x001b
		//delayms(100); 
		//vom amplitude
		LCD_WriteReg(0x0013, 0x1900);
		//delayms(100); 
		//vom H
		LCD_WriteReg(0x0029, 0x0025); 
		LCD_WriteReg(0x002B, 0x000D); 
		//gamma
		LCD_WriteReg(0x0030, 0x0007);
		LCD_WriteReg(0x0031, 0x0303);
		LCD_WriteReg(0x0032, 0x0003);// 0006
		LCD_WriteReg(0x0035, 0x0206);
		LCD_WriteReg(0x0036, 0x0008);
		LCD_WriteReg(0x0037, 0x0406); 
		LCD_WriteReg(0x0038, 0x0304);//0200
		LCD_WriteReg(0x0039, 0x0007); 
		LCD_WriteReg(0x003C, 0x0602);// 0504
		LCD_WriteReg(0x003D, 0x0008); 
		//ram
		LCD_WriteReg(0x0050, 0x0000); 
		LCD_WriteReg(0x0051, 0x00EF);
		LCD_WriteReg(0x0052, 0x0000); 
		LCD_WriteReg(0x0053, 0x013F);  
		LCD_WriteReg(0x0060, 0xA700); 
		LCD_WriteReg(0x0061, 0x0001); 
		LCD_WriteReg(0x006A, 0x0000); 
		//
		LCD_WriteReg(0x0080, 0x0000); 
		LCD_WriteReg(0x0081, 0x0000); 
		LCD_WriteReg(0x0082, 0x0000); 
		LCD_WriteReg(0x0083, 0x0000); 
		LCD_WriteReg(0x0084, 0x0000); 
		LCD_WriteReg(0x0085, 0x0000); 
		//
		LCD_WriteReg(0x0090, 0x0010); 
		LCD_WriteReg(0x0092, 0x0600); 
		
		LCD_WriteReg(0x0007, 0x0133);
		LCD_WriteReg(0x00, 0x0022);
	}
	else if(lcddev.id == 0x9328)		//ILI9328   OK  
	{
  		LCD_WriteReg(0x00EC, 0x108F);	//internal timeing      
 		LCD_WriteReg(0x00EF, 0x1234);	//ADD        
		//LCD_WriteReg(0x00e7, 0x0010);      
        //LCD_WriteReg(0x0000, 0x0001);	//turn on the internal clock
        LCD_WriteReg(0x0001, 0x0100);     
        LCD_WriteReg(0x0002, 0x0700);	//Power on                    
		//LCD_WriteReg(0x0003, (1 << 3) | (1 << 4)); //65K  RGB
		//DRIVE TABLE (register 03H)
		//BIT3=AM BIT4:5=ID0:1
		//AM ID0 ID1   FUNCATION
		// 0  0   0	   R->L D->U
		// 1  0   0	   D->U	R->L
		// 0  1   0	   L->R D->U
		// 1  1   0    D->U	L->R
		// 0  0   1	   R->L U->D
		// 1  0   1    U->D	R->L
		// 0  1   1    L->R U->D is the normal choice.
		// 1  1   1	   U->D	L->R
        LCD_WriteReg(0x0003, (1 << 12) | (3 << 4) | (0 << 3));	//65K    
        LCD_WriteReg(0x0004, 0x0000);                                   
        LCD_WriteReg(0x0008, 0x0202);	           
        LCD_WriteReg(0x0009, 0x0000);         
        LCD_WriteReg(0x000a, 0x0000);//display setting         
        LCD_WriteReg(0x000c, 0x0001);//display setting          
        LCD_WriteReg(0x000d, 0x0000);//0f3c          
        LCD_WriteReg(0x000f, 0x0000);
		//Power configuration
        LCD_WriteReg(0x0010, 0x0000);   
        LCD_WriteReg(0x0011, 0x0007);
        LCD_WriteReg(0x0012, 0x0000);                                                                 
        LCD_WriteReg(0x0013, 0x0000);                 
     	LCD_WriteReg(0x0007, 0x0001);                 
       	delay_ms(50); 
        LCD_WriteReg(0x0010, 0x1490);   
        LCD_WriteReg(0x0011, 0x0227);
        delay_ms(50); 
        LCD_WriteReg(0x0012, 0x008A);                  
        delay_ms(50); 
        LCD_WriteReg(0x0013, 0x1a00);   
        LCD_WriteReg(0x0029, 0x0006);
        LCD_WriteReg(0x002b, 0x000d);
        delay_ms(50); 
        LCD_WriteReg(0x0020, 0x0000);                                                            
        LCD_WriteReg(0x0021, 0x0000);           
		delay_ms(50); 
		//Gamma correction
        LCD_WriteReg(0x0030, 0x0000); 
        LCD_WriteReg(0x0031, 0x0604);   
        LCD_WriteReg(0x0032, 0x0305);
        LCD_WriteReg(0x0035, 0x0000);
        LCD_WriteReg(0x0036, 0x0C09); 
        LCD_WriteReg(0x0037, 0x0204);
        LCD_WriteReg(0x0038, 0x0301);        
        LCD_WriteReg(0x0039, 0x0707);     
        LCD_WriteReg(0x003c, 0x0000);
        LCD_WriteReg(0x003d, 0x0a0a);
        delay_ms(50); 
        LCD_WriteReg(0x0050, 0x0000); //Horizontal GRAM start position 
        LCD_WriteReg(0x0051, 0x00ef); //Horizontal GRAM end position                    
        LCD_WriteReg(0x0052, 0x0000); //Vertical GRAM start position                    
        LCD_WriteReg(0x0053, 0x013f); //Vertical GRAM end position  
 
         LCD_WriteReg(0x0060, 0xa700);        
        LCD_WriteReg(0x0061, 0x0001); 
        LCD_WriteReg(0x006a, 0x0000);
        LCD_WriteReg(0x0080, 0x0000);
        LCD_WriteReg(0x0081, 0x0000);
        LCD_WriteReg(0x0082, 0x0000);
        LCD_WriteReg(0x0083, 0x0000);
        LCD_WriteReg(0x0084, 0x0000);
        LCD_WriteReg(0x0085, 0x0000);
      
        LCD_WriteReg(0x0090, 0x0010);     
        LCD_WriteReg(0x0092, 0x0600);  
        //Display-on settings    
        LCD_WriteReg(0x0007, 0x0133); 
	}
	else if(lcddev.id == 0x9320)	//Tested OK.
	{
		LCD_WriteReg(0x00, 0x0000);
		LCD_WriteReg(0x01, 0x0100);	//Driver Output Contral.
		LCD_WriteReg(0x02, 0x0700);	//LCD Driver Waveform Contral.
		LCD_WriteReg(0x03, 0x1030);	//Entry Mode Set.
		//LCD_WriteReg(0x03, 0x1018);//Entry Mode Set.
	
		LCD_WriteReg(0x04, 0x0000);	//Scalling Contral.
		LCD_WriteReg(0x08, 0x0202);	//Display Contral 2.(0x0207)
		LCD_WriteReg(0x09, 0x0000);	//Display Contral 3.(0x0000)
		LCD_WriteReg(0x0a, 0x0000);	//Frame Cycle Contal.(0x0000)
		LCD_WriteReg(0x0c, (1 << 0));//Extern Display Interface Contral 1.(0x0000)
		LCD_WriteReg(0x0d, 0x0000);	//Frame Maker Position.
		LCD_WriteReg(0x0f, 0x0000);	//Extern Display Interface Contral 2.	    
		delay_ms(50); 
		LCD_WriteReg(0x07, 0x0101);	//Display Contral.
		delay_ms(50); 								  
		LCD_WriteReg(0x10, (1 << 12) | (0 << 8) | (1 << 7) | (1 << 6) | (0 << 4));	//Power Control 1.(0x16b0)
		LCD_WriteReg(0x11, 0x0007);								//Power Control 2.(0x0001)
		LCD_WriteReg(0x12, (1 << 8) | (1 << 4) | (0 << 0));		//Power Control 3.(0x0138)
		LCD_WriteReg(0x13, 0x0b00);								//Power Control 4.
		LCD_WriteReg(0x29, 0x0000);								//Power Control 7.
	
		LCD_WriteReg(0x2b, (1 << 14) | (1 << 4));	    
		LCD_WriteReg(0x50, 0);	//Set X Star
		//Horizontal GRAM end position, Set X End.
		LCD_WriteReg(0x51, 239);	//Set Y Star
		LCD_WriteReg(0x52, 0);		//Set Y End.t.
		LCD_WriteReg(0x53, 319);	//
	
		LCD_WriteReg(0x60, 0x2700);	//Driver Output Control.
		LCD_WriteReg(0x61, 0x0001);	//Driver Output Control.
		LCD_WriteReg(0x6a, 0x0000);	//Vertical Srcoll Control.
	
		LCD_WriteReg(0x80, 0x0000);	//Display Position? Partial Display 1.
		LCD_WriteReg(0x81, 0x0000);	//RAM Address Start? Partial Display 1.
		LCD_WriteReg(0x82, 0x0000);	//RAM Address End-Partial Display 1.
		LCD_WriteReg(0x83, 0x0000);	//Displsy Position? Partial Display 2.
		LCD_WriteReg(0x84, 0x0000);	//RAM Address Start? Partial Display 2.
		LCD_WriteReg(0x85, 0x0000);	//RAM Address End? Partial Display 2.
	
		LCD_WriteReg(0x90, (0 << 7) | (16 << 0));	//Frame Cycle Contral.(0x0013)
		LCD_WriteReg(0x92, 0x0000);	//Panel Interface Contral 2.(0x0000)
		LCD_WriteReg(0x93, 0x0001);	//Panel Interface Contral 3.
		LCD_WriteReg(0x95, 0x0110);	//Frame Cycle Contral.(0x0110)
		LCD_WriteReg(0x97, (0 << 8));//
		LCD_WriteReg(0x98, 0x0000);	//Frame Cycle Contral.	   
		LCD_WriteReg(0x07, 0x0173);	//(0x0173)
	}
	else if(lcddev.id == 0X9331)	//OK |/|/|			 
	{
		LCD_WriteReg(0x00E7, 0x1014);
		LCD_WriteReg(0x0001, 0x0100); // set SS and SM bit
		LCD_WriteReg(0x0002, 0x0200); // set 1 line inversion
        LCD_WriteReg(0x0003, (1 << 12) | (3 << 4) | (1 << 3));//65K    
		//LCD_WriteReg(0x0003, 0x1030); // set GRAM write direction and BGR=1.
		LCD_WriteReg(0x0008, 0x0202); // set the back porch and front porch
		LCD_WriteReg(0x0009, 0x0000); // set non-display area refresh cycle ISC[3:0]
		LCD_WriteReg(0x000A, 0x0000); // FMARK function
		LCD_WriteReg(0x000C, 0x0000); // RGB interface setting
		LCD_WriteReg(0x000D, 0x0000); // Frame marker Position
		LCD_WriteReg(0x000F, 0x0000); // RGB interface polarity
		//*************Power On sequence ****************//
		LCD_WriteReg(0x0010, 0x0000); // SAP, BT[3:0], AP, DSTB, SLP, STB
		LCD_WriteReg(0x0011, 0x0007); // DC1[2:0], DC0[2:0], VC[2:0]
		LCD_WriteReg(0x0012, 0x0000); // VREG1OUT voltage
		LCD_WriteReg(0x0013, 0x0000); // VDV[4:0] for VCOM amplitude
		delay_ms(200); // Dis-charge capacitor power voltage
		LCD_WriteReg(0x0010, 0x1690); // SAP, BT[3:0], AP, DSTB, SLP, STB
		LCD_WriteReg(0x0011, 0x0227); // DC1[2:0], DC0[2:0], VC[2:0]
		delay_ms(50); // Delay 50ms
		LCD_WriteReg(0x0012, 0x000C); // Internal reference voltage= Vci;
		delay_ms(50); // Delay 50ms
		LCD_WriteReg(0x0013, 0x0800); // Set VDV[4:0] for VCOM amplitude
		LCD_WriteReg(0x0029, 0x0011); // Set VCM[5:0] for VCOMH
		LCD_WriteReg(0x002B, 0x000B); // Set Frame Rate
		delay_ms(50); // Delay 50ms
		LCD_WriteReg(0x0020, 0x0000); // GRAM horizontal Address
		LCD_WriteReg(0x0021, 0x013f); // GRAM Vertical Address
		// ----------- Adjust the Gamma Curve ----------//
		LCD_WriteReg(0x0030, 0x0000);
		LCD_WriteReg(0x0031, 0x0106);
		LCD_WriteReg(0x0032, 0x0000);
		LCD_WriteReg(0x0035, 0x0204);
		LCD_WriteReg(0x0036, 0x160A);
		LCD_WriteReg(0x0037, 0x0707);
		LCD_WriteReg(0x0038, 0x0106);
		LCD_WriteReg(0x0039, 0x0707);
		LCD_WriteReg(0x003C, 0x0402);
		LCD_WriteReg(0x003D, 0x0C0F);
		//------------------ Set GRAM area ---------------//
		LCD_WriteReg(0x0050, 0x0000); // Horizontal GRAM Start Address
		LCD_WriteReg(0x0051, 0x00EF); // Horizontal GRAM End Address
		LCD_WriteReg(0x0052, 0x0000); // Vertical GRAM Start Address
		LCD_WriteReg(0x0053, 0x013F); // Vertical GRAM Start Address
		LCD_WriteReg(0x0060, 0x2700); // Gate Scan Line
		LCD_WriteReg(0x0061, 0x0001); // NDL,VLE, REV 
		LCD_WriteReg(0x006A, 0x0000); // set scrolling line
		//-------------- Partial Display Control ---------//
		LCD_WriteReg(0x0080, 0x0000);
		LCD_WriteReg(0x0081, 0x0000);
		LCD_WriteReg(0x0082, 0x0000);
		LCD_WriteReg(0x0083, 0x0000);
		LCD_WriteReg(0x0084, 0x0000);
		LCD_WriteReg(0x0085, 0x0000);
		//-------------- Panel Control -------------------//
		LCD_WriteReg(0x0090, 0x0010);
		LCD_WriteReg(0x0092, 0x0600);
		LCD_WriteReg(0x0007, 0x0133); // 262K color and display ON
	}
	else if(lcddev.id == 0x5408)
	{
		LCD_WriteReg(0x01, 0x0100);								  
		LCD_WriteReg(0x02, 0x0700);	//LCD Driving Waveform Contral 
		LCD_WriteReg(0x03, 0x1030);	//Entry Mode settings 	   
		//Pointer auto-increment, left to right and top to bottom
		//Normal Mode(Window Mode disable)
		//RGB order
		//8-bit bus setting, 16-bit data in two transfers
		LCD_WriteReg(0x04, 0x0000); //Scalling Control register     
		LCD_WriteReg(0x08, 0x0207); //Display Control 2 
		LCD_WriteReg(0x09, 0x0000); //Display Control 3	 
		LCD_WriteReg(0x0A, 0x0000); //Frame Cycle Control	 
		LCD_WriteReg(0x0C, 0x0000); //External Display Interface Control 1 
		LCD_WriteReg(0x0D, 0x0000); //Frame Maker Position		 
		LCD_WriteReg(0x0F, 0x0000); //External Display Interface Control 2 
 		delay_ms(20);
		//TFT LCD colour image display method 14
		LCD_WriteReg(0x10, 0x16B0); //0x14B0 //Power Control 1
		LCD_WriteReg(0x11, 0x0001); //0x0007 //Power Control 2
		LCD_WriteReg(0x17, 0x0001); //0x0000 //Power Control 3
		LCD_WriteReg(0x12, 0x0138); //0x013B //Power Control 4
		LCD_WriteReg(0x13, 0x0800); //0x0800 //Power Control 5
		LCD_WriteReg(0x29, 0x0009); //NVM read data 2
		LCD_WriteReg(0x2a, 0x0009); //NVM read data 3
		LCD_WriteReg(0xa4, 0x0000);	 
		LCD_WriteReg(0x50, 0x0000); //Set the start column of the window on the X axis
		LCD_WriteReg(0x51, 0x00EF); //Set the end column of the window on the X axis
		LCD_WriteReg(0x52, 0x0000); //Set the start row of the window on the Y axis
		LCD_WriteReg(0x53, 0x013F); //Set the end row of the window on the Y axis
		LCD_WriteReg(0x60, 0x2700); //Driver Output Control
		//Set the number of screen lines and the scan start line
		LCD_WriteReg(0x61, 0x0001); //Driver Output Control
		LCD_WriteReg(0x6A, 0x0000); //Vertical Scroll Control
		LCD_WriteReg(0x80, 0x0000); //Display Position 每 Partial Display 1
		LCD_WriteReg(0x81, 0x0000); //RAM Address Start 每 Partial Display 1
		LCD_WriteReg(0x82, 0x0000); //RAM address End - Partial Display 1
		LCD_WriteReg(0x83, 0x0000); //Display Position 每 Partial Display 2
		LCD_WriteReg(0x84, 0x0000); //RAM Address Start 每 Partial Display 2
		LCD_WriteReg(0x85, 0x0000); //RAM address End 每 Partail Display2
		LCD_WriteReg(0x90, 0x0013); //Frame Cycle Control
		LCD_WriteReg(0x92, 0x0000);  //Panel Interface Control 2
		LCD_WriteReg(0x93, 0x0003); //Panel Interface control 3
		LCD_WriteReg(0x95, 0x0110);  //Frame Cycle Control
		LCD_WriteReg(0x07, 0x0173);		 
		delay_ms(50);
	}	
	else if(lcddev.id == 0x1505)//OK
	{
		// second release on 3/5  ,luminance is acceptable,water wave appear during camera preview
        LCD_WriteReg(0x0007, 0x0000);
        delay_ms(50); 
        LCD_WriteReg(0x0012, 0x011C);//0x011A   why need to set several times?
        LCD_WriteReg(0x00A4, 0x0001);//NVM	 
        LCD_WriteReg(0x0008, 0x000F);
        LCD_WriteReg(0x000A, 0x0008);
        LCD_WriteReg(0x000D, 0x0008);	    
  		//Gamma correction
        LCD_WriteReg(0x0030, 0x0707);
        LCD_WriteReg(0x0031, 0x0007); //0x0707
        LCD_WriteReg(0x0032, 0x0603); 
        LCD_WriteReg(0x0033, 0x0700); 
        LCD_WriteReg(0x0034, 0x0202); 
        LCD_WriteReg(0x0035, 0x0002); //?0x0606
        LCD_WriteReg(0x0036, 0x1F0F);
        LCD_WriteReg(0x0037, 0x0707); //0x0f0f  0x0105
        LCD_WriteReg(0x0038, 0x0000); 
        LCD_WriteReg(0x0039, 0x0000); 
        LCD_WriteReg(0x003A, 0x0707); 
        LCD_WriteReg(0x003B, 0x0000); //0x0303
        LCD_WriteReg(0x003C, 0x0007); //?0x0707
        LCD_WriteReg(0x003D, 0x0000); //0x1313//0x1f08
        delay_ms(50); 
        LCD_WriteReg(0x0007, 0x0001);
        LCD_WriteReg(0x0017, 0x0001);//Turn the power on
        delay_ms(50); 
  		//Power configuration
        LCD_WriteReg(0x0010, 0x17A0); 
        LCD_WriteReg(0x0011, 0x0217);//reference voltage VC[2:0]   Vciout = 1.00*Vcivl
        LCD_WriteReg(0x0012, 0x011E);//0x011c  //Vreg1out = Vcilvl*1.80   is it the same as Vgama1out ?
        LCD_WriteReg(0x0013, 0x0F00);//VDV[4:0]-->VCOM Amplitude VcomL = VcomH - Vcom Ampl
        LCD_WriteReg(0x002A, 0x0000);  
        LCD_WriteReg(0x0029, 0x000A);//0x0001F  Vcomh = VCM1[4:0]*Vreg1out    gate source voltage??
        LCD_WriteReg(0x0012, 0x013E);// 0x013C  power supply on
        //Coordinates Control//
        LCD_WriteReg(0x0050, 0x0000);//0x0e00
        LCD_WriteReg(0x0051, 0x00EF); 
        LCD_WriteReg(0x0052, 0x0000); 
        LCD_WriteReg(0x0053, 0x013F); 
    	//Pannel Image Control//
        LCD_WriteReg(0x0060, 0x2700); 
        LCD_WriteReg(0x0061, 0x0001); 
        LCD_WriteReg(0x006A, 0x0000); 
        LCD_WriteReg(0x0080, 0x0000); 
    	//Partial Image Control//
        LCD_WriteReg(0x0081, 0x0000); 
        LCD_WriteReg(0x0082, 0x0000); 
        LCD_WriteReg(0x0083, 0x0000); 
        LCD_WriteReg(0x0084, 0x0000); 
        LCD_WriteReg(0x0085, 0x0000); 
  		//Panel Interface Control//
        LCD_WriteReg(0x0090, 0x0013);//0x0010 frenqucy
        LCD_WriteReg(0x0092, 0x0300); 
        LCD_WriteReg(0x0093, 0x0005); 
        LCD_WriteReg(0x0095, 0x0000); 
        LCD_WriteReg(0x0097, 0x0000); 
        LCD_WriteReg(0x0098, 0x0000); 
  
        LCD_WriteReg(0x0001, 0x0100); 
        LCD_WriteReg(0x0002, 0x0700); 
        LCD_WriteReg(0x0003, 0x1038);//Scan direction: top->bottom, left->right 
        LCD_WriteReg(0x0004, 0x0000); 
        LCD_WriteReg(0x000C, 0x0000); 
        LCD_WriteReg(0x000F, 0x0000); 
        LCD_WriteReg(0x0020, 0x0000); 
        LCD_WriteReg(0x0021, 0x0000); 
        LCD_WriteReg(0x0007, 0x0021); 
        delay_ms(20);
        LCD_WriteReg(0x0007, 0x0061); 
        delay_ms(20);
        LCD_WriteReg(0x0007, 0x0173); 
        delay_ms(20);
	}
	else if(lcddev.id == 0xB505)
	{
		LCD_WriteReg(0x0000, 0x0000);
		LCD_WriteReg(0x0000, 0x0000);
		LCD_WriteReg(0x0000, 0x0000);
		LCD_WriteReg(0x0000, 0x0000);
		
		LCD_WriteReg(0x00a4, 0x0001);
		delay_ms(20);		  
		LCD_WriteReg(0x0060, 0x2700);
		LCD_WriteReg(0x0008, 0x0202);
		
		LCD_WriteReg(0x0030, 0x0214);
		LCD_WriteReg(0x0031, 0x3715);
		LCD_WriteReg(0x0032, 0x0604);
		LCD_WriteReg(0x0033, 0x0e16);
		LCD_WriteReg(0x0034, 0x2211);
		LCD_WriteReg(0x0035, 0x1500);
		LCD_WriteReg(0x0036, 0x8507);
		LCD_WriteReg(0x0037, 0x1407);
		LCD_WriteReg(0x0038, 0x1403);
		LCD_WriteReg(0x0039, 0x0020);
		
		LCD_WriteReg(0x0090, 0x001a);
		LCD_WriteReg(0x0010, 0x0000);
		LCD_WriteReg(0x0011, 0x0007);
		LCD_WriteReg(0x0012, 0x0000);
		LCD_WriteReg(0x0013, 0x0000);
		delay_ms(20);
		
		LCD_WriteReg(0x0010, 0x0730);
		LCD_WriteReg(0x0011, 0x0137);
		delay_ms(20);
		
		LCD_WriteReg(0x0012, 0x01b8);
		delay_ms(20);
		
		LCD_WriteReg(0x0013, 0x0f00);
		LCD_WriteReg(0x002a, 0x0080);
		LCD_WriteReg(0x0029, 0x0048);
		delay_ms(20);
		
		LCD_WriteReg(0x0001, 0x0100);
		LCD_WriteReg(0x0002, 0x0700);
        LCD_WriteReg(0x0003, 0x1038);//Scan direction: top->bottom, left->right 
		LCD_WriteReg(0x0008, 0x0202);
		LCD_WriteReg(0x000a, 0x0000);
		LCD_WriteReg(0x000c, 0x0000);
		LCD_WriteReg(0x000d, 0x0000);
		LCD_WriteReg(0x000e, 0x0030);
		LCD_WriteReg(0x0050, 0x0000);
		LCD_WriteReg(0x0051, 0x00ef);
		LCD_WriteReg(0x0052, 0x0000);
		LCD_WriteReg(0x0053, 0x013f);
		LCD_WriteReg(0x0060, 0x2700);
		LCD_WriteReg(0x0061, 0x0001);
		LCD_WriteReg(0x006a, 0x0000);
		//LCD_WriteReg(0x0080, 0x0000);
		//LCD_WriteReg(0x0081, 0x0000);
		LCD_WriteReg(0x0090, 0X0011);
		LCD_WriteReg(0x0092, 0x0600);
		LCD_WriteReg(0x0093, 0x0402);
		LCD_WriteReg(0x0094, 0x0002);
		delay_ms(20);
		
		LCD_WriteReg(0x0007, 0x0001);
		delay_ms(20);
		LCD_WriteReg(0x0007, 0x0061);
		LCD_WriteReg(0x0007, 0x0173);
		
		LCD_WriteReg(0x0020, 0x0000);
		LCD_WriteReg(0x0021, 0x0000);	  
		LCD_WriteReg(0x00, 0x22);  
	}
	else if(lcddev.id == 0xC505)
	{
		LCD_WriteReg(0x0000, 0x0000);
		LCD_WriteReg(0x0000, 0x0000);
		delay_ms(20);		  
		LCD_WriteReg(0x0000, 0x0000);
		LCD_WriteReg(0x0000, 0x0000);
		LCD_WriteReg(0x0000, 0x0000);
		LCD_WriteReg(0x0000, 0x0000);
 		LCD_WriteReg(0x00a4, 0x0001);
		delay_ms(20);		  
		LCD_WriteReg(0x0060, 0x2700);
		LCD_WriteReg(0x0008, 0x0806);
		
		LCD_WriteReg(0x0030, 0x0703);//gamma setting
		LCD_WriteReg(0x0031, 0x0001);
		LCD_WriteReg(0x0032, 0x0004);
		LCD_WriteReg(0x0033, 0x0102);
		LCD_WriteReg(0x0034, 0x0300);
		LCD_WriteReg(0x0035, 0x0103);
		LCD_WriteReg(0x0036, 0x001F);
		LCD_WriteReg(0x0037, 0x0703);
		LCD_WriteReg(0x0038, 0x0001);
		LCD_WriteReg(0x0039, 0x0004);
		
		LCD_WriteReg(0x0090, 0x0015);	//80Hz
		LCD_WriteReg(0x0010, 0X0410);	//BT,AP
		LCD_WriteReg(0x0011, 0x0247);	//DC1,DC0,VC
		LCD_WriteReg(0x0012, 0x01BC);
		LCD_WriteReg(0x0013, 0x0e00);
		delay_ms(120);
		LCD_WriteReg(0x0001, 0x0100);
		LCD_WriteReg(0x0002, 0x0200);
		LCD_WriteReg(0x0003, 0x1030);
		
		LCD_WriteReg(0x000A, 0x0008);
		LCD_WriteReg(0x000C, 0x0000);
		
		LCD_WriteReg(0x000E, 0x0020);
		LCD_WriteReg(0x000F, 0x0000);
		LCD_WriteReg(0x0020, 0x0000);	//H Start
		LCD_WriteReg(0x0021, 0x0000);	//V Start
		LCD_WriteReg(0x002A, 0x003D);	//vcom2
		delay_ms(20);
		LCD_WriteReg(0x0029, 0x002d);
		LCD_WriteReg(0x0050, 0x0000);
		LCD_WriteReg(0x0051, 0xD0EF);
		LCD_WriteReg(0x0052, 0x0000);
		LCD_WriteReg(0x0053, 0x013F);
		LCD_WriteReg(0x0061, 0x0000);
		LCD_WriteReg(0x006A, 0x0000);
		LCD_WriteReg(0x0092, 0x0300); 
 
 		LCD_WriteReg(0x0093, 0x0005);
		LCD_WriteReg(0x0007, 0x0100);
	}
	else if(lcddev.id == 0x8989)		//OK |/|/|
	{	   
		LCD_WriteReg(0x0000, 0x0001);//Start the oscillator
    	LCD_WriteReg(0x0003, 0xA8A4);//0xA8A4
    	LCD_WriteReg(0x000C, 0x0000);    
    	LCD_WriteReg(0x000D, 0x080C);   
    	LCD_WriteReg(0x000E, 0x2B00);    
    	LCD_WriteReg(0x001E, 0x00B0);    
    	LCD_WriteReg(0x0001, 0x2B3F);//Driver output control 320*240, 0x6B3F
    	LCD_WriteReg(0x0002, 0x0600);
    	LCD_WriteReg(0x0010, 0x0000);  
    	LCD_WriteReg(0x0011, 0x6078); //Data format: 16-bit colour 		landscape 0x6058
    	LCD_WriteReg(0x0005, 0x0000);  
    	LCD_WriteReg(0x0006, 0x0000);  
    	LCD_WriteReg(0x0016, 0xEF1C);  
    	LCD_WriteReg(0x0017, 0x0003);  
    	LCD_WriteReg(0x0007, 0x0233); //0x0233       
    	LCD_WriteReg(0x000B, 0x0000);  
    	LCD_WriteReg(0x000F, 0x0000); //Scan start address
    	LCD_WriteReg(0x0041, 0x0000);  
    	LCD_WriteReg(0x0042, 0x0000);  
    	LCD_WriteReg(0x0048, 0x0000);  
    	LCD_WriteReg(0x0049, 0x013F);  
    	LCD_WriteReg(0x004A, 0x0000);  
    	LCD_WriteReg(0x004B, 0x0000);  
    	LCD_WriteReg(0x0044, 0xEF00);  
    	LCD_WriteReg(0x0045, 0x0000);  
    	LCD_WriteReg(0x0046, 0x013F);  
    	LCD_WriteReg(0x0030, 0x0707);  
    	LCD_WriteReg(0x0031, 0x0204);  
    	LCD_WriteReg(0x0032, 0x0204);  
    	LCD_WriteReg(0x0033, 0x0502);  
    	LCD_WriteReg(0x0034, 0x0507);  
    	LCD_WriteReg(0x0035, 0x0204);  
    	LCD_WriteReg(0x0036, 0x0204);  
    	LCD_WriteReg(0x0037, 0x0502);  
    	LCD_WriteReg(0x003A, 0x0302);  
    	LCD_WriteReg(0x003B, 0x0302);  
    	LCD_WriteReg(0x0023, 0x0000);  
    	LCD_WriteReg(0x0024, 0x0000);  
    	LCD_WriteReg(0x0025, 0x8000);  
    	LCD_WriteReg(0x004f, 0);        //Row start address 0
    	LCD_WriteReg(0x004e, 0);        //Column start address 0
	}
	else if(lcddev.id == 0x4531)		//OK |/|/|
	{
		LCD_WriteReg(0X00, 0X0001);   
		delay_ms(10);   
		LCD_WriteReg(0X10, 0X1628);   
		LCD_WriteReg(0X12, 0X000e);//0x0006    
		LCD_WriteReg(0X13, 0X0A39);   
		delay_ms(10);   
		LCD_WriteReg(0X11, 0X0040);   
		LCD_WriteReg(0X15, 0X0050);   
		delay_ms(10);   
		LCD_WriteReg(0X12, 0X001e);//16    
		delay_ms(10);   
		LCD_WriteReg(0X10, 0X1620);   
		LCD_WriteReg(0X13, 0X2A39);   
		delay_ms(10);   
		LCD_WriteReg(0X01, 0X0100);   
		LCD_WriteReg(0X02, 0X0300);   
		LCD_WriteReg(0X03, 0X1038);//Direction change   
		LCD_WriteReg(0X08, 0X0202);   
		LCD_WriteReg(0X0A, 0X0008);   
		LCD_WriteReg(0X30, 0X0000);   
		LCD_WriteReg(0X31, 0X0402);   
		LCD_WriteReg(0X32, 0X0106);   
		LCD_WriteReg(0X33, 0X0503);   
		LCD_WriteReg(0X34, 0X0104);   
		LCD_WriteReg(0X35, 0X0301);   
		LCD_WriteReg(0X36, 0X0707);   
		LCD_WriteReg(0X37, 0X0305);   
		LCD_WriteReg(0X38, 0X0208);   
		LCD_WriteReg(0X39, 0X0F0B);   
		LCD_WriteReg(0X41, 0X0002);   
		LCD_WriteReg(0X60, 0X2700);   
		LCD_WriteReg(0X61, 0X0001);   
		LCD_WriteReg(0X90, 0X0210);   
		LCD_WriteReg(0X92, 0X010A);   
		LCD_WriteReg(0X93, 0X0004);   
		LCD_WriteReg(0XA0, 0X0100);   
		LCD_WriteReg(0X07, 0X0001);   
		LCD_WriteReg(0X07, 0X0021);   
		LCD_WriteReg(0X07, 0X0023);   
		LCD_WriteReg(0X07, 0X0033);   
		LCD_WriteReg(0X07, 0X0133);   
		LCD_WriteReg(0XA0, 0X0000); 
	}
	else if(lcddev.id == 0x4535)
	{			      
		LCD_WriteReg(0X15, 0X0030);   
		LCD_WriteReg(0X9A, 0X0010);   
 		LCD_WriteReg(0X11, 0X0020);   
 		LCD_WriteReg(0X10, 0X3428);   
		LCD_WriteReg(0X12, 0X0002);//16    
 		LCD_WriteReg(0X13, 0X1038);   
		delay_ms(40);   
		LCD_WriteReg(0X12, 0X0012);//16    
		delay_ms(40);   
  		LCD_WriteReg(0X10, 0X3420);   
 		LCD_WriteReg(0X13, 0X3038);   
		delay_ms(70);   
		LCD_WriteReg(0X30, 0X0000);   
		LCD_WriteReg(0X31, 0X0402);   
		LCD_WriteReg(0X32, 0X0307);   
		LCD_WriteReg(0X33, 0X0304);   
		LCD_WriteReg(0X34, 0X0004);   
		LCD_WriteReg(0X35, 0X0401);   
		LCD_WriteReg(0X36, 0X0707);   
		LCD_WriteReg(0X37, 0X0305);   
		LCD_WriteReg(0X38, 0X0610);   
		LCD_WriteReg(0X39, 0X0610); 
		  
		LCD_WriteReg(0X01, 0X0100);   
		LCD_WriteReg(0X02, 0X0300);   
		LCD_WriteReg(0X03, 0X1030);//Direction change   
		LCD_WriteReg(0X08, 0X0808);   
		LCD_WriteReg(0X0A, 0X0008);   
 		LCD_WriteReg(0X60, 0X2700);   
		LCD_WriteReg(0X61, 0X0001);   
		LCD_WriteReg(0X90, 0X013E);   
		LCD_WriteReg(0X92, 0X0100);   
		LCD_WriteReg(0X93, 0X0100);   
 		LCD_WriteReg(0XA0, 0X3000);   
 		LCD_WriteReg(0XA3, 0X0010);   
		LCD_WriteReg(0X07, 0X0001);   
		LCD_WriteReg(0X07, 0X0021);   
		LCD_WriteReg(0X07, 0X0023);   
		LCD_WriteReg(0X07, 0X0033);   
		LCD_WriteReg(0X07, 0X0133);   
	}

	LCD_Display_Dir(0);		 		//Portrait by default
	LCD_LED = 1;					//Turn the backlight on
	LCD_Clear(WHITE);
}
 
//Screen clear function
//color: the colour to clear the screen to
void LCD_Clear(u16 color)
{
	u32 index = 0;      
	u32 totalpoint = lcddev.width;
	totalpoint *= lcddev.height; 					//Work out the total number of pixels

	if((lcddev.id == 0X6804) && (lcddev.dir == 1))	//Special handling for the 6804 in landscape  
	{						    
 		lcddev.dir = 0;	 
 		lcddev.setxcmd = 0X2A;
		lcddev.setycmd = 0X2B;  	 			
		LCD_SetCursor(0x00, 0x0000);				//Set the cursor position  
 		lcddev.dir = 1;	 
  		lcddev.setxcmd = 0X2B;
		lcddev.setycmd = 0X2A;  	 
 	}
	else
	{
		LCD_SetCursor(0x00,0x0000);					//Set the cursor position 
	}

	LCD_WriteRAM_Prepare();     					//Start writing to GRAM	 	  
	for(index = 0; index < totalpoint; index++)
	{
		LCD->LCD_RAM = color;	   
	}
}
  
//Fill an area with a single colour
//(sx,sy),(ex,ey): opposite corners of the rectangle to fill; the area is (ex-sx+1)*(ey-sy+1)   
//color: the fill colour
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color)
{          
	u16 i, j;
	u16 xlen = 0;
	u16 temp;

	if((lcddev.id == 0X6804) && (lcddev.dir == 1))	//Special handling for the 6804 in landscape  
	{
		temp = sx;
		sx = sy;
		sy = lcddev.width - ex - 1;	  
		ex = ey;
		ey = lcddev.width - temp - 1;
 		lcddev.dir = 0;	 
 		lcddev.setxcmd = 0X2A;
		lcddev.setycmd = 0X2B;  	 			
		LCD_Fill(sx, sy, ex, ey, color);  
 		lcddev.dir = 1;	 
  		lcddev.setxcmd = 0X2B;
		lcddev.setycmd = 0X2A;  	 
 	}
	else
	{
		xlen = ex - sx + 1;	 
		for(i = sy; i <= ey; i++)
		{
		 	LCD_SetCursor(sx, i);      				//Set the cursor position 
			LCD_WriteRAM_Prepare();     			//Start writing to GRAM	  
			for(j = 0; j < xlen; j++)
				LCD_WR_DATA(color);		    
		}
	}	 
}
 
//Fill an area with a block of colours			 
//(sx,sy),(ex,ey): opposite corners of the rectangle to fill; the area is (ex-sx+1)*(ey-sy+1)   
//color: the fill colour
void LCD_Color_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 *color)
{  
	u16 height, width;
	u16 i, j;
	width = ex - sx + 1; 				//Work out the fill width
	height = ey - sy + 1;				//Height
 	for(i = 0; i < height; i++)
	{
 		LCD_SetCursor(sx, sy + i);   	//Set the cursor position 
		LCD_WriteRAM_Prepare();     	//Start writing to GRAM
		for(j = 0; j < width; j++)
			LCD->LCD_RAM = color[i * height + j];//Write data 
	}	  
}
  
//Draw a line
//x1,y1: start coordinates
//x2,y2: end coordinates  
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
{
	u16 t; 
	int xerr = 0, yerr = 0, delta_x, delta_y, distance; 
	int incx, incy, uRow, uCol;
	 
	delta_x = x2 - x1; 					//Work out the coordinate deltas 
	delta_y = y2 - y1; 
	uRow = x1; 
	uCol = y1; 
	if(delta_x > 0)
		incx = 1; 						//Set the step direction 
	else if(delta_x == 0)
		incx = 0;						//Vertical line 
	else
	{
		incx = -1;
		delta_x = -delta_x;
	}
	 
	if(delta_y > 0)
		incy = 1; 
	else if(delta_y == 0)
		incy = 0;						//Horizontal line 
	else
	{
		incy = -1;
		delta_y = -delta_y;
	}
	 
	if(delta_x > delta_y)
		distance = delta_x; 			//Pick the axis with the larger step 
	else
		distance = delta_y;
		 
	for(t = 0; t <= distance + 1; t++ )	//Draw the line 
	{  
		LCD_DrawPoint(uRow, uCol);		//Draw a point 
		xerr += delta_x ; 
		yerr += delta_y ; 
		if(xerr > distance) 
		{ 
			xerr -= distance; 
			uRow += incx; 
		} 
		if(yerr > distance) 
		{ 
			yerr -= distance; 
			uCol += incy; 
		} 
	}  
}
   
//Draw a rectangle	  
//(x1,y1),(x2,y2): opposite corners of the rectangle
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
{
	LCD_DrawLine(x1, y1, x2, y1);
	LCD_DrawLine(x1, y1, x1, y2);
	LCD_DrawLine(x1, y2, x2, y2);
	LCD_DrawLine(x2, y1, x2, y2);
}

//Draw a circle of a given size at a given position
//(x,y): the centre point
//r    : radius
void Draw_Circle(u16 x0, u16 y0, u8 r)
{
	int a, b;
	int di;

	a = 0;
	b = r;	  
	di = 3 - (r << 1);             			//Flag used to decide where the next point goes
	while(a <= b)
	{
		LCD_DrawPoint(x0 + a, y0 - b);             //5
 		LCD_DrawPoint(x0 + b, y0 - a);             //0           
		LCD_DrawPoint(x0 + b, y0 + a);             //4               
		LCD_DrawPoint(x0 + a, y0 + b);             //6 
		LCD_DrawPoint(x0 - a, y0 + b);             //1       
 		LCD_DrawPoint(x0 - b, y0 + a);             //3
		LCD_DrawPoint(x0 - a, y0 - b);             //2             
  		LCD_DrawPoint(x0 - b, y0 - a);             //7     	         
		a++;

		//Draw the circle with Bresenham's algorithm     
		if(di < 0)
		{
			di += 4 * a + 6;
		}	  
		else
		{
			di += 10 + 4 * (a - b);   
			b--;
		} 						    
	}
}
 									  
//Show one character at the given position
//x,y: start coordinates
//num: the character to show, " "--->"~"
//size: font size, 12 or 16
//mode: overlay (1) or replace (0)
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u8 mode)
{  							  
    u8 temp, t1, t;
	u16 y0 = y;
	u16 colortemp = POINT_COLOR;      			     
	//Set the window		   
	num = num - ' ';//Get the offset value
	if(!mode) //Replace mode
	{
	    for(t = 0; t < size; t++)
	    {   
			if(size == 12)
				temp = asc2_1206[num][t];	//Use the 12x06 font
			else
				temp = asc2_1608[num][t];	//Use the 16x08 font 	                          

	        for(t1 = 0; t1 < 8; t1++)
			{			    
		        if(temp & 0x80)
					POINT_COLOR = colortemp;
				else
					POINT_COLOR = BACK_COLOR;

				LCD_DrawPoint(x, y);	
				temp <<= 1;
				y++;

				if(x >= lcddev.width)		//Outside the area
				{
					POINT_COLOR = colortemp;
					return;
				}

				if((y - y0) == size)
				{
					y = y0;
					x++;
					if(x >= lcddev.width)	//Outside the area
					{
						POINT_COLOR = colortemp;
						return;
					}
					break;
				}
			}  	 
	    }    
	}
	else	//Overlay mode
	{
	    for(t = 0; t < size; t++)
	    {   
			if(size == 12)
				temp = asc2_1206[num][t];	//Use the 12x06 font
			else
				temp = asc2_1608[num][t];	//Use the 16x08 font
				 	                          
	        for(t1 = 0; t1 < 8; t1++)
			{			    
		        if(temp & 0x80)
					LCD_DrawPoint(x, y);
					 
				temp <<= 1;
				y++;

				if(x >= lcddev.height)		//Outside the area
				{
					POINT_COLOR = colortemp;
					return;
				}

				if((y - y0) == size)
				{
					y = y0;
					x++;

					if(x >= lcddev.width)	//Outside the area
					{
						POINT_COLOR = colortemp;
						return;
					}
					break;
				}
			}  	 
	    }     
	}

	POINT_COLOR = colortemp;	    	   	 	  
}
   
//m raised to the power n
//Return: m raised to the power n.
u32 LCD_Pow(u8 m, u8 n)
{
	u32 result = 1;	 
	while(n--)
		result *= m;    
	return result;
}
			 
//Show a number, suppressing leading zeros
//x,y : start coordinates	 
//len : number of digits
//size: font size
//color: colour 
//num: the value (0~4294967295);	 
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size)
{         	
	u8 t, temp;
	u8 enshow = 0;
							   
	for(t = 0; t < len; t++)
	{
		temp = (num / LCD_Pow(10, len - t - 1)) % 10;
		if(enshow == 0 && t < (len - 1))
		{
			if(temp == 0)
			{
				LCD_ShowChar(x + (size / 2) * t, y, ' ', size, 0);
				continue;
			}
			else
			{
				enshow = 1;
			}		 	 
		}

	 	LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, 0); 
	}
}

//Show a number, keeping leading zeros
//x,y: start coordinates
//num: the value (0~999999999);	 
//len: length, i.e. how many digits to show
//size: font size
//mode:
//[7]: 0 = no padding; 1 = pad with zeros.
//[6:1]: reserved
//[0]: 0 = replace; 1 = overlay.
void LCD_ShowxNum(u16 x, u16 y, u32 num, u8 len, u8 size, u8 mode)
{  
	u8 t, temp;
	u8 enshow = 0;						   
	for(t = 0; t < len; t++)
	{
		temp = (num / LCD_Pow(10, len - t - 1)) % 10;
		if(enshow == 0 && t < (len - 1))
		{
			if(temp == 0)
			{
				if(mode & 0X80)
					LCD_ShowChar(x + (size / 2) * t, y, '0', size, mode & 0X01);  
				else
					LCD_ShowChar(x + (size / 2) * t, y, ' ', size, mode & 0X01); 

 				continue;
			}
			else
			{
				enshow = 1;
			}		 	 
		}

	 	LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, mode & 0X01); 
	}
}

//Show a string
//x,y: start coordinates
//width,height: the size of the area  
//size: font size
//*p: start of the string		  
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, char *p)
{         
	u8 x0 = x;
	width += x;
	height += y;
    while((*p <= '~') && (*p >= ' '))	//Check for an invalid character!
    {       
        if(x >= width)
		{
			x = x0;
			y += size;
		}

        if(y >= height)
			break;						//Exit

        LCD_ShowChar(x, y, *p, size, 0);
        x += size / 2;
        p++;
    }  
}
