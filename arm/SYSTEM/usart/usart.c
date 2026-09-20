
#include "usart.h"

#include "main.h"

//////////////////////////////////////////////////////////////////////////////////
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
//V1.5 change log
////////////////////////////////////////////////////////////////////////////////// 	  
 

//////////////////////////////////////////////////////////////////
//Added the code below so printf works without having to select MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//Support functions the standard library needs                 
struct __FILE 
{ 
	int handle; 
}; 
FILE __stdout;
       
//Define _sys_exit() to avoid semihosting    
void _sys_exit(int x) 
{ 
	x = x; 
}

//Redefine fputc 
int fputc(int ch, FILE *f)
{      
//	while((USART1->SR & 0X40) == 0);//loop until the byte has gone out   
//	USART1->DR = (u8)ch;
//	TXEN = SEND;
	while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
	USART_SendData(USART1, ch);
//	TXEN = RECEIVE;
	return ch;
}
#endif 

/*The MicroLIB way of doing it*/
 /* 
int fputc(int ch, FILE *f)
{
	USART_SendData(USART1, (uint8_t) ch);

	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {}	
   
    return ch;
}

int GetKey (void)  { 

    while (!(USART1->SR & USART_FLAG_RXNE));

    return ((int)(USART1->DR & 0x1FF));
}
*/
 
#if EN_USART1_RX   //If receiving is enabled
//USART1 interrupt service routine
//Note that reading USARTx->SR avoids some baffling errors   	



//Receive state
//bit15,	receive complete flag
//bit14,	0x0d has been received
//bit13~0,	number of valid bytes received
u16 USART_RX_STA = 0;       //Receive state flags	  

//Initialise the IO and USART1 
//bound: the baud rate
// SUB
void uart1_init(u32 bound)
{
    //GPIO port configuration
  GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);	//Enable the USART1 and GPIOA clocks
 	USART_DeInit(USART1);  //Reset USART1
	//USART1_TX   PA.9
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;				//PA.9
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;			//Alternate-function push-pull output
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//Initialise PA9
 
	//USART1_RX	  PA.10
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;				//PA.10
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	//Floating input
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//Initialise PA10

#if (ARM_MINI || ARM_CM5)
	//RS485_TXEN	PC.2
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;				//PA.2
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		//Standard push-pull output
	GPIO_Init(GPIOC, &GPIO_InitStructure);					//Initialise PA2
//	GPIO_SetBits(GPIOC, GPIO_Pin_2);
#endif

#if ARM_TSTAT_WIFI
	//RS485_TXEN	PA.8
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;				//PA.8
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		//Standard push-pull output
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//Initialise PA2
	//GPIO_SetBits(GPIOA, GPIO_Pin_8);
#endif
// FOR TINY
	//RS485_TXEN	PD.8
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;				//PD.8
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		//Standard push-pull output
	GPIO_Init(GPIOD, &GPIO_InitStructure);					//Initialise PD8
//	GPIO_SetBits(GPIOC, GPIO_Pin_2);
	
	//USART1 NVIC configuration
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//Pre-emption priority 3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;			//Sub-priority 3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//Enable the IRQ channel
	NVIC_Init(&NVIC_InitStructure);								//Initialise the VIC registers with the given parameters
  
	//USART initialisation settings
	USART_InitStructure.USART_BaudRate = bound;					//Baud rate setting
//	USART_InitStructure.USART_StopBits = USART_StopBits_1;		//one stop bit
	
	
	if(Modbus.uart_parity[0] == 2)
	{
		USART_InitStructure.USART_Parity = USART_Parity_Even;
		USART_InitStructure.USART_WordLength = USART_WordLength_9b;	//9-bit word length
	}
	else if(Modbus.uart_parity[0] == 1)
	{
		USART_InitStructure.USART_Parity = USART_Parity_Odd;
		USART_InitStructure.USART_WordLength = USART_WordLength_9b;	//9-bit word length
	}
	else
	{
		USART_InitStructure.USART_Parity = USART_Parity_No;			//No parity
		USART_InitStructure.USART_WordLength = USART_WordLength_8b;	//8-bit word length	
	}
	// stop bit
//	USART_StopBits_1        0             
// 	USART_StopBits_0_5      1          
// 	USART_StopBits_2        2           
// 	USART_StopBits_1_5  		3
	if(Modbus.uart_stopbit[0] == 1)
	{
		USART_InitStructure.USART_StopBits = USART_StopBits_0_5;		
	}
	else if(Modbus.uart_stopbit[0] == 2)
	{
		USART_InitStructure.USART_StopBits = USART_StopBits_2;		
	}
	else if(Modbus.uart_stopbit[0] == 3)
	{
		USART_InitStructure.USART_StopBits = USART_StopBits_1_5;		
	}
	else
	{
		USART_InitStructure.USART_StopBits = USART_StopBits_1;		
	}
//	
//	if((Modbus.uart_WordLen[0] == 8) && (Modbus.uart_parity[0] != 0))
//	{ 
//		USART_InitStructure.USART_WordLength = USART_WordLength_9b;	//9-bit word length
//	}
//	else 
//	{		// default 8 bit
//		USART_InitStructure.USART_WordLength = USART_WordLength_8b;	//8-bit word length
//	}	
	
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	//No hardware flow control
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;					//Transmit and receive mode
	USART_Init(USART1, &USART_InitStructure); 					//Initialise the serial port

	USART_ITConfig(USART1, USART_IT_RXNE/*|USART_IT_TC*/, ENABLE);				//Enable the interrupt
	USART_Cmd(USART1, ENABLE);                    				
}

// MAIN
void uart3_init(u32 bound)
{
    //GPIO port configuration
  GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE); 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOE, ENABLE);	//Enable the USART3 and GPIOA clocks
 	USART_DeInit(USART3);  //Reset USART1
	//USART3_TX   PB.10
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;				//PB.10
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;			//Alternate-function push-pull output
	GPIO_Init(GPIOB, &GPIO_InitStructure);					//Initialise PB10
 
	//USART3_RX	  PB.11
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;				//PB.11
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	//Floating input
	GPIO_Init(GPIOB, &GPIO_InitStructure);					//Initialise PB11
	
	//RS485_TXEN	PE.11
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;				//PE.11
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		//Standard push-pull output
	GPIO_Init(GPIOE, &GPIO_InitStructure);				
//	GPIO_SetBits(GPIOE, GPIO_Pin_11);
	
	//USART3 NVIC configuration
  NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//Pre-emption priority 3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;			//Sub-priority 3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//Enable the IRQ channel
	NVIC_Init(&NVIC_InitStructure);								//Initialise the VIC registers with the given parameters
  
	//USART initialisation settings
	USART_InitStructure.USART_BaudRate = bound;					//Baud rate setting
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;	//8-bit word length
//	USART_InitStructure.USART_StopBits = USART_StopBits_1;		//one stop bit

	if(Modbus.uart_parity[2] == 2)
	{
		USART_InitStructure.USART_Parity = USART_Parity_Even;
		USART_InitStructure.USART_WordLength = USART_WordLength_9b;	//9-bit word length
	}
	else if(Modbus.uart_parity[2] == 1)
	{
		USART_InitStructure.USART_Parity = USART_Parity_Odd;
		USART_InitStructure.USART_WordLength = USART_WordLength_9b;	//9-bit word length
	}
	else
	{
		USART_InitStructure.USART_Parity = USART_Parity_No;			//No parity bit	
		USART_InitStructure.USART_WordLength = USART_WordLength_8b;	//9-bit word length
	}	

	// stop bit
//	USART_StopBits_1        0             
// 	USART_StopBits_0_5      1          
// 	USART_StopBits_2        2           
// 	USART_StopBits_1_5  		3
	if(Modbus.uart_stopbit[2] == 1)
	{
		USART_InitStructure.USART_StopBits = USART_StopBits_0_5;		
	}
	else if(Modbus.uart_stopbit[2] == 2)
	{
		USART_InitStructure.USART_StopBits = USART_StopBits_2;		
	}
	else if(Modbus.uart_stopbit[2] == 3)
	{
		USART_InitStructure.USART_StopBits = USART_StopBits_1_5;		
	}
	else
	{
		USART_InitStructure.USART_StopBits = USART_StopBits_1;		
	}
	
//	if((Modbus.uart_WordLen[2] == 8) && (Modbus.uart_parity[2] != 0))
//	{
//		USART_InitStructure.USART_WordLength = USART_WordLength_9b;	//9-bit word length
//	}
//	else 
//	{
//		USART_InitStructure.USART_WordLength = USART_WordLength_8b;	//8-bit word length
//	}
	
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	//No hardware flow control
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;					//Transmit and receive mode
	USART_Init(USART3, &USART_InitStructure); 					//Initialise the serial port

	USART_ITConfig(USART3, USART_IT_RXNE/*|USART_IT_TC*/, ENABLE);				//Enable the interrupt
	USART_Cmd(USART3, ENABLE);                    				
}

// ZIGBEE
void uart2_init(u32 bound)
{
    //GPIO port configuration
  GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	 
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE); 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA , ENABLE);	//Enable the USART2 and GPIOA clocks
 	USART_DeInit(USART2);  //Reset USART2
	//USART1_TX   PA.2
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;				//PA.2
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;			//Alternate-function push-pull output
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//Initialise PA9
 
	//USART1_RX	  PA.3
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;				//PA.3
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	//Floating input
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//Initialise PA10

// SELECT RS232 or ZIGBEE by PC3	
	if((Modbus.mini_type == MINI_BIG) || (Modbus.mini_type == MINI_BIG_ARM))
	{
		// if T3_BB
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;				//PE.11
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		//Standard push-pull output
	GPIO_Init(GPIOC, &GPIO_InitStructure);		
	}
	
	//USART2 NVIC configuration
  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//Pre-emption priority 3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;			//Sub-priority 3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//Enable the IRQ channel
	NVIC_Init(&NVIC_InitStructure);								//Initialise the VIC registers with the given parameters
  
	//USART initialisation settings
	USART_InitStructure.USART_BaudRate = bound;					//Baud rate setting
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;	//8-bit word length
	USART_InitStructure.USART_StopBits = USART_StopBits_1;		//One stop bit
	USART_InitStructure.USART_Parity = USART_Parity_No;			//No parity bit
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	//No hardware flow control
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;					//Transmit and receive mode
	USART_Init(USART2, &USART_InitStructure); 					//Initialise the serial port

	USART_ITConfig(USART2, USART_IT_RXNE/*|USART_IT_TC*/, ENABLE);				//Enable the interrupt
	USART_Cmd(USART2, ENABLE);                    				
}



#endif
