#include "delay.h"
#include "usart.h"
#include "rtc.h" 		    
#include "datetime.h"

extern BACNET_DATE Local_Date;
extern BACNET_TIME Local_Time;
//			vu8 sec;				/* 0-59	*/
//			vu8 min;    		/* 0-59	*/
//			vu8 hour;      		/* 0-23	*/
//			vu8 day;       		/* 1-31	*/
//			vu8 week;  		/* 0-6, 0=Sunday	*/
//			vu8 mon;     		/* 0-11	*/
//			vu16 year;      		/* 0-99	*/
//			vu16 day_of_year; 	/* 0-365	*/
//			vu8 is_dst;        /* daylight saving time on / off */

extern U16_T Test[50];
UN_Time Rtc;//Clock structure 
/*
void set_clock(u16 divx)
{
 	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);	//enable the PWR and BKP peripheral clocks  
	PWR_BackupAccessCmd(ENABLE);	//allow access to the RTC and backup registers 

	RTC_EnterConfigMode();/// allow configuration	
 
	RTC_SetPrescaler(divx); //set the RTC prescaler          									 
	RTC_ExitConfigMode();//leave configuration mode  				   		 									  
	RTC_WaitForLastTask();	//wait for the last write to the RTC registers to finish		 									  
}	   
*/

static void RTC_NVIC_Config(void)
{	
//	NVIC_InitTypeDef NVIC_InitStructure;
//	NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;				//RTC global interrupt
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//1 bit of pre-emption priority, 3 bits of sub-priority
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;			//0 bits of pre-emption priority, 4 bits of sub-priority
//	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//enable this interrupt channel
//	NVIC_Init(&NVIC_InitStructure);								//initialise the NVIC registers from NVIC_InitStruct
}

//Real time clock configuration
//Initialise the RTC and check that it is running properly
//BKP->DR1 records whether this is the first time it has been configured
//Returns 0: ok
//Other: error code
//void watchdog(void);
//void RTC_Check_Initial(void)
//{
//	uint8 rtc_state = 0;
//	uint8 i;
//	
//	rtc_state = 1;
//	while(rtc_state)
//	{ 
//		watchdog();
//		if(i < 20)
//		{
//			if(RTC_Init() == 1) //initial OK
//			{
//				rtc_state = 0;
//				break;
//			}
//			else
//				i++;
//		}
//		else
//		{
//			rtc_state = 0;
//			break;
//		}
//		delay_ms(100);	
//	}	
//	
//}

u8 RTC_Init(void)
{
	//Check whether the clock is being configured for the first time
	u8 temp = 0;	
	if(BKP_ReadBackupRegister(BKP_DR1) != 0x5050)	//Read the backup register: what was read back does not match what was written
	{	 
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);	//Enable the PWR and BKP peripheral clocks   
		PWR_BackupAccessCmd(ENABLE);												//Allow access to the backup registers 
		BKP_DeInit();				//Reset the backup domain 	
		RCC_LSEConfig(RCC_LSE_ON);	//Select the external low speed oscillator (LSE)
		while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)	//Check the RCC flag and wait for the low speed oscillator to be ready
		{
			temp++;
			delay_ms(10);
			if(temp >= 250)
				return 0;	//Clock initialisation failed; the crystal has a problem
		}
		
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);	//Set the RTC clock (RTCCLK), choosing LSE as its source    
		RCC_RTCCLKCmd(ENABLE);					//Enable the RTC clock  
		RTC_WaitForLastTask();					//Wait for the last write to the RTC registers to finish
		RTC_WaitForSynchro();					//Wait for the RTC registers to synchronise  
		RTC_ITConfig(RTC_IT_SEC, ENABLE);		//Enable the RTC second interrupt
		RTC_WaitForLastTask();					//Wait for the last write to the RTC registers to finish
		RTC_EnterConfigMode();					//Allow configuration	
		RTC_SetPrescaler(32767);				//Set the RTC prescaler
		RTC_WaitForLastTask();					//Wait for the last write to the RTC registers to finish
		Rtc_Set(16, 8, 27, 15, 42, 55,0);		//Set the time	
		RTC_ExitConfigMode(); 
		//Leave configuration mode  
		BKP_WriteBackupRegister(BKP_DR1, 0X5050);//Write user data into the backup register
	}
	else//The system carries on keeping time
	{
		RTC_WaitForSynchro();					//Wait for the last write to the RTC registers to finish
		RTC_ITConfig(RTC_IT_SEC, ENABLE);		//Enable the RTC second interrupt
		RTC_WaitForLastTask();					//Wait for the last write to the RTC registers to finish
	}
	
	RTC_NVIC_Config();							//RTC interrupt grouping		    				     
	RTC_Get();									//Update the time	
	return 1;
}

//RTC clock interrupt
//Fires once a second  
//extern u16 tcnt; 
//void RTC_IRQHandler(void)
//{		 
//	if(RTC_GetITStatus(RTC_IT_SEC) != RESET)	//second interrupt
//	{							
//		RTC_Get();//update the time   
// 	}
//	
//	if(RTC_GetITStatus(RTC_IT_ALR)!= RESET)		//alarm interrupt
//	{
//		RTC_ClearITPendingBit(RTC_IT_ALR);		//clear the alarm interrupt	  	   
//  	}
//	
//	RTC_ClearITPendingBit(RTC_IT_SEC|RTC_IT_OW);//clear the alarm interrupt
//	RTC_WaitForLastTask();	  	    						 	   	 
//}

//Leap year test
//Month  1  2  3  4  5  6  7  8  9  10 11 12
//Leap   31 29 31 30 31 30 31 31 30 31 30 31
//Common 31 28 31 30 31 30 31 31 30 31 30 31
//Input: the year
//Output: whether it is a leap year. 1 = yes, 0 = no
u8 Is_Leap_Year(u16 year)
{			  
	if(year % 4 == 0)				//Must be divisible by 4
	{ 
		if(year % 100 == 0) 
		{ 
			if(year % 400 == 0)		//If it ends in 00 it must also be divisible by 400
				return 1; 	   
			else
				return 0;   
		}
		else
		{
			return 1;   
		}
	}
	else 
	{
		return 0;
	}		
}

//Set the clock
//Convert the given time into seconds
//Measured from 1 January 1970
//Years 1970~2099 are valid
//Return: 0 = success; other = error code.
//Month data table											 
u8 const table_week[12] = {0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5};	//Month correction table	  
//Days per month in a common year

const u8 mon_table[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
u32 Rtc_Set(u16 syear, u8 smon, u8 sday, u8 hour, u8 min, u8 sec, u8 flag)
{
	u16 t;
	u32 seccount = 0;
	if(2000 + syear < 1970 || 2000 + syear > 2099)	
		return 1;
	
	for(t = 1970; t < 2000 + syear; t++)				//Add up the seconds for all the whole years
	{
		if(Is_Leap_Year(t))
			seccount += 31622400;				//Number of seconds in a leap year
		else 
			seccount += 31536000;				//Seconds in a common year
	}
	
	smon -= 1;
	for(t = 0; t < smon; t++)					//Add up the seconds for the preceding months
	{
		seccount += (u32)mon_table[t] * 86400;	//Add the seconds for the months
		if(Is_Leap_Year(2000 + syear) && t == 1)
			seccount += 86400;					//In a leap year February gains a day's worth of seconds	   
	}
	seccount += (u32)(sday - 1) * 86400;		//Add up the seconds for the preceding days 
	seccount += (u32)hour * 3600;					//Seconds from the hours
  seccount += (u32)min * 60;					//Seconds from the minutes
	seccount += sec;							//Finally add the seconds

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);//Enable the PWR and BKP peripheral clocks  
	PWR_BackupAccessCmd(ENABLE);	//Allow access to the RTC and backup registers 
	if(flag == 0) {	RTC_SetCounter(seccount);	}	//Set the RTC counter
	
	RTC_WaitForLastTask();			//Wait for the last write to the RTC registers to finish  	
	return seccount;	    
}

/*flag : whether update local_date and local_time*/
/*flag == 0 used bacnet trendlog*/
void Get_Time_by_sec(u32 sec_time,UN_Time * rtc, uint8_t flag)
{
	static u16 daycnt = 0;
	u32 temp = 0;
	u16 temp1 = 0;
	
	
 	temp = sec_time / 86400;		//Work out the number of days from the seconds
	if(flag == 0)
	{
		daycnt = 0;
	}
	if(daycnt != temp)				//More than a day
	{	  
		daycnt = temp;
		temp1 = 1970;				//Starting from 1970
		while(temp >= 365)
		{				 
			if(Is_Leap_Year(temp1))	//Leap year
			{
				if(temp >= 366)
				{
					temp -= 366;	//Number of seconds in a leap year
				}
				else 
				{//??????????????????????
					// the last day of a leap year comes out wrong
					//temp1++;
					break;
				}  
			}
			else
			{
				temp -= 365;		//Common year
			}			
			temp1++;  
		}   
		rtc->Clk.year = temp1 - 2000;	//Work out the year
		rtc->Clk.day_of_year = temp + 1;  // get day of year, added by chelsea
		temp1 = 0;
		while(temp >= 28)			//More than a month
		{
			if(Is_Leap_Year(rtc->Clk.year) && temp1 == 1)	//Whether this year is a leap year, and February
			{
				if(temp >= 29)
					temp -=	29;		//Number of seconds in a leap year
				else
					break; 
			}
			else 
			{
				if(temp >= mon_table[temp1])
					temp -= mon_table[temp1];	//Common year
				else
					break;
			}
			temp1++;  
		}
		rtc->Clk.mon = temp1 + 1;	//Work out the month
		rtc->Clk.day = temp + 1;  	//Work out the day 
	}
	temp = sec_time % 86400;     		//Work out the seconds   	   
	rtc->Clk.hour = temp / 3600;     	//Hours
	rtc->Clk.min = (temp % 3600) / 60; 	//Minutes	
	rtc->Clk.sec = (temp % 3600) % 60; 	//Seconds
	rtc->Clk.week = RTC_Get_Week(2000 + rtc->Clk.year, rtc->Clk.mon,rtc->Clk.day);	//Get the weekday   
	
	if(flag == 1)
	{
	Local_Date.year = rtc->Clk.year + 2000;
	Local_Date.month = rtc->Clk.mon;
	Local_Date.day = rtc->Clk.day;
	Local_Date.wday = rtc->Clk.week;
	
	Local_Time.hour = rtc->Clk.hour;
	Local_Time.min = rtc->Clk.min;
	Local_Time.sec = rtc->Clk.sec;
	}
}



//Get the current time
//Return: 0 = success; other = error code.
u8 RTC_Get(void)
{
  Get_Time_by_sec(RTC_GetCounter(),&Rtc,1);
	return 0;
}

//Work out what day of the week it is
//Description: given a Gregorian date, return the weekday (1901-2099 only)
//Input: Gregorian year, month and day 
//Return: the weekday number																						 
u8 RTC_Get_Week(u16 year, u8 month, u8 day)
{	
	u16 temp2;
	u8 yearH, yearL;
	
	yearH = year / 100;
	yearL = year % 100;
	  
	if(yearH > 19)	// for the 21st century, add 100 to the year
		yearL += 100;
	
	// only leap years after 1900 are counted  
	temp2 = yearL + yearL / 4;
	temp2 = temp2 % 7; 
	temp2 = temp2 + day + table_week[month - 1];
	
	if(yearL % 4 == 0 && month < 3)
		temp2--;
	
	return(temp2 % 7);
}			  
