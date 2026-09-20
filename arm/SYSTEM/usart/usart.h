#ifndef __USART_H
#define __USART_H

#include "stdio.h"
#include "stm32f10x.h"

//V1.3 change log 
//Baud rate setting now adapts to different clock frequencies.
//Added printf support
//Added serial command reception.
//Fixed the bug where printf lost its first character
//V1.4 change log
//1, Fixed a bug in the serial port IO initialisation
//2, Changed USART_RX_STA so the maximum receive length is 2^14 bytes
//3, Added USART_REC_LEN, which sets the maximum number of bytes the serial port will accept (no more than 2^14)
//4, Changed how EN_USART1_RX is enabled
////////////////////////////////////////////////////////////////////////////////// 	


	  	


void uart1_init(u32 bound);
void uart2_init(u32 bound);
void uart3_init(u32 bound);
#define EN_USART1_RX 			1		//使能（1）/禁止（0）串口1接收


#endif


