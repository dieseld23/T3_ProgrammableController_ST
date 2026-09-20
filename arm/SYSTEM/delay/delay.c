#include "delay.h"

#ifdef SYSTEM_SUPPORT_OS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#endif

static u8  fac_us = 0;	//Microsecond delay multiplier			   
static u16 fac_ms = 0;	//Millisecond delay multiplier; under an OS it is the number of ms per tick
   
//Initialise the delay functions
//When uC/OS is used, this also sets up the uC/OS tick
//The SysTick clock is fixed at HCLK/8
//SYSCLK: the system clock
void delay_init(u8 SYSCLK)
{
#ifdef SYSTEM_SUPPORT_OS
	u32 reload;
#endif
 	SysTick->CTRL &= ~(1 << 2);				//SysTick uses the external clock source	 
	fac_us = SYSCLK / 8;					//fac_us is needed whether or not uC/OS is used
	    
#ifdef SYSTEM_SUPPORT_OS
	reload = SYSCLK / 8;					//Counts per second, in thousands	   
	reload *= 1000000 / configTICK_RATE_HZ;	//Set the overflow period from configTICK_RATE_HZ
											//reload is a 24-bit register with a maximum of 16777216, which is about 1.86s at 72M	
	fac_ms = 1000 / configTICK_RATE_HZ;		//The smallest delay uC/OS can provide	   
// 	SysTick->CTRL |= 1 << 1;				//enable the SysTick interrupt
	SysTick->LOAD = reload; 				//Interrupts once every 1/configTICK_RATE_HZ seconds	
	SysTick->CTRL |= 1 << 0;				//Start SysTick    
#else
	fac_ms = (u16)fac_us * 1000;			//Without an OS this is the number of SysTick clocks per millisecond   
#endif
}								    

#ifdef SYSTEM_SUPPORT_OS
//Delay for nus microseconds
//nus is the number of microseconds to delay.		    								   
void delay_us(u32 nus)
{		
	u32 ticks;
	u32 told, tnow, tcnt = 0;
	u32 reload = SysTick->LOAD;				//The LOAD value	    	 
	ticks = nus * fac_us; 					//The number of ticks needed	  		 
	tcnt = 0;

// 	vTaskSuspendAll();						//stop OS scheduling so the microsecond delay is not interrupted
	taskENTER_CRITICAL();
	told = SysTick->VAL;        			//The counter value on entry
	while(1)
	{
		tnow = SysTick->VAL;	
		if(tnow != told)
		{	    
			if(tnow < told)
				tcnt += told - tnow;		//Just remember that SysTick counts down.
			else
				tcnt += reload - tnow + told;	    

			told = tnow;
			if(tcnt >= ticks)				//Once the elapsed time reaches the requested delay, return.
				break;
		}  
	};
// 	xTaskResumeAll();						//resume OS scheduling
	taskEXIT_CRITICAL(); 									    
}

//Delay for nms milliseconds
//nms: the number of milliseconds to delay
void delay_ms(u16 nms)
{	
	if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)	//If the OS is already running
	{		  
		if(nms >= fac_ms)					//The delay is longer than the smallest uC/OS period 
		{
   			vTaskDelay(nms / fac_ms);		//uC/OS delay
		}
		nms %= fac_ms;						//uC/OS cannot provide a delay this short, so fall back to the plain one    
	}
	delay_us((u32)(nms*1000));				//Plain delay 
}

#else	//When no OS is used
//Delay for nus microseconds
//nus is the number of microseconds to delay.		    								   
void delay_us(u32 nus)
{		
	u32 temp;	    	 
	SysTick->LOAD = nus * fac_us;	//Load the time	  		 
	SysTick->VAL = 0x00;			//Clear the counter
	SysTick->CTRL = 0x01;			//Start counting down 	 
	do
	{
		temp = SysTick->CTRL;
	}
	while((temp & 0x01) && !(temp & (1 << 16)));	//Wait until the time expires 
	  
	SysTick->CTRL = 0x00;			//Stop the counter
	SysTick->VAL = 0X00;			//Clear the counter	 
}

//Delay for nms milliseconds
//Mind the range of nms
//SysTick->LOAD is a 24-bit register, so the longest delay is:
//nms<=0xffffff*8*1000/SYSCLK
//SYSCLK is in Hz and nms is in ms
//At 72M that means nms<=1864 
void delay_ms(u16 nms)
{	 		  	  
	u32 temp;		   
	SysTick->LOAD = (u32)nms * fac_ms;	//Load the time (SysTick->LOAD is 24-bit)
	SysTick->VAL = 0x00;				//Clear the counter
	SysTick->CTRL = 0x01;				//Start counting down  
	do
	{
		temp = SysTick->CTRL;
	}
	while((temp & 0x01) && !(temp & (1 << 16)));	//Wait until the time expires   

	SysTick->CTRL = 0x00;				//Stop the counter
	SysTick->VAL = 0X00;				//Clear the counter	  	    
} 
#endif
