#include "usmart_str.h"
#include "usmart.h"		   
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
  
//Compare the strings str1 and str2
//*str1: pointer to string 1
//*str2: pointer to string 2
//Return: 0 = equal; 1 = not equal;
u8 usmart_strcmp(u8 *str1,u8 *str2)
{
	while(1)
	{
		if(*str1!=*str2)return 1;//Not equal
		if(*str1=='\0')break;//Comparison finished.
		str1++;
		str2++;
	}
	return 0;//The two strings are equal
}
//Copy str1 into str2
//*str1: pointer to string 1
//*str2: pointer to string 2			   
void usmart_strcopy(u8*str1,u8 *str2)
{
	while(1)
	{										   
		*str2=*str1;	//Copy
		if(*str1=='\0')break;//Copy finished.
		str1++;
		str2++;
	}
}
//Get the length of the string in bytes
//*str: pointer to the string
//Return: the length of the string		   
u8 usmart_strlen(u8*str)
{
	u8 len=0;
	while(1)
	{							 
		if(*str=='\0')break;//Copy finished.
		len++;
		str++;
	}
	return len;
}
//m raised to the power n
//Return: m raised to the power n
u32 usmart_pow(u8 m,u8 n)
{
	u32 result=1;	 
	while(n--)result*=m;    
	return result;
}	    
//Convert a string into a number
//Hexadecimal is supported, but the letters must be upper case and it must start with 0X.
//Negative numbers are not supported 
//*str: pointer to the number string
//*res: where the converted result is stored.
//Return: 0 = converted successfully; other = error code.
//1 = bad data format. 2 = no hexadecimal digits. 3 = bad prefix. 4 = no decimal digits.
u8 usmart_str2num(u8*str,u32 *res)
{
	u32 t;
	u8 bnum=0;	//Number of digits
	u8 *p;		  
	u8 hexdec=10;//Decimal by default
	p=str;
	*res=0;//Clear it.
	while(1)
	{
		if((*p<='9'&&*p>='0')||(*p<='F'&&*p>='A')||(*p=='X'&&bnum==1))//The argument is valid
		{
			if(*p>='A')hexdec=16;	//The string contains letters, so it is hexadecimal.
			bnum++;					//One more digit.
		}else if(*p=='\0')break;	//Hit the terminator, so stop.
		else return 1;				//Not all of it is decimal or hexadecimal.
		p++; 
	} 
	p=str;			    //Go back to the start of the string.
	if(hexdec==16)		//Hexadecimal
	{
		if(bnum<3)return 2;			//Fewer than 3 characters, so stop: 0X takes two of them, and 0X with nothing after it is invalid.
		if(*p=='0' && (*(p+1)=='X'))//It must start with '0X'.
		{
			p+=2;	//Move to the start of the digits.
			bnum-=2;//Subtract the offset	 
		}else return 3;//The prefix is wrong
	}else if(bnum==0)return 4;//No digits, so stop.	  
	while(1)
	{
		if(bnum)bnum--;
		if(*p<='9'&&*p>='0')t=*p-'0';	//Work out the value
		else t=*p-'A'+10;				//Work out the value of A~F	    
		*res+=t*usmart_pow(hexdec,bnum);		   
		p++;
		if(*p=='\0')break;//All the digits have been read.	
	}
	return 0;//Converted successfully
}
//Get the command name
//*str: the source string
//*cmdname: the command name
//*nlen: the length of the command name		
//maxlen: the limit, since a command cannot be very long	
//Return: 0 = success; other = failure.	  
u8 usmart_get_cmdname(u8*str,u8*cmdname,u8 *nlen,u8 maxlen)
{
	*nlen=0;
 	while(*str!=' '&&*str!='\0') //A space or a terminator ends it
	{
		*cmdname=*str;
		str++;
		cmdname++;
		(*nlen)++;//Count the command length
		if(*nlen>=maxlen)return 1;//Bad command
	}
	*cmdname='\0';//Append the terminator
	return 0;//Normal return
}
//Get the next character; runs of spaces are skipped and the first character after them is returned
//str: pointer to the string	
//Return: the next character
u8 usmart_search_nextc(u8* str)
{		   	 	
	str++;
	while(*str==' '&&str!='\0')str++;
	return *str;
} 
//Extract the function name from str
//*str: pointer to the source string
//*fname: pointer to the function name found
//*pnum: the function's argument count
//*rval: whether the return value should be shown (0 = no; 1 = yes)
//Return: 0 = success; other = error code.
u8 usmart_get_fname(u8*str,u8*fname,u8 *pnum,u8 *rval)
{
	u8 res;
	u8 fover=0;	  //Bracket depth
	u8 *strtemp;
	u8 offset=0;  
	u8 parmnum=0;
	u8 temp=1;
	u8 fpname[6];//void+X+'/0'
	u8 fplcnt=0; //Length counter for the first argument
	u8 pcnt=0;	 //Argument counter
	u8 nchar;
	//Work out whether the function returns anything
	strtemp=str;
	while(*strtemp!='\0')//Not finished
	{
		if(*strtemp!=' '&&(pcnt&0X7F)<5)//Record at most 5 characters
		{	
			if(pcnt==0)pcnt|=0X80;//Set the top bit to mark the start of the return type
			if(((pcnt&0x7f)==4)&&(*strtemp!='*'))break;//The last character must be *
			fpname[pcnt&0x7f]=*strtemp;//Record the function's return type
			pcnt++;
		}else if(pcnt==0X85)break;
		strtemp++; 
	} 
	if(pcnt)//Finished receiving
	{
		fpname[pcnt&0x7f]='\0';//Append the terminator
		if(usmart_strcmp(fpname,"void")==0)*rval=0;//No return value wanted
		else *rval=1;							   //A return value is wanted
		pcnt=0;
	} 
	res=0;
	strtemp=str;
	while(*strtemp!='('&&*strtemp!='\0') //This finds where the function name really starts
	{  
		strtemp++;
		res++;
		if(*strtemp==' '||*strtemp=='*')
		{
			nchar=usmart_search_nextc(strtemp);		//Get the next character
			if(nchar!='('&&nchar!='*')offset=res;	//Skip spaces and asterisks
		}
	}	 
	strtemp=str;
	if(offset)strtemp+=offset+1;//Jump to the start of the function name	   
	res=0;
	nchar=0;//Whether we are inside a string: 0 = no; 1 = yes;
	while(1)
	{
		if(*strtemp==0)
		{
			res=USMART_FUNCERR;//Function error
			break;
		}else if(*strtemp=='('&&nchar==0)fover++;//Bracket depth goes up one	 
		else if(*strtemp==')'&&nchar==0)
		{
			if(fover)fover--;
			else res=USMART_FUNCERR;//Ended badly: no '(' was seen
			if(fover==0)break;//Reached the end, so stop	    
		}else if(*strtemp=='"')nchar=!nchar;

		if(fover==0)//The function name is not complete yet
		{
			if(*strtemp!=' ')//A space is not part of the name
			{
				*fname=*strtemp;//Got the function name
				fname++;
			}
		}else //The function name is complete.
		{
			if(*strtemp==',')
			{
				temp=1;		//Allow one more argument
				pcnt++;	
			}else if(*strtemp!=' '&&*strtemp!='(')
			{
				if(pcnt==0&&fplcnt<5)		//On the first argument a check is needed so that a void parameter is not counted.
				{
					fpname[fplcnt]=*strtemp;//Record what kind of argument it is.
					fplcnt++;
				}
				temp++;	//Got a real argument, not a space
			}
			if(fover==1&&temp==2)
			{
				temp++;		//Stops it being counted twice
				parmnum++; 	//One more argument
			}
		}
		strtemp++; 			
	}   
	if(parmnum==1)//Only one argument.
	{
		fpname[fplcnt]='\0';//Append the terminator
		if(usmart_strcmp(fpname,"void")==0)parmnum=0;//The argument is void, so there are none.
	}
	*pnum=parmnum;	//Record the argument count
	*fname='\0';	//Append the terminator
	return res;		//Return the result
}


//Pull one function argument out of str
//*str: pointer to the source string
//*fparm: pointer to the argument string
//*ptype: the argument type; 0 = number, 1 = string, 0xFF = bad argument
//Return: 0 = no more arguments; other = offset of the next one.
u8 usmart_get_aparm(u8 *str,u8 *fparm,u8 *ptype)
{
	u8 i=0;
	u8 enout=0;
	u8 type=0;//A number by default
	u8 string=0; //Marks whether a string is being read
	while(1)
	{		    
		if(*str==','&& string==0)enout=1;			//Hold off returning so the start of the next argument can be found
		if((*str==')'||*str=='\0')&&string==0)break;//Return-immediately flag
		if(type==0)//A number by default
		{
			if((*str>='0' && *str<='9')||(*str>='a' && *str<='f')||(*str>='A' && *str<='F')||*str=='X'||*str=='x')//Number string check
			{
				if(enout)break;					//Found the next parameter, so return straight away.
				if(*str>='a')*fparm=*str-0X20;	//Convert lower case to upper case
				else *fparm=*str;		   		//Lower case and digits are left alone
				fparm++;
			}else if(*str=='"')//Found the start of a string
			{
				if(enout)break;//A comma followed by a quote means the end.
				type=1;
				string=1;//Record that a STRING is being read
			}else if(*str!=' '&&*str!=',')//An invalid character was found, so the argument is bad
			{
				type=0XFF;
				break;
			}
		}else//String type
		{ 
			if(*str=='"')string=0;
			if(enout)break;			//Found the next parameter, so return straight away.
			if(string)				//A string is being read
			{	
				if(*str=='\\')		//Hit an escape character (which is not copied)
				{ 
					str++;			//Move to the character after the escape and copy it whatever it is
					i++;
				}					
				*fparm=*str;		//Lower case and digits are left alone
				fparm++;
			}	
		}
		i++;//Advance the offset
		str++;
	}
	*fparm='\0';	//Append the terminator
	*ptype=type;	//Return the argument type
	return i;		//Return the argument length
}
//Get the start of a given argument
//num: which argument, 0~9.
//Return: the start of that argument
u8 usmart_get_parmpos(u8 num)
{
	u8 temp=0;
	u8 i;
	for(i=0;i<num;i++)temp+=usmart_dev.plentbl[i];
	return temp;
}
//Pull the function arguments out of str
//str: the source string;
//parn: how many arguments; 0 means none, i.e. void
//Return: 0 = success; other = error code.
u8 usmart_get_fparam(u8*str,u8 *parn)
{	
	u8 i,type;  
	u32 res;
	u8 n=0;
	u8 len;
	u8 tstr[PARM_LEN+1];//A byte buffer holding a string of at most PARM_LEN characters
	for(i=0;i<MAX_PARM;i++)usmart_dev.plentbl[i]=0;//Clear the argument length table
	while(*str!='(')//Move to where the arguments start
	{
		str++;											    
		if(*str=='\0')return USMART_FUNCERR;//Hit the terminator
	}
	str++;//Move to the first byte after the "("
	while(1)
	{
		i=usmart_get_aparm(str,tstr,&type);	//Get the first argument  
		str+=i;								//Offset
		switch(type)
		{
			case 0:	//Number
				if(tstr[0]!='\0')				//The argument received is valid
				{					    
					i=usmart_str2num(tstr,&res);	//Record this parameter	 
					if(i)return USMART_PARMERR;		//Parameter error.
					*(u32*)(usmart_dev.parm+usmart_get_parmpos(n))=res;//Record the converted value.
					usmart_dev.parmtype&=~(1<<n);	//Mark it as a number
					usmart_dev.plentbl[n]=4;		//This argument is 4 bytes long  
					n++;							//One more argument  
					if(n>MAX_PARM)return USMART_PARMOVER;//Too many parameters
				}
				break;
			case 1://String	 	
				len=usmart_strlen(tstr)+1;	//Includes the '\0' terminator
				usmart_strcopy(tstr,&usmart_dev.parm[usmart_get_parmpos(n)]);//Copy tstr into usmart_dev.parm[n]
				usmart_dev.parmtype|=1<<n;	//Mark it as a string 
				usmart_dev.plentbl[n]=len;	//This argument is len bytes long  
				n++;
				if(n>MAX_PARM)return USMART_PARMOVER;//Too many parameters
				break;
			case 0XFF://Error
				return USMART_PARMERR;//Parameter error	  
		}
		if(*str==')'||*str=='\0')break;//Found the end marker.
	}
	*parn=n;	//Record the argument count
	return USMART_OK;//The arguments were read correctly
}














