#include "touch.h" 
#include "lcd.h"
#include "delay.h"
#include "stdlib.h"
#include "math.h"
#include "24cxx.h"
#include "spi.h"
#include "usart.h"

//#define SIMULATE_TOUCH_TIMING

_m_tp_dev tp_dev =
{
	TP_Init,
	TP_Scan,
	TP_Adjust,
	0,
	0,
 	0,
	0,
	0,
	0,
	0,
	0,	  	 		
	0,
	0,	  	 		
};

//Defaults to the touchtype = 0 data.
u8 CMD_RDX = 0XD0;
u8 CMD_RDY = 0X90;
 	 			    					   
//SPI write
//Write one byte to the touch screen IC    
//num: the data to write
void TP_Write_Byte(u8 num)    
{
#ifndef SIMULATE_TOUCH_TIMING
	SPI1_ReadWriteByte(num);
#else
	u8 count = 0;   
	for(count = 0; count < 8; count++)  
	{ 	  
		if(num & 0x80)
			TDIN=1;  
		else
			TDIN=0;
		
		num <<= 1;    
		TCLK = 0;	 
		TCLK = 1;		//Active on the rising edge	        
	}
#endif	
}

//SPI read 
//Read an ADC value from the touch screen IC
//CMD: the command
//Return: the data read	   
u16 TP_Read_AD(u8 CMD)	  
{
#ifndef SIMULATE_TOUCH_TIMING  
	u16 Num = 0; 
	TCS = 0; 						//Select the touch screen IC
	TP_Write_Byte(CMD);				//Send the command word
	delay_us(6);					//The ADS7846 takes at most 6us to convert
	
	Num = SPI1_ReadWriteByte(0x00);	//7 valid bits in the high byte
	Num <<= 8;
	Num |= SPI1_ReadWriteByte(0x00);//5 valid bits in the low byte
	
	Num >>= 3;   					//Only 12 bits are valid.
	TCS = 1;						//Release the chip select	

	return(Num);
#else
	u8 count = 0; 	  
	u16 Num = 0; 
	TCLK = 0;			//Pull the clock low first 	 
	TDIN = 0; 			//Pull the data line low
	TCS = 0; 			//Select the touch screen IC
	TP_Write_Byte(CMD);	//Send the command word
	delay_us(6);		//The ADS7846 takes at most 6us to convert
	TCLK = 0; 	     	    
	delay_us(1);    	   
	TCLK = 1;			//One clock to clear BUSY	    	    
	TCLK = 0;
	
	for(count = 0; count < 16; count++)//Read 16 bits, of which only the top 12 are valid 
	{ 				  
		Num <<= 1; 	 
		TCLK = 0;	//Active on the falling edge  	    	   
		TCLK = 1;
		if(DOUT)Num++; 		 
	}
 	
	Num >>= 4;   	//Only the top 12 bits are valid.
	TCS = 1;		//Release the chip select
	
	return(Num);
#endif
}

//Read one coordinate, x or y
//Take READ_TIMES readings, sort them into ascending order,
//then drop the lowest and highest LOST_VAL of them and average the rest 
//xy: the command (CMD_RDX/CMD_RDY)
//Return: the data read
#define READ_TIMES	5 	//Number of readings
#define LOST_VAL	1	//Readings to discard
u16 TP_Read_XOY(u8 xy)
{
	u16 i, j;
	u16 buf[READ_TIMES];
	u16 sum = 0;
	u16 temp;
	for(i = 0; i < READ_TIMES; i++)
		buf[i] = TP_Read_AD(xy);		 		    
	for(i = 0; i < READ_TIMES - 1; i++)//Sort
	{
		for(j = i + 1; j < READ_TIMES; j++)
		{
			if(buf[i] > buf[j])//Ascending order
			{
				temp = buf[i];
				buf[i] = buf[j];
				buf[j] = temp;
			}
		}
	}	  
	sum = 0;
	for(i = LOST_VAL; i < READ_TIMES - LOST_VAL; i++)
		sum += buf[i];
	
	temp = sum / (READ_TIMES - 2 * LOST_VAL);
	return temp;   
}

//Read the x and y coordinates
//The minimum must not be below 100.
//x,y: the coordinates read back
//Return: 0 = failure; 1 = success.
u8 TP_Read_XY(u16 *x, u16 *y)
{
	u16 xtemp, ytemp;			 	 		  
	xtemp = TP_Read_XOY(CMD_RDX);
	ytemp = TP_Read_XOY(CMD_RDY);	  												   
	//if(xtemp < 100 || ytemp < 100) return 0;//reading failed
	*x = xtemp;
	*y = ytemp;
	return 1;//Reading succeeded
}

//Read the touch screen IC twice, and the two readings must differ by no
//more than ERR_RANGE; if they do, the reading is taken as good, otherwise bad.	   
//This raises the accuracy considerably
//x,y: the coordinates read back
//Return: 0 = failure; 1 = success.
#define	ERR_RANGE	50 //Error range 
u8 TP_Read_XY2(u16 *x, u16 *y) 
{
	u16 x1, y1;
 	u16 x2, y2;
 	u8 flag;    
    flag = TP_Read_XY(&x1, &y1);   
    if(flag == 0)
		return 0;
	
    flag = TP_Read_XY(&x2, &y2);	   
    if(flag == 0)
		return 0;
	
    if((((x2 <= x1) && (x1 < (x2 + ERR_RANGE))) || ((x1 <= x2) && (x2 < (x1 + ERR_RANGE))))//The two samples are within +-50 of each other
		&& (((y2 <= y1) && (y1 < (y2 + ERR_RANGE))) || ((y1 <= y2) && (y2 < (y1 + ERR_RANGE)))))
    {
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return 1;
    }
	else
		return 0;
}

//////////////////////////////////////////////////////////////////////////////////		  
//Functions that involve the LCD  
//Draw a touch point
//Used for calibration
//x,y: coordinates
//color: colour
void TP_Drow_Touch_Point(u16 x, u16 y, u16 color)
{
	POINT_COLOR = color;
	LCD_DrawLine(x - 12, y, x + 13, y);//Horizontal line
	LCD_DrawLine(x, y - 12, x, y + 13);//Vertical line
	LCD_DrawPoint(x + 1, y + 1);
	LCD_DrawPoint(x - 1, y + 1);
	LCD_DrawPoint(x + 1, y - 1);
	LCD_DrawPoint(x - 1, y - 1);
	Draw_Circle(x, y, 6);//Draw the centre ring
}

//Draw a large point (2*2 pixels)		   
//x,y: coordinates
//color: colour
void TP_Draw_Big_Point(u16 x, u16 y, u16 color)
{	    
	POINT_COLOR = color;
	LCD_DrawPoint(x, y);//Centre point 
	LCD_DrawPoint(x + 1, y);
	LCD_DrawPoint(x, y + 1);
	LCD_DrawPoint(x + 1, y + 1);	 	  	
}

//////////////////////////////////////////////////////////////////////////////////		  
//Touch scan
//tp: 0 = screen coordinates; 1 = physical coordinates (for calibration and the like)
//Return: the current touch state.
//0 = not touched; 1 = touched
u8 TP_Scan(u8 tp)
{			   
	if(PEN == 0)//Something is pressed
	{
		if(tp)
		{
			TP_Read_XY2(&tp_dev.x, &tp_dev.y);//Read the physical coordinates
		}
		else if(TP_Read_XY2(&tp_dev.x, &tp_dev.y))//Read the screen coordinates
		{
	 		tp_dev.x = tp_dev.xfac * tp_dev.x + tp_dev.xoff;//Convert the result into screen coordinates
			tp_dev.y = tp_dev.yfac * tp_dev.y + tp_dev.yoff;  
	 	}
		 
		if((tp_dev.sta & TP_PRES_DOWN) == 0)//It was not pressed before
		{		 
			tp_dev.sta = TP_PRES_DOWN | TP_CATH_PRES;//Pressed  
			tp_dev.x0 = tp_dev.x;//Record the coordinates of the first press
			tp_dev.y0 = tp_dev.y;
		}
	}
	else
	{
		if(tp_dev.sta & TP_PRES_DOWN)//It was pressed before
		{
			tp_dev.sta &= ~TP_PRES_DOWN;//Mark it as released	
		}
		else//It was not pressed before either
		{
			tp_dev.x0 = 0;
			tp_dev.y0 = 0;
			tp_dev.x = 0xffff;
			tp_dev.y = 0xffff;
		}	    
	}
	
	return tp_dev.sta & TP_PRES_DOWN;//Return the current touch state
}

//////////////////////////////////////////////////////////////////////////	 
//Base of the EEPROM area used, 13 bytes (SAVE_ADDR_BASE~SAVE_ADDR_BASE+12)
#define SAVE_ADDR_BASE 40
//Save the calibration parameters										    
void TP_Save_Adjdata(void)
{
	s32 temp;			 
	//Save the calibration results!		   							  
	temp = tp_dev.xfac * 100000000;//Save the x scale factor      
    AT24CXX_WriteLenByte(SAVE_ADDR_BASE, temp, 4);   
	temp = tp_dev.yfac * 100000000;//Save the y scale factor    
    AT24CXX_WriteLenByte(SAVE_ADDR_BASE + 4, temp, 4);
	//Save the x offset
    AT24CXX_WriteLenByte(SAVE_ADDR_BASE + 8, tp_dev.xoff, 2);		    
	//Save the y offset
	AT24CXX_WriteLenByte(SAVE_ADDR_BASE + 10, tp_dev.yoff, 2);	
	//Save the touch screen type
	AT24CXX_WriteOneByte(SAVE_ADDR_BASE + 12, tp_dev.touchtype);	
	temp = 0X0A;//Mark it as calibrated
	AT24CXX_WriteOneByte(SAVE_ADDR_BASE + 13, temp); 
}

//Read the calibration values back from EEPROM
//Return: 1, the data was read
//        0, it could not be read and calibration must be redone
u8 TP_Get_Adjdata(void)
{					  
	s32 tempfac;
	tempfac = AT24CXX_ReadOneByte(SAVE_ADDR_BASE + 13);	//Read the flag word to see whether it has been calibrated! 		 
	if(tempfac == 0X0A)									//The touch screen has already been calibrated			   
	{    												 
		tempfac = AT24CXX_ReadLenByte(SAVE_ADDR_BASE, 4);		   
		tp_dev.xfac = (float)tempfac / 100000000;		//Read the x calibration factor
		tempfac = AT24CXX_ReadLenByte(SAVE_ADDR_BASE + 4, 4);			          
		tp_dev.yfac = (float)tempfac / 100000000;		//Read the y calibration factor
	    //Read the x offset
		tp_dev.xoff = AT24CXX_ReadLenByte(SAVE_ADDR_BASE + 8, 2);			   	  
 	    //Read the y offset
		tp_dev.yoff = AT24CXX_ReadLenByte(SAVE_ADDR_BASE + 10, 2);				 	  
 		tp_dev.touchtype = AT24CXX_ReadOneByte(SAVE_ADDR_BASE + 12);//Read the touch screen type flag
		if(tp_dev.touchtype)	//X and Y run opposite to the screen
		{
			CMD_RDX = 0X90;
			CMD_RDY = 0XD0;	 
		}
		else					//X and Y run the same way as the screen
		{
			CMD_RDX = 0XD0;
			CMD_RDY = 0X90;	 
		}		 
		return 1;	 
	}
	return 0;
}

//Prompt string
const char *TP_REMIND_MSG_TBL = "Please use the stylus click the cross on the screen.The cross will always move until the screen adjustment is completed.";
 					  
//Show the calibration results
void TP_Adj_Info_Show(u16 x0, u16 y0, u16 x1, u16 y1, u16 x2, u16 y2, u16 x3, u16 y3, u16 fac)
{	  
	POINT_COLOR = RED;
	LCD_ShowString(40, 160, lcddev.width, lcddev.height, 16, "x1:");
 	LCD_ShowString(40 + 80, 160, lcddev.width, lcddev.height, 16, "y1:");
 	LCD_ShowString(40, 180, lcddev.width, lcddev.height, 16, "x2:");
 	LCD_ShowString(40 + 80, 180, lcddev.width, lcddev.height, 16, "y2:");
	LCD_ShowString(40, 200, lcddev.width, lcddev.height, 16, "x3:");
 	LCD_ShowString(40 + 80, 200, lcddev.width, lcddev.height, 16, "y3:");
	LCD_ShowString(40, 220, lcddev.width, lcddev.height, 16, "x4:");
 	LCD_ShowString(40 + 80, 220, lcddev.width, lcddev.height, 16, "y4:");  
 	LCD_ShowString(40, 240, lcddev.width, lcddev.height, 16, "fac is:");     
	LCD_ShowNum(40 + 24, 160, x0, 4, 16);		//Display the value
	LCD_ShowNum(40 + 24 + 80, 160, y0, 4, 16);	//Display the value
	LCD_ShowNum(40 + 24, 180, x1, 4, 16);		//Display the value
	LCD_ShowNum(40 + 24 + 80, 180, y1, 4, 16);	//Display the value
	LCD_ShowNum(40 + 24, 200, x2, 4, 16);		//Display the value
	LCD_ShowNum(40 + 24 + 80, 200, y2, 4, 16);	//Display the value
	LCD_ShowNum(40 + 24, 220, x3, 4, 16);		//Display the value
	LCD_ShowNum(40 + 24 + 80, 220, y3, 4, 16);	//Display the value
 	LCD_ShowNum(40 + 56, lcddev.width, fac, 3, 16); //Show the value, which must lie between 95 and 105.
}
		 
//Touch screen calibration
//Work out the four calibration parameters
void TP_Adjust(void)
{								 
	u16 pos_temp[4][2];//Coordinate buffer
	u8 cnt = 0;	
	u16 d1, d2;
	u32 tem1, tem2;
	float fac; 	
	u16 outtime = 0;
 	cnt = 0;				
	POINT_COLOR = BLUE;
	BACK_COLOR = WHITE;
	LCD_Clear(WHITE);//Clear the screen   
	POINT_COLOR = RED;//Red 
	LCD_Clear(WHITE);//Clear the screen 	   
	POINT_COLOR = BLACK;
	LCD_ShowString(40, 40, 160, 100, 16, (char *)TP_REMIND_MSG_TBL);//Show the prompt
	TP_Drow_Touch_Point(20, 20, RED);//Draw point 1 
	tp_dev.sta = 0;//Clear the trigger 
	tp_dev.xfac = 0;//xfac marks whether calibration has been done, so it must be cleared first to avoid errors	 
	while(1)//Give up automatically after 10 seconds with no press
	{
		tp_dev.scan(1);//Scan the physical coordinates
		if((tp_dev.sta & 0xc0) == TP_CATH_PRES)	//One press has happened (and has now been released.)
		{	
			outtime = 0;		
			tp_dev.sta &= ~TP_CATH_PRES;		//Mark the press as handled.
						   			   
			pos_temp[cnt][0] = tp_dev.x;
			pos_temp[cnt][1] = tp_dev.y;
			cnt++;	  
			switch(cnt)
			{			   
				case 1:						 
					TP_Drow_Touch_Point(20, 20, WHITE);				//Clear point 1 
					TP_Drow_Touch_Point(lcddev.width - 20, 20, RED);	//Draw point 2
					break;
				case 2:
 					TP_Drow_Touch_Point(lcddev.width - 20, 20, WHITE);	//Clear point 2
					TP_Drow_Touch_Point(20, lcddev.height - 20, RED);	//Draw point 3
					break;
				case 3:
 					TP_Drow_Touch_Point(20, lcddev.height - 20, WHITE);			//Clear point 3
 					TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, RED);	//Draw point 4
					break;
				case 4:	 //All four points have been captured
	    		    //Opposite sides are equal
					tem1 = abs(pos_temp[0][0] - pos_temp[1][0]);//x1-x2
					tem2 = abs(pos_temp[0][1] - pos_temp[1][1]);//y1-y2
					tem1 *= tem1;
					tem2 *= tem2;
					d1 = sqrt(tem1 + tem2);//Distance between 1 and 2
					
					tem1 = abs(pos_temp[2][0] - pos_temp[3][0]);//x3-x4
					tem2 = abs(pos_temp[2][1] - pos_temp[3][1]);//y3-y4
					tem1 *= tem1;
					tem2 *= tem2;
					d2 = sqrt(tem1 + tem2);//Distance between 3 and 4
				
					fac = (float)d1/d2;
					if(fac < 0.95 || fac > 1.05 || d1 == 0 || d2 == 0)//Failed
					{
						cnt = 0;
 				    	TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, WHITE);	//Clear point 4
   	 					TP_Drow_Touch_Point(20, 20, RED);								//Draw point 1
 						TP_Adj_Info_Show(pos_temp[0][0], pos_temp[0][1], pos_temp[1][0], pos_temp[1][1], pos_temp[2][0], pos_temp[2][1], pos_temp[3][0], pos_temp[3][1], fac * 100);//Display the data   
 						continue;
					}
					
					tem1 = abs(pos_temp[0][0] - pos_temp[2][0]);//x1-x3
					tem2 = abs(pos_temp[0][1] - pos_temp[2][1]);//y1-y3
					tem1 *= tem1;
					tem2 *= tem2;
					d1 = sqrt(tem1 + tem2);//Distance between 1 and 3
					
					tem1 = abs(pos_temp[1][0] - pos_temp[3][0]);//x2-x4
					tem2 = abs(pos_temp[1][1] - pos_temp[3][1]);//y2-y4
					tem1 *= tem1;
					tem2 *= tem2;
					d2 = sqrt(tem1 + tem2);//Distance between 2 and 4
					
					fac = (float)d1/d2;
					if(fac < 0.95 || fac > 1.05)//Failed
					{
						cnt = 0;
 				    	TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, WHITE);	//Clear point 4
   	 					TP_Drow_Touch_Point(20, 20, RED);								//Draw point 1
 						TP_Adj_Info_Show(pos_temp[0][0], pos_temp[0][1], pos_temp[1][0], pos_temp[1][1], pos_temp[2][0], pos_temp[2][1], pos_temp[3][0], pos_temp[3][1], fac * 100);//Display the data   
						continue;
					}//Correct now
								   
					//The diagonals are equal
					tem1 = abs(pos_temp[1][0] - pos_temp[2][0]);//x1-x3
					tem2 = abs(pos_temp[1][1] - pos_temp[2][1]);//y1-y3
					tem1 *= tem1;
					tem2 *= tem2;
					d1 = sqrt(tem1 + tem2);//Distance between 1 and 4
	
					tem1 = abs(pos_temp[0][0] - pos_temp[3][0]);//x2-x4
					tem2 = abs(pos_temp[0][1] - pos_temp[3][1]);//y2-y4
					tem1 *= tem1;
					tem2 *= tem2;
					d2 = sqrt(tem1 + tem2);//Distance between 2 and 3
					
					fac = (float)d1/d2;
					if(fac < 0.95 || fac > 1.05)//Failed
					{
						cnt = 0;
 				    	TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, WHITE);	//Clear point 4
   	 					TP_Drow_Touch_Point(20, 20, RED);								//Draw point 1
 						TP_Adj_Info_Show(pos_temp[0][0], pos_temp[0][1], pos_temp[1][0], pos_temp[1][1], pos_temp[2][0], pos_temp[2][1], pos_temp[3][0], pos_temp[3][1], fac * 100);//Display the data   
						continue;
					}//Correct now
					
					//Work out the result
					tp_dev.xfac = (float)(lcddev.width - 40) / (pos_temp[1][0] - pos_temp[0][0]);//Work out xfac		 
					tp_dev.xoff = (lcddev.width - tp_dev.xfac * (pos_temp[1][0] + pos_temp[0][0])) / 2;//Work out xoff
						  
					tp_dev.yfac = (float)(lcddev.height - 40) / (pos_temp[2][1] - pos_temp[0][1]);//Work out yfac
					tp_dev.yoff = (lcddev.height - tp_dev.yfac * (pos_temp[2][1] + pos_temp[0][1])) / 2;//Work out yoff  
					if(abs(tp_dev.xfac) > 2 || abs(tp_dev.yfac) > 2)//The touch screen is the other way round from what was assumed.
					{
						cnt = 0;
 				    	TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, WHITE);	//Clear point 4
   	 					TP_Drow_Touch_Point(20, 20, RED);								//Draw point 1
						LCD_ShowString(40, 26, lcddev.width, lcddev.height, 16, "TP Need readjust!");
						tp_dev.touchtype = !tp_dev.touchtype;//Change the touch screen type.
						if(tp_dev.touchtype)//X and Y run opposite to the screen
						{
							CMD_RDX = 0X90;
							CMD_RDY = 0XD0;	 
						}else				   //X and Y run the same way as the screen
						{
							CMD_RDX = 0XD0;
							CMD_RDY = 0X90;	 
						}			    
						continue;
					}		
					POINT_COLOR = BLUE;
					LCD_Clear(WHITE);//Clear the screen
					LCD_ShowString(35, 110, lcddev.width, lcddev.height, 16, "Touch Screen Adjust OK!");//Calibration complete
					delay_ms(1000);
					TP_Save_Adjdata();  
 					LCD_Clear(WHITE);//Clear the screen   
					return;//Calibration complete				 
			}
		}
		delay_ms(10);
		outtime++;
		if(outtime > 1000)
		{
			TP_Get_Adjdata();
			break;
	 	} 
 	}
}

//Touch screen initialisation  		    
//Return: 0, not calibrated
//       1, calibrated
u8 TP_Init(void)
{			    		   
//	RCC->APB2ENR |= (1 << 3);			//enable the PB clock, PB6-INT, PB7-CS
//	GPIOB->CRL &= 0X00FFFFFF; 
//	GPIOB->CRL |= 0X38000000;
//	GPIOB->ODR |= (1 << 6) | (1 << 7);
//
//#ifdef SIMULATE_TOUCH_TIMING
//	RCC->APB2ENR |= (1 << 2);			//enable the PA clock
//	GPIOA->CRL &= 0X000FFFFF; 
//	GPIOA->CRL |= 0X38300000;
//	GPIOA->ODR |= (1 << 5) | (1 << 6) | (1 << 7);
//#endif
//
////	GPIOA->CRL &= 0XFFF0FFFF;			//PA4-ETH-CS1 = 1
////	GPIOA->CRL |= 0X00030000;
////	GPIOA->ODR |= (1 << 4);
//
////	RCC->APB2ENR |= (1 << 4);
////	GPIOC->CRL &= 0XFFF0FFFF;			//PC4-FLASH-CS2 = 1
////	GPIOC->CRL |= 0X00030000;
////	GPIOC->ODR |= (1 << 4);
//
//	GPIOB->CRH &= 0XFFF0FFFF;			//PB12-VS1003-CS4 = 1
//	GPIOB->CRH |= 0X00030000;
//	GPIOB->ODR |= (1 << 12);

	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	//Enable the PORTB clock

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;			//PB7-CS
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  	//PB7 push-pull output 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_Init(GPIOB, &GPIO_InitStructure);				//Initialise GPIOA
	GPIO_SetBits(GPIOB, GPIO_Pin_7);  					//Drive PB7 high

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;			//PB6-INT
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  		//PB6 pull-up input
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_Init(GPIOB, &GPIO_InitStructure);				//Initialise GPIOA
	GPIO_SetBits(GPIOB, GPIO_Pin_6);  					//PB6 pull-up


	//The three IOs below are driven high to disable the other SPI devices
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//Enable the PORTA, PORTB and PORTC clocks
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;			//PA4--ETH-CS1
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  	//PA4 push-pull output 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_Init(GPIOA, &GPIO_InitStructure);				//Initialise GPIOA
	GPIO_SetBits(GPIOA, GPIO_Pin_4);  					//Drive PB7 high

//	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);	//enable the PORTC clock
//	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;			//PC4--FLASH-CS2
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  	//PC4 push-pull output 
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
//	GPIO_Init(GPIOC, &GPIO_InitStructure);				//initialise GPIOC
//	GPIO_SetBits(GPIOC, GPIO_Pin_4);  					//drive PC4 high

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	//Enable the PORTB clock
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;			//PB12--VS1003-CS4
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  	//PB12 push-pull output 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_Init(GPIOB, &GPIO_InitStructure);				//Initialise GPIOA
	GPIO_SetBits(GPIOB, GPIO_Pin_12);  					//Drive PB12 high
	//////////////////////////////////////////////////////////////////////
	
  	TP_Read_XY(&tp_dev.x, &tp_dev.y);	//First read initialises it	 
	if(TP_Get_Adjdata())
	{
		return 0;						//Already calibrated
	}
	else								//Not calibrated?
	{ 										    
		LCD_Clear(WHITE);				//Clear the screen
	    TP_Adjust();					//Screen calibration 
		TP_Save_Adjdata();	 
	}			
	TP_Get_Adjdata();	
	return 1; 									 
}
