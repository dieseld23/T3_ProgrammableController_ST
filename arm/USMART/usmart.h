#ifndef __USMART_H
#define __USMART_H	  		  
#include "usmart_str.h"
//////////////////////////////////////////////////////////////////////////////////	 
//This code is for study use only and may not be used for any other purpose without the author's permission
//ALIENTEK STM32 development board	   
//ALIENTEK
//Support forum: www.openedv.com 
//Version: V3.1
//All rights reserved.
//Copyright(C) ALIENTEK 2011-2021
//All rights reserved
//********************************************************************************
//Change log
//V1.4
//Added support for functions taking string parameters, which widens the range of use considerably.
//Reduced memory use: 79 bytes of static memory at 10 parameters, adapting dynamically to number and string lengths
//V2.0 
//1, Changed the list command to print the full function expression.
//2, Added the id command, which prints each function's entry address.
//3, Changed parameter matching to support calling a function as a parameter (by entry address).
//4, Added a macro for the function name length.	
//V2.1 20110707		 
//1, Added the dec and hex commands, which set the parameter display base and perform base conversion.
//Note: with no argument, dec/hex set the display base; with an argument they perform a base conversion.
//e.g. "dec 0XFF" converts 0XFF to 255 and returns it over the serial port.
//e.g. "hex 100" 	converts 100 to 0X64 and returns it over the serial port
//2, Added the usmart_get_cmdname function, which fetches a command name.
//V2.2 20110726	
//1, Fixed the parameter count being wrong for void parameters.
//2, Changed the default data display format to hexadecimal.
//V2.3 20110815
//1, Removed the rule that a function name must be followed by "(".
//2, Fixed the bug where a string parameter could not contain "(".
//3, Changed how a function's default parameter display format is set. 
//V2.4 20110905
//1, Changed usmart_get_cmdname to cap the maximum parameter length, which stops the hang seen when a bad parameter is entered.
//2, Added the USMART_ENTIM2_SCAN macro, which selects whether TIM2 is used to run the scan function periodically.
//V2.5 20110930
//1, Changed usmart_init to void usmart_init(u8 sysclk) so the scan interval is set automatically from the system clock (fixed at 100ms).
//2, Removed the uart_init call from usmart_init; the serial port must now be initialised externally so the user can manage it.
//V2.6 20111009
//1, Added the read_addr and write_addr functions, which read and write any internal address (it must be a valid one). Handy for debugging.
//2, read_addr and write_addr can be enabled or disabled through USMART_USE_WRFUNS.
//3, Tidied up usmart_strcmp.			  
//V2.7 20111024
//1, Fixed the missing newline when a return value is shown in hexadecimal.
//2, Added a check for whether a function has a return value; the value is only shown when there is one.
//V2.8 20111116
//1, Fixed the hang that could follow an argument-less command such as list.
//V2.9 20120917
//1, Fixed the bug where functions of the form void*xxx(void) were not recognised.
//V3.0 20130425
//1, Added escape-character support in string parameters.
//V3.1 20131120
//1, Added the runtime system command, which measures function execution time.
//Usage:
//Send "runtime 1" to turn function timing on
//Send "runtime 0" to turn function timing off
///runtime timing feature: USMART_ENTIMX_SCAN must be 1 for this to work!!
/////////////////////////////////////////////////////////////////////////////////////
//USMART resource use, MDK 3.80A, version 2.0:
//FLASH: 4K bytes or less, depending on USMART_USE_HELP and USMART_USE_WRFUNS
//SRAM: 72 bytes at minimum
//SRAM = PARM_LEN+72-4, where PARM_LEN must be at least 4.
//The stack should be at least 100 bytes.
////////////////////////////////////////////user configuration////////////////////////////////////////////////////	  
#define MAX_FNAME_LEN 		30	//The maximum function name length, which must be at least as long as the longest name.											   
#define MAX_PARM 			10	//10 arguments at most; changing this means changing usmart_exe to match.
#define PARM_LEN 			200	//All the arguments together must fit in PARM_LEN bytes, and the serial receive buffer must be at least that big


#define USMART_ENTIMX_SCAN 	1	//Use a timer interrupt to run the scan function; set to 0 and you must call scan periodically yourself.
								//Note: the runtime timing feature needs USMART_ENTIMX_SCAN set to 1!!!!
								
#define USMART_USE_HELP		1	//Help text; setting this to 0 saves nearly 700 bytes but means no help can be shown.
#define USMART_USE_WRFUNS	1	//Read/write functions; enabling this lets you read any address and write registers.
///////////////////////////////////////////////END///////////////////////////////////////////////////////////

#define USMART_OK 			0  //No error
#define USMART_FUNCERR 		1  //Function error
#define USMART_PARMERR 		2  //Parameter error
#define USMART_PARMOVER 	3  //Argument overflow
#define USMART_NOFUNCFIND 	4  //No matching function

#define SP_TYPE_DEC      	0  //Show parameters in decimal
#define SP_TYPE_HEX       	1  //Show parameters in hexadecimal


 //Function name list	 
struct _m_usmart_nametab
{
	void* func;			//Function pointer
	const u8* name;		//Function name (the search string)	 
};
//usmart control manager
struct _m_usmart_dev
{
	struct _m_usmart_nametab *funs;	//Pointer to the function name

	void (*init)(u8);				//Initialise
	u8 (*cmd_rec)(u8*str);			//Recognise the function name and arguments
	void (*exe)(void); 				//Execute 
	void (*scan)(void);             //Scan
	u8 fnum; 				  		//Number of functions
	u8 pnum;                        //Number of parameters
	u8 id;							//Function id
	u8 sptype;						//Argument display type for non-string arguments: 0 = decimal; 1 = hexadecimal;
	u16 parmtype;					//The argument types
	u8  plentbl[MAX_PARM];  		//Scratch table of each argument's length
	u8  parm[PARM_LEN];  			//The function's arguments
	u8 runtimeflag;					//0 = do not time the function; 1 = time it. This only works when USMART_ENTIMX_SCAN is enabled
	u32 runtime;					//Run time in units of 0.1ms; the longest measurable time is 2*CNT*0.1ms
};
extern struct _m_usmart_nametab usmart_nametab[];	//Defined in usmart_config.c
extern struct _m_usmart_dev usmart_dev;				//Defined in usmart_config.c


void usmart_init(u8 sysclk);//Initialise
u8 usmart_cmd_rec(u8*str);	//Recognise
void usmart_exe(void);		//Execute
void usmart_scan(void);     //Scan
u32 read_addr(u32 addr);	//Read the value at a given address
void write_addr(u32 addr,u32 val);//Write a given value to a given address
u32 usmart_get_runtime(void);	//Read the run time
void usmart_reset_runtime(void);//Reset the run time

#endif






























