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
//USMART资源占用情况@MDK 3.80A@2.0版本：
//FLASH:4K~K字节(通过USMART_USE_HELP和USMART_USE_WRFUNS设置)
//SRAM:72字节(最少的情况下)
//SRAM计算公式:   SRAM=PARM_LEN+72-4  其中PARM_LEN必须大于等于4.
//应该保证堆栈不小于100个字节.
////////////////////////////////////////////用户配置参数////////////////////////////////////////////////////	  
#define MAX_FNAME_LEN 		30	//函数名最大长度，应该设置为不小于最长函数名的长度。											   
#define MAX_PARM 			10	//最大为10个参数 ,修改此参数,必须修改usmart_exe与之对应.
#define PARM_LEN 			200	//所有参数之和的长度不超过PARM_LEN个字节,注意串口接收部分要与之对应(不小于PARM_LEN)


#define USMART_ENTIMX_SCAN 	1	//使用TIM的定时中断来扫描SCAN函数,如果设置为0,需要自己实现隔一段时间扫描一次scan函数.
								//注意:如果要用runtime统计功能,必须设置USMART_ENTIMX_SCAN为1!!!!
								
#define USMART_USE_HELP		1	//使用帮助，该值设为0，可以节省近700个字节，但是将导致无法显示帮助信息。
#define USMART_USE_WRFUNS	1	//使用读写函数,使能这里,可以读取任何地址的值,还可以写寄存器的值.
///////////////////////////////////////////////END///////////////////////////////////////////////////////////

#define USMART_OK 			0  //无错误
#define USMART_FUNCERR 		1  //Function error
#define USMART_PARMERR 		2  //Parameter error
#define USMART_PARMOVER 	3  //参数溢出
#define USMART_NOFUNCFIND 	4  //未找到匹配函数

#define SP_TYPE_DEC      	0  //Show parameters in decimal
#define SP_TYPE_HEX       	1  //Show parameters in hexadecimal


 //函数名列表	 
struct _m_usmart_nametab
{
	void* func;			//函数指针
	const u8* name;		//函数名(查找串)	 
};
//usmart控制管理器
struct _m_usmart_dev
{
	struct _m_usmart_nametab *funs;	//函数名指针

	void (*init)(u8);				//Initialise
	u8 (*cmd_rec)(u8*str);			//识别函数名及参数
	void (*exe)(void); 				//Execute 
	void (*scan)(void);             //Scan
	u8 fnum; 				  		//Number of functions
	u8 pnum;                        //Number of parameters
	u8 id;							//函数id
	u8 sptype;						//参数显示类型(非字符串参数):0,10进制;1,16进制;
	u16 parmtype;					//参数的类型
	u8  plentbl[MAX_PARM];  		//每个参数的长度暂存表
	u8  parm[PARM_LEN];  			//函数的参数
	u8 runtimeflag;					//0,不统计函数执行时间;1,统计函数执行时间,注意:此功能必须在USMART_ENTIMX_SCAN使能的时候,才有用
	u32 runtime;					//运行时间,单位:0.1ms,最大延时时间为定时器CNT值的2倍*0.1ms
};
extern struct _m_usmart_nametab usmart_nametab[];	//Defined in usmart_config.c
extern struct _m_usmart_dev usmart_dev;				//Defined in usmart_config.c


void usmart_init(u8 sysclk);//Initialise
u8 usmart_cmd_rec(u8*str);	//识别
void usmart_exe(void);		//Execute
void usmart_scan(void);     //Scan
u32 read_addr(u32 addr);	//Read the value at a given address
void write_addr(u32 addr,u32 val);//Write a given value to a given address
u32 usmart_get_runtime(void);	//获取运行时间
void usmart_reset_runtime(void);//复位运行时间

#endif






























