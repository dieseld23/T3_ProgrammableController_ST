#ifndef __TOUCH_H__
#define __TOUCH_H__

#include "bitmap.h"


#define TP_PRES_DOWN 0x80  //The screen is being touched	  
#define TP_CATH_PRES 0x40  //Something has been pressed 	  
										    
//Touch screen controller
typedef struct
{
	u8 (*init)(void);			//Initialise the touch screen controller
	u8 (*scan)(u8);				//Scan the touch screen. 0 = screen coordinates; 1 = physical coordinates;	 
	void (*adjust)(void);		//Touch screen calibration
	u16 x0;						//The original coordinates, from the first press
	u16 y0;
	u16 x; 						//The current coordinates, from this scan
	u16 y;						   	    
	u8  sta;					//Pen state 
								//b7: 1 = pressed, 0 = released; 
	                            //b6: 0 = nothing pressed; 1 = something pressed.         			  
////////////////////////touch screen calibration parameters/////////////////////////								
	float xfac;					
	float yfac;
	short xoff;
	short yoff;	   
//A new parameter, needed when the touch screen is completely reversed left-right and top-bottom.
//touchtype=0 suits a panel where left-right is X and top-bottom is Y.
//touchtype=1 suits a panel where left-right is Y and top-bottom is X.
	u8 touchtype;
}_m_tp_dev;

extern _m_tp_dev tp_dev;	 	//The touch controller is defined in touch.c

//Pins connected to the touch screen chip	   
#define TCS  			PBout(7)  	//PB7  CS 
#define PEN  			PBin(6)  	//PB6 INT
#define TDIN			PAout(7)	//PA7 MOSI
#define	DOUT			PAin(6)		//PA6 MISO
#define TCLK			PAout(5)	//PA5 CLK
     

void TP_Write_Byte(u8 num);							//Write one value to the controller
u16 TP_Read_AD(u8 CMD);								//Read an ADC conversion
u16 TP_Read_XOY(u8 xy);								//Filtered coordinate read (X/Y)
u8 TP_Read_XY(u16 *x, u16 *y);						//Read both directions (X+Y)
u8 TP_Read_XY2(u16 *x, u16 *y);						//Read both directions with stronger filtering
void TP_Drow_Touch_Point(u16 x, u16 y, u16 color);	//Draw a calibration point
void TP_Draw_Big_Point(u16 x, u16 y, u16 color);	//Draw a large point
u8 TP_Scan(u8 tp);									//Scan
void TP_Save_Adjdata(void);							//Save the calibration parameters
u8 TP_Get_Adjdata(void);							//Read the calibration parameters
void TP_Adjust(void);								//Touch screen calibration
u8 TP_Init(void);									//Initialise
																 
void TP_Adj_Info_Show(u16 x0, u16 y0, u16 x1, u16 y1, u16 x2, u16 y2, u16 x3, u16 y3, u16 fac);//Show the calibration details
 		  
#endif
