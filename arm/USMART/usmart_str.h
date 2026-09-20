#ifndef __USMART_STR_H
#define __USMART_STR_H	 
#include "stm32f10x.h"
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
 
 
u8 usmart_get_parmpos(u8 num);						//得到某个参数在参数列里面的起始位置
u8 usmart_strcmp(u8*str1,u8 *str2);					//对比两个字符串是否相等
u32 usmart_pow(u8 m,u8 n);							//M^N次方
u8 usmart_str2num(u8*str,u32 *res);					//字符串转为数字
u8 usmart_get_cmdname(u8*str,u8*cmdname,u8 *nlen,u8 maxlen);//从str中得到指令名,并返回指令长度
u8 usmart_get_fname(u8*str,u8*fname,u8 *pnum,u8 *rval);		//Extract the function name from str
u8 usmart_get_aparm(u8 *str,u8 *fparm,u8 *ptype); 	//从str中得到一个函数参数
u8 usmart_get_fparam(u8*str,u8 *parn);  			//得到str中所有的函数参数.
#endif











