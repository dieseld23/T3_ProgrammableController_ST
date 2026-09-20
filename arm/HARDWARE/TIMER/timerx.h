#ifndef __TIMERX_H
#define __TIMERX_H

#include "stm32f10x.h"


//V1.1 20120904
//1, Added the TIM3_PWM_Init function.
//2, Added the LED0_PWM_VAL macro, which controls the TIM3_CH2 pulse width									  
//////////////////////////////////////////////////////////////////////////////////  


//Changing TIM3->CCR2 changes the duty cycle and so the brightness of LED0
#define LED0_PWM_VAL	TIM3->CCR2    
#define SWTIMER_INTERVAL	1
void TIM3_Int_Init(u16 arr, u16 psc);
void TIM3_PWM_Init(u16 arr, u16 psc);

extern u32 uip_timer;
void TIM6_Int_Init(u16 arr, u16 psc);

#endif
