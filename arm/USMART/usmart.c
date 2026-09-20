#include "usmart.h"
#include "usart.h"
//#include "bitmap.h"
 

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
//System commands
u8 *sys_cmd_tab[]=
{
	"?",
	"help",
	"list",
	"id",
	"hex",
	"dec",
	"runtime",	   
};

//Handle a system command
//0 = handled; other = error code;
u8 usmart_sys_cmd_exe(u8 *str)
{
	u8 i;
	u8 sfname[MAX_FNAME_LEN];	//Holds the local function name
	u8 pnum;
	u8 rval;
	u32 res;  
	res = usmart_get_cmdname(str, sfname, &i, MAX_FNAME_LEN);	//Get the command and its length
	if(res)
		return USMART_FUNCERR;	//Bad command 
	
	str += i;	 	 			    
	for(i = 0; i < sizeof(sys_cmd_tab) / 4; i++)				//The system commands that are supported
	{
		if(usmart_strcmp(sfname, sys_cmd_tab[i]) == 0)
			break;
	}
	
	switch(i)
	{					   
		case 0:
		case 1:	//help command
			printf("\r\n");
#if USMART_USE_HELP
			printf("------------------------USMART V3.1------------------------ \r\n");
			printf("    USMART is a neat serial debugging component from ALIENTEK. With \r\n");
			printf("it you can call and run any function in your program from a serial \r\n");
			printf("terminal, so you can freely change a function's inputs (numbers in \r\n");	  
			printf("decimal or hex, strings and entry addresses), up to 10 per call, and \r\n");
			printf("the return value is shown. Display base and base conversion are new.\r\n");
			printf("Support: www.openedv.com\r\n");
			printf("USMART has 7 system commands:\r\n");
			printf("?:      show this help\r\n");
			printf("help:   show this help\r\n");
			printf("list:   list the available functions\r\n\n");
			printf("id:     list the function IDs\r\n\n");
			printf("hex:    show arguments in hex; follow with a space and a number to convert\r\n\n");
			printf("dec:    show arguments in decimal; follow with a space and a number to convert\r\n\n");
			printf("runtime:1 turns function timing on; 0 turns it off;\r\n\n");
			printf("Type the function name and arguments as they are written in the code, then press Enter.\r\n");    
			printf("--------------------------ALIENTEK------------------------- \r\n");
#else
			printf("Command failed\r\n");
#endif
			break;
		case 2:	//list command
			printf("\r\n");
			printf("-------------------------function list--------------------------- \r\n");
			for(i = 0; i < usmart_dev.fnum; i++)
				printf("%s\r\n", usmart_dev.funs[i].name);
			printf("\r\n");
			break;	 
		case 3:	//query the ID
			printf("\r\n");
			printf("-------------------------function ID --------------------------- \r\n");
			for(i = 0; i < usmart_dev.fnum; i++)
			{
				usmart_get_fname((u8*)usmart_dev.funs[i].name, sfname, &pnum,&rval);	//Get the local function name 
				printf("%s id is:\r\n0X%08X\r\n", sfname, usmart_dev.funs[i].func); 	//Show the ID
			}
			printf("\r\n");
			break;
		case 4:	//hex command
			printf("\r\n");
			usmart_get_aparm(str, sfname, &i);
			if(i == 0)	//Parameters are valid
			{
				i = usmart_str2num(sfname, &res);	   	//Record this parameter	
				if(i == 0)							  	//Base conversion
				{
					printf("HEX:0X%X\r\n", res);	   	//Convert to hexadecimal
				}
				else if(i != 4)
				{
					return USMART_PARMERR;				//Parameter error.
				}
				else 				   					//Parameter display setting
				{
					printf("Arguments shown in hex!\r\n");
					usmart_dev.sptype = SP_TYPE_HEX;  
				}

			}
			else
				return USMART_PARMERR;					//Parameter error.
			printf("\r\n"); 
			break;
		case 5:	//dec command
			printf("\r\n");
			usmart_get_aparm(str, sfname, &i);
			if(i == 0)	//Parameters are valid
			{
				i = usmart_str2num(sfname, &res);	   	//Record this parameter	
				if(i == 0)						   		//Base conversion
				{
					printf("DEC:%lu\r\n", res);	   		//Convert to decimal
				}
				else if(i != 4)
				{
					return USMART_PARMERR;				//Parameter error.
				}
				else 				   					//Parameter display setting
				{
					printf("Arguments shown in decimal!\r\n");
					usmart_dev.sptype = SP_TYPE_DEC;  
				}

			}
			else
				return USMART_PARMERR;			//Parameter error. 
			printf("\r\n"); 
			break;	 
		case 6:	//runtime command, which turns the function timing display on and off
			printf("\r\n");
			usmart_get_aparm(str, sfname, &i);
			if(i == 0)	//Parameters are valid
			{
				i = usmart_str2num(sfname, &res);	   	//Record this parameter	
				if(i == 0)						   		//Read the data at a given address
				{
					if(USMART_ENTIMX_SCAN == 0)
					{
						printf("\r\nError! \r\nTo EN RunTime function,Please set USMART_ENTIMX_SCAN = 1 first!\r\n");//Report the error
					}
					else
					{
						usmart_dev.runtimeflag = res;
						if(usmart_dev.runtimeflag)
							printf("Run Time Calculation ON\r\n");
						else
							printf("Run Time Calculation OFF\r\n"); 
					}
				}
				else
					return USMART_PARMERR;   			//No argument, or a bad one	 
 			}
			else 
				return USMART_PARMERR;					//Parameter error. 
			printf("\r\n"); 
			break;	    
		default:	//Invalid command
			return USMART_FUNCERR;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
//Porting note: this is written for the STM32; adapt it for another MCU.
//usmart_reset_runtime clears the measured run time along with the timer count and flags, and sets the reload value to its maximum so the timing window is as long as possible.
//usmart_get_runtime reads the run time from CNT. Because usmart calls the function from an interrupt, the timer interrupt is no longer served, so at most
//two CNT spans can be counted, that is one clear plus one overflow. More than two overflows cannot be handled, so the longest measurable time is 2*CNT*0.1ms, about 13.1s on the STM32
//TIM2_IRQHandler and Timer2_Init must be adapted to the MCU; just keep the counter running at 10kHz. Do not enable timer auto-reload!!

#if USMART_ENTIMX_SCAN == 1
//Reset the run time
//Must be adjusted to the timer parameters of whichever MCU this is ported to
void usmart_reset_runtime(void)
{
	TIM2->SR &= ~(1 << 0);	//Clear the interrupt flag 
	TIM2->ARR = 0XFFFF;		//Set the reload value to its maximum
	TIM2->CNT = 0;			//Clear the timer count
	usmart_dev.runtime = 0;	
}

//Read the measured run time
//Return: the run time in units of 0.1ms; the longest measurable time is 2*CNT*0.1ms
//Must be adjusted to the timer parameters of whichever MCU this is ported to
u32 usmart_get_runtime(void)
{
	if(TIM2->SR & 0X0001)	//The timer overflowed while it was running
	{
		usmart_dev.runtime += 0XFFFF;
	}
	usmart_dev.runtime += TIM2->CNT;
	return usmart_dev.runtime;		//Return the count
}

//The two functions below are not part of USMART; they are here only to make porting easier. 
//Timer 2 interrupt service routine	 
void TIM2_IRQHandler(void)
{ 		    		  			    
	if(TIM2->SR & 0X0001)	//Overflow interrupt
	{ 
		usmart_dev.scan();	//Run the usmart scan	
		TIM2->CNT = 0;		//Clear the timer count
		TIM2->ARR = 1000;	//Restore the original settings
	}				   
	TIM2->SR &= ~(1 << 0);	//Clear the interrupt flag 	    
}

//Enable timer 2 and its interrupt.
void Timer2_Init(u16 arr, u16 psc)
{
	RCC->APB1ENR |= 1 << 0;	//Enable the TIM2 clock    
 	TIM2->ARR = arr;  		//Set the auto-reload value  
	TIM2->PSC = psc;  		//A prescaler of 7200 gives a 10kHz count clock
	//Both of these have to be set before the interrupt works
	TIM2->DIER |= 1 << 0;   //Allow the update interrupt				
	TIM2->DIER |= 1 << 6;   //Allow the trigger interrupt
		  							    
	TIM2->CR1 |= 0x01;		//Enable timer 2
  	MY_NVIC_Init(3, 3, TIM2_IRQn, 2);//Pre-emption 3, sub-priority 3, group 2 (the lowest priority in group 2)									 
}
#endif
////////////////////////////////////////////////////////////////////////////////////////
//Initialise the serial controller
//sysclk: the system clock in MHz
void usmart_init(u8 sysclk)
{
#if USMART_ENTIMX_SCAN==1
	Timer2_Init(1000, (u32)sysclk * 100 - 1);	//Divide down to 10kHz and interrupt every 100ms. The count frequency must be 10kHz so it matches the 0.1ms runtime unit.
#endif
	usmart_dev.sptype = 1;	//Show arguments in hexadecimal
}

//Pull the function name, id and arguments out of str
//*str: pointer to the string.
//Return: 0 = recognised; other = error code.
u8 usmart_cmd_rec(u8 *str) 
{
	u8 sta, i, rval;//State	 
	u8 rpnum,spnum;
	u8 rfname[MAX_FNAME_LEN];//Scratch space for the function name received  
	u8 sfname[MAX_FNAME_LEN];//Holds the local function name
	sta=usmart_get_fname(str,rfname,&rpnum,&rval);//Get the function name and argument count from what was received	  
	if(sta)return sta;//Error
	for(i=0;i<usmart_dev.fnum;i++)
	{
		sta=usmart_get_fname((u8*)usmart_dev.funs[i].name,sfname,&spnum,&rval);//Get the local function name and argument count
		if(sta)return sta;//The local parse failed	  
		if(usmart_strcmp(sfname,rfname)==0)//Equal
		{
			if(spnum>rpnum)return USMART_PARMERR;//Argument error (fewer arguments given than the function takes)
			usmart_dev.id=i;//Record the function ID.
			break;//Break out.
		}	
	}
	if(i==usmart_dev.fnum)return USMART_NOFUNCFIND;	//No matching function was found
 	sta=usmart_get_fparam(str,&i);					//Get the function's argument count	
	if(sta)return sta;								//Return the error
	usmart_dev.pnum=i;								//Record the argument count
    return USMART_OK;
}
//usmart function execution
//This is what finally calls the function received over the serial port.
//Up to 10 arguments are supported. More would be easy to add but are rarely needed; even five-argument functions are uncommon.
//It prints the result to the serial port as "name(arg1, arg2...argN)=return value".
//When the function has no return value, the value printed is meaningless.
void usmart_exe(void)
{
	u8 id,i;
	u32 res;		   
	u32 temp[MAX_PARM];//Argument conversion, which adds string support 
	u8 sfname[MAX_FNAME_LEN];//Holds the local function name
	u8 pnum,rval;
	id=usmart_dev.id;
	if(id>=usmart_dev.fnum)return;//Do not run it.
	usmart_get_fname((u8*)usmart_dev.funs[id].name,sfname,&pnum,&rval);//Get the local function name and argument count 
	printf("\r\n%s(",sfname);//Print the name of the function about to run
	for(i=0;i<pnum;i++)//Print the arguments
	{
		if(usmart_dev.parmtype&(1<<i))//The argument is a string
		{
			printf("%c",'"');			 
			printf("%s",usmart_dev.parm+usmart_get_parmpos(i));
			printf("%c",'"');
			temp[i]=(u32)&(usmart_dev.parm[usmart_get_parmpos(i)]);
		}else						  //The argument is a number
		{
			temp[i]=*(u32*)(usmart_dev.parm+usmart_get_parmpos(i));
			if(usmart_dev.sptype==SP_TYPE_DEC)printf("%lu",temp[i]);//Show parameters in decimal
			else printf("0X%X",temp[i]);//Show parameters in hexadecimal 	   
		}
		if(i!=pnum-1)printf(",");
	}
	printf(")");
	usmart_reset_runtime();	//Clear the timer and start timing
	switch(usmart_dev.pnum)
	{
		case 0://No arguments (void)											  
			res=(*(u32(*)())usmart_dev.funs[id].func)();
			break;
	    case 1://1 argument
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0]);
			break;
	    case 2://2 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1]);
			break;
	    case 3://3 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2]);
			break;
	    case 4://4 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3]);
			break;
	    case 5://5 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4]);
			break;
	    case 6://6 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],\
			temp[5]);
			break;
	    case 7://7 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],\
			temp[5],temp[6]);
			break;
	    case 8://8 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],\
			temp[5],temp[6],temp[7]);
			break;
	    case 9://9 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],\
			temp[5],temp[6],temp[7],temp[8]);
			break;
	    case 10://10 arguments
			res=(*(u32(*)())usmart_dev.funs[id].func)(temp[0],temp[1],temp[2],temp[3],temp[4],\
			temp[5],temp[6],temp[7],temp[8],temp[9]);
			break;
	}
	usmart_get_runtime();//Read the function's run time
	if(rval==1)//A return value is wanted.
	{
		if(usmart_dev.sptype==SP_TYPE_DEC)printf("=%lu;\r\n",res);//Print the result with arguments in decimal
		else printf("=0X%X;\r\n",res);//Print the result with arguments in hexadecimal	   
	}else printf(";\r\n");		//No return value wanted, so just finish the line
	if(usmart_dev.runtimeflag)	//The run time is to be shown
	{ 
		printf("Function Run Time:%d.%1dms\r\n",usmart_dev.runtime/10,usmart_dev.runtime%10);//Print the function's run time 
	}	
}
//usmart scan function
//Calling this drives everything usmart does. It must be called periodically
//so that functions sent over the serial port run promptly.
//It can be called from an interrupt so it looks after itself.
//Outside ALIENTEK's own code you must provide USART_RX_STA and USART_RX_BUF[] yourself
void usmart_scan(void)
{
	u8 sta,len;  
	if(USART_RX_STA&0x8000)//Has the serial receive finished?
	{					   
		len=USART_RX_STA&0x3fff;	//Get the length of what was received
		USART_RX_BUF[len]='\0';	//Append the terminator. 
		sta=usmart_dev.cmd_rec(USART_RX_BUF);//Work out the details of the function
		if(sta==0)usmart_dev.exe();	//Run the function 
		else 
		{  
			len=usmart_sys_cmd_exe(USART_RX_BUF);
			if(len!=USMART_FUNCERR)sta=len;
			if(sta)
			{
				switch(sta)
				{
					case USMART_FUNCERR:
						printf("Bad function!\r\n");   			
						break;	
					case USMART_PARMERR:
						printf("Bad argument!\r\n");   			
						break;				
					case USMART_PARMOVER:
						printf("Too many arguments!\r\n");   			
						break;		
					case USMART_NOFUNCFIND:
						printf("No matching function found!\r\n");   			
						break;		
				}
			}
		}
		USART_RX_STA=0;//Clear the state register	    
	}
}

#if USMART_USE_WRFUNS==1 	//If the read/write operations are enabled
//Read the value at a given address		 
u32 read_addr(u32 addr)
{
	return *(u32*)addr;//	
}

//Write a given value to a given address		 
void write_addr(u32 addr,u32 val)
{
	*(u32*)addr=val; 	
}
#endif
