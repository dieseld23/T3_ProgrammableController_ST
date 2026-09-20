#include "bsp_esp8266.h"
//#include "common.h"
#include <stdio.h>  
#include <string.h>  
#include <stdbool.h>
//#include "bsp_SysTick.h"
#include "main.h"
#include "wifi.h"




static void                   ESP8266_GPIO_Config                 ( void );
void                   ESP8266_USART_Config                ( void );
static void                   ESP8266_USART_NVIC_Configuration    ( void );



struct  STRUCT_USARTx_Fram strEsp8266_Fram_Record = { 0 };

//#include "common.h"
#include "stm32f10x.h"
#include <stdarg.h>



char *                 itoa                                ( int value, char * string, int radix );

/*
 * Function   : USART2_printf
 * Description: formatted output, like printf in the C library, but without using the C library
 * Input      : -USARTx serial channel; only serial port 2, USART2, is used here
 *		     -Data   pointer to the content to send to the serial port
 *			   -...    further arguments
 * Output     : none
 * Return     : none 
 * Called by  : external code
 *         typical use USART2_printf( USART2, "\r\n this is a demo \r\n" );
 *            		 USART2_printf( USART2, "\r\n %d \r\n", i );
 *            		 USART2_printf( USART2, "\r\n %s \r\n", j );
 */
void USART_printf ( USART_TypeDef * USARTx, char * Data, ... )
{
	const char *s;
	int d;   
	char buf[16];

	
	va_list ap;
	va_start(ap, Data);

	while ( * Data != 0 )     // check whether the end of the string has been reached
	{				                          
		if ( * Data == 0x5c )  //'\'
		{									  
			switch ( *++Data )
			{
				case 'r':							          //Carriage return
				USART_SendData(USARTx, 0x0d);
				Data ++;
				break;

				case 'n':							          //Line feed
				USART_SendData(USARTx, 0x0a);	
				Data ++;
				break;

				default:
				Data ++;
				break;
			}			 
		}
		
		else if ( * Data == '%')
		{									  //
			switch ( *++Data )
			{				
				case 's':										  //String
				s = va_arg(ap, const char *);
				
				for ( ; *s; s++) 
				{
					USART_SendData(USARTx,*s);
					while( USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET );
				}
				
				Data++;
				
				break;

				case 'd':			
					//Decimal
				d = va_arg(ap, int);
				
				itoa(d, buf, 10);
				
				for (s = buf; *s; s++) 
				{
					USART_SendData(USARTx,*s);
					while( USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET );
				}
				
				Data++;
				
				break;
				
				default:
				Data++;
				
				break;
				
			}		 
		}
		
		else USART_SendData(USARTx, *Data++);
		
		while ( USART_GetFlagStatus ( USARTx, USART_FLAG_TXE ) == RESET );
		
	}
}


/*
 * Function   : itoa
 * Description: converts an integer into a string
 * Input      : -radix =10 means base 10; anything else gives 0
 *         -value the integer to convert
 *         -buf the resulting string
 *         -radix = 10
 * Output     : none
 * Return     : none
 * Called by  : USART2_printf()
 */
char * itoa( int value, char *string, int radix )
{
	int     i, d;
	int     flag = 0;
	char    *ptr = string;

	/* This implementation only works for decimal numbers. */
	if (radix != 10)
	{
		*ptr = 0;
		return string;
	}

	if (!value)
	{
		*ptr++ = 0x30;
		*ptr = 0;
		return string;
	}

	/* if this is a negative value insert the minus sign. */
	if (value < 0)
	{
		*ptr++ = '-';

		/* Make the value positive. */
		value *= -1;
		
	}

	for (i = 10000; i > 0; i /= 10)
	{
		d = value / i;

		if (d || flag)
		{
			*ptr++ = (char)(d + 0x30);
			value -= (d * i);
			flag = 1;
		}
	}

	/* Null terminate the string. */
	*ptr = 0;

	return string;

} /* NCL_Itoa */





/**
  * @brief  ESP8266 initialisation
  * @param  none
  * @retval none
  */
void ESP8266_Init ( void )
{
	ESP8266_GPIO_Config (); 
	
	ESP8266_USART_Config (); 
   
	dma_init_uart4();
	
	macESP8266_RST_HIGH_LEVEL();

//	macESP8266_CH_DISABLE();
	
	
}


/**
  * @brief  initialise the GPIO pins used by the ESP8266
  * @param  none
  * @retval none
  */
static void ESP8266_GPIO_Config ( void )
{
	/*Declare a GPIO_InitTypeDef structure*/
	GPIO_InitTypeDef GPIO_InitStructure;


	/* configure the CH_PD pin*/
//	macESP8266_CH_PD_APBxClock_FUN ( macESP8266_CH_PD_CLK, ENABLE ); 
//											   
//	GPIO_InitStructure.GPIO_Pin = macESP8266_CH_PD_PIN;	

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   
   
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 

//	GPIO_Init ( macESP8266_CH_PD_PORT, & GPIO_InitStructure );	 


	
	/* configure the RST pin*/
	macESP8266_RST_APBxClock_FUN ( macESP8266_RST_CLK, ENABLE ); 
											   
	GPIO_InitStructure.GPIO_Pin = macESP8266_RST_PIN;	

	GPIO_Init ( macESP8266_RST_PORT, & GPIO_InitStructure );	 


}


/**
  * @brief  initialise the USART used by the ESP8266
  * @param  none
  * @retval none
  */
void ESP8266_USART_Config ( void )
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	
	
	/* config USART clock */
	macESP8266_USART_APBxClock_FUN ( macESP8266_USART_CLK, ENABLE );
	macESP8266_USART_GPIO_APBxClock_FUN ( macESP8266_USART_GPIO_CLK, ENABLE );
	
	/* USART GPIO config */
	/* Configure USART Tx as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin =  macESP8266_USART_TX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(macESP8266_USART_TX_PORT, &GPIO_InitStructure);  
  
	/* Configure USART Rx as input floating */
	GPIO_InitStructure.GPIO_Pin = macESP8266_USART_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(macESP8266_USART_RX_PORT, &GPIO_InitStructure);
	
	/* USART1 mode config */
	USART_InitStructure.USART_BaudRate = macESP8266_USART_BAUD_RATE;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(macESP8266_USARTx, &USART_InitStructure);
	
	
	/* interrupt configuration */
	USART_ITConfig ( macESP8266_USARTx, USART_IT_RXNE, ENABLE ); //Enable the serial receive interrupt 
	USART_ITConfig ( macESP8266_USARTx, USART_IT_IDLE, ENABLE ); //Enable the serial bus idle interrupt 	

	ESP8266_USART_NVIC_Configuration ();
	
	
	USART_Cmd(macESP8266_USARTx, ENABLE);
	
	
}


/**
  * @brief  configure the NVIC interrupt for the ESP8266 USART
  * @param  none
  * @retval none
  */
static void ESP8266_USART_NVIC_Configuration ( void )
{
	NVIC_InitTypeDef NVIC_InitStructure; 
	
	
	/* Configure the NVIC Preemption Priority Bits */  
//	NVIC_PriorityGroupConfig ( NVIC_PriorityGroup_2 );

	/* Enable the USART2 Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = macESP8266_USART_IRQ;	 
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

}


/*
 * Function   : ESP8266_Rst
 * Description: restarts the WF-ESP8266 module
 * Input      : none
 * Return  : none
 * Called by  : ESP8266_AT_Test
 */
//void ESP8266_Rst ( void )
//{
//	#if 0
//	 ESP8266_Cmd ( "AT+RST", "OK", "ready", 2500 );   	
//	
//	#else
//	 macESP8266_RST_LOW_LEVEL();
//	 delay_ms ( 500 ); 
//	 macESP8266_RST_HIGH_LEVEL();
//	#endif

//}


/*
 * Function   : ESP8266_Cmd
 * Description: sends an AT command to the WF-ESP8266 module
 * Input      : cmd, the command to send
 *         reply1, reply2, the expected responses; NULL means no response is needed, and the two are ORed
 *         waittime, how long to wait for a response
 * Return  : 1, the command was sent successfully
 *         0, the command failed
 * Called by  : external code
 */
bool ESP8266_Cmd ( char * cmd, char * reply1, char * reply2, u32 waittime )
{    
//	strEsp8266_Fram_Record .InfBit .FramLength = 0;               //start receiving a new packet
	uint16 count;
	uint16 delay_num;
	macESP8266_Usart ( "%s\r\n", cmd );

	if ( ( reply1 == 0 ) && ( reply2 == 0 ) )                      //No data needs to be received
		return true;
	
//	delay_ms ( waittime );                 //delay
//	
//	strEsp8266_Fram_Record .Data_RX_BUF [ strEsp8266_Fram_Record .InfBit .FramLength ]  = '\0';
	strEsp8266_Fram_Record .InfBit .FramLength = 0;
	strEsp8266_Fram_Record .InfBit .FramFinishFlag = 0;
	
	count = 0;
	delay_num = waittime / 50;
	while ( (! strEsp8266_Fram_Record .InfBit .FramFinishFlag) && (count++ < delay_num))
	{
		delay_ms(50) ;
		IWDG_ReloadCounter();
	}

	strEsp8266_Fram_Record .Data_RX_BUF [ strEsp8266_Fram_Record .InfBit .FramLength ] = '\0';
#if 0
	uart1_init(115200);
	DEBUG_EN = 1;
	printf("ret: %s \r\n",strEsp8266_Fram_Record .Data_RX_BUF);
#endif	
 
	if ( ( reply1 != 0 ) && ( reply2 != 0 ) )
	{
		return ( ( bool ) strstr ( strEsp8266_Fram_Record .Data_RX_BUF, reply1 ) || 
						 ( bool ) strstr ( strEsp8266_Fram_Record .Data_RX_BUF, reply2 ) ); 
 	}
	else if ( reply1 != 0 )
	{
		return ( ( bool ) strstr ( strEsp8266_Fram_Record .Data_RX_BUF, reply1 ) );
	}
	else
	{
		return ( ( bool ) strstr ( strEsp8266_Fram_Record .Data_RX_BUF, reply2 ) );
	}
}


/*
 * Function   : ESP8266_AT_Test
 * Description: runs the AT start-up test on the WF-ESP8266 module
 * Input      : none
 * Return  : none
 * Called by  : external code
 */
//void ESP8266_AT_Test ( void )
//{
//	macESP8266_RST_HIGH_LEVEL();
//	
//	delay_ms ( 1000 ); 
//	
//	while ( ! ESP8266_Cmd ( "AT", "OK", NULL, 500 ) ) ESP8266_Rst ();  	

//}
//char ESP8266_AT_Test ( void )
//{
//	char count=0;

//	macESP8266_RST_HIGH_LEVEL();	
//	delay_ms ( 1000 );

//	while ( count < 10 )
//	{
//		if( ESP8266_Cmd ( "AT", "OK", NULL, 500 ) ) 
//			return 2;
//		ESP8266_Rst();
//		++ count;
//	}
//	if(count == 10)
//		return 0;
//	else
//		return 1;
//}


/*
 * Function   : ESP8266_Net_Mode_Choose
 * Description: selects the working mode of the WF-ESP8266 module
 * Input      : enumMode, the working mode
 * Return  : 1, selected successfully
 *         0, selection failed
 * Called by  : external code
 */
bool ESP8266_Net_Mode_Choose ( ENUM_Net_ModeTypeDef enumMode )
{
	switch ( enumMode )
	{
		case STA:
			return ESP8266_Cmd ( "AT+CWMODE=1", "OK", "no change", 2500 ); 
		
	  case AP:
		  return ESP8266_Cmd ( "AT+CWMODE=2", "OK", "no change", 2500 ); 
		
		case STA_AP:
		  return ESP8266_Cmd ( "AT+CWMODE=3", "OK", "no change", 2500 ); 
		
	  default:
		  return false;
  }
	
}


/*
 * Function   : ESP8266_JoinAP
 * Description: connects the WF-ESP8266 module to an external WiFi network
 * Input      : pSSID, the WiFi name string
 *       : pPassWord, the WiFi password string
 * Return  : 1, connected
 *         0, connection failed
 * Called by  : external code
 */
bool ESP8266_JoinAP ( char * pSSID, char * pPassWord )
{
	char cCmd [120];

	sprintf ( cCmd, "AT+CWJAP=\"%s\",\"%s\"", pSSID, pPassWord );
	
	return ESP8266_Cmd ( cCmd, "OK", NULL, 5000 );
	
}


bool ESP8266_JoinAP_DEF( char * pSSID, char * pPassWord )
{
	char cCmd [120];

	sprintf ( cCmd, "AT+CWJAP_DEF=\"%s\",\"%s\"", pSSID, pPassWord );
	
	return ESP8266_Cmd ( cCmd, "OK", NULL, 5000 );
	
}

bool ESP8266_CIPSTA_DEF (void)
{
	char i;
	char cStr [100] = { 0 }, cCmd [120];
	char strip[4][4];
	char strsubnet[4][4];
	char strgetway[4][4];
	
	for(i=0;i<4;i++)
		itoa(SSID_Info.ip_addr[i],strip[i],10);
	for(i=0;i<4;i++)
		itoa(SSID_Info.net_mask[i],strsubnet[i],10);
	for(i=0;i<4;i++)
		itoa(SSID_Info.getway[i],strgetway[i],10);
  
//	sprintf ( cStr, "\"%s.%s.%s.%s\",\"192.168.10.1\",\"255.255.255.0\"", strip[0],strip[1],strip[2],strip[3]);	
	sprintf ( cStr, "\"%s.%s.%s.%s\",\"%s.%s.%s.%s\",\"%s.%s.%s.%s\"", 
	strip[0],strip[1],strip[2],strip[3],
	strgetway[0],strgetway[1],strgetway[2],strgetway[3],
	strsubnet[0],strsubnet[1],strsubnet[2],strsubnet[3]);
	
  sprintf ( cCmd, "AT+CIPSTA_DEF=%s",cStr);	
//	sprintf ( cCmd, "AT+CIPSTA_DEF=\"192.168.1.77\",\"192.168.1.1\",\"255.255.255.0\"");

#if 0
	uart1_init(115200);
	DEBUG_EN = 1;
	printf("cCMD: %s \r\n",cCmd);
#endif
	return ESP8266_Cmd (cCmd, "OK",0, 4000 );
	
}

/*
 * Function   : ESP8266_BuildAP
 * Description: makes the WF-ESP8266 module create a WiFi access point
 * Input      : pSSID, the WiFi name string
 *       : pPassWord, the WiFi password string
 *       : enunPsdMode, the WiFi encryption mode code string
 * Return  : 1, created
 *         0, creation failed
 * Called by  : external code
 */
bool ESP8266_BuildAP ( char * pSSID, char * pPassWord, ENUM_AP_PsdMode_TypeDef enunPsdMode )
{
	char cCmd [120];

	sprintf ( cCmd, "AT+CWSAP=\"%s\",\"%s\",1,%d", pSSID, pPassWord, enunPsdMode );
	
	return ESP8266_Cmd ( cCmd, "OK", 0, 1000 );
	
}


/*
 * Function   : ESP8266_Enable_MultipleId
 * Description: turns on multiple connections in the WF-ESP8266 module
 * Input      : enumEnUnvarnishTx, whether multiple connections are used
 * Return  : 1, configured
 *         0, configuration failed
 * Called by  : external code
 */
bool ESP8266_Enable_MultipleId ( FunctionalState enumEnUnvarnishTx )
{
	char cStr [20];
	
	sprintf ( cStr, "AT+CIPMUX=%d", ( enumEnUnvarnishTx ? 1 : 0 ) );
	
	return ESP8266_Cmd ( cStr, "OK", 0, 500 );
	
}


/*
 * Function   : ESP8266_Link_Server
 * Description: connects the WF-ESP8266 module to an external server
 * Input      : enumE, the network protocol
 *       : ip, the server IP string
 *       : ComNum, the server port string
 *       : id, the ID the module uses for this server connection
 * Return  : 1, connected
 *         0, connection failed
 * Called by  : external code
 */
bool ESP8266_Link_Server ( ENUM_NetPro_TypeDef enumE, char * ip, char * ComNum, ENUM_ID_NO_TypeDef id)
{
	char cStr [100] = { 0 }, cCmd [120];

  switch (  enumE )
  {
		case enumTCP:
		  sprintf ( cStr, "\"%s\",\"%s\",%s", "TCP", ip, ComNum );
		  break;
		
		case enumUDP:
		  sprintf ( cStr, "\"%s\",\"%s\",%s", "UDP", ip, ComNum );
		  break;
		
		default:
			break;
  }

  if ( id < 5 )
    sprintf ( cCmd, "AT+CIPSTART=%d,%s", id, cStr);

  else
	  sprintf( cCmd, "AT+CIPSTART=%s", cStr );

	return ESP8266_Cmd ( cCmd, "OK", "ALREAY CONNECT", 4000 );
	
}

bool ESP8266_Link_UDP( char * ip, uint16_t remoteport, uint16_t localport, uint8_t udpmode,ENUM_ID_NO_TypeDef id)
{
	char cStr [100] = { 0 }, cCmd [120];


	sprintf ( cStr, "\"%s\",\"%s\",%d,%d,%d", "UDP", ip, remoteport, localport,udpmode);


  if ( id < 5 )
    sprintf ( cCmd, "AT+CIPSTART=%d,%s", id, cStr);

  else
	  sprintf( cCmd, "AT+CIPSTART=%s", cStr );

	return ESP8266_Cmd ( cCmd, "OK", "ALREAY CONNECT", 4000 );
	
}
/*
 * Function   : ESP8266_StartOrShutServer
 * Description: starts or stops server mode on the WF-ESP8266 module
 * Input      : enumMode, start or stop
 *       : pPortNum, the server port number string
 *       : pTimeOver, the server timeout string, in seconds
 * Return  : 1, succeeded
 *         0, failed
 * Called by  : external code
 */
bool ESP8266_StartOrShutServer ( FunctionalState enumMode, char * pPortNum, char * pTimeOver )
{
	char cCmd1 [120], cCmd2 [120];

	if ( enumMode )
	{
		sprintf ( cCmd1, "AT+CIPSERVER=%d,%s", 1, pPortNum );
		return ( ESP8266_Cmd ( cCmd1, "OK", 0, 500 ));
//		sprintf ( cCmd2, "AT+CIPSTO=%s", pTimeOver );
//		
//		return ( ESP8266_Cmd ( cCmd1, "OK", 0, 500 ) &&
//						 ESP8266_Cmd ( cCmd2, "OK", 0, 500 ) );
	}
	
	else
	{
		sprintf ( cCmd1, "AT+CIPSERVER=%d,%s", 0, pPortNum );

		return ESP8266_Cmd ( cCmd1, "OK", 0, 500 );
	}
	
}


/*
 * Function   : ESP8266_Get_LinkStatus
 * Description: reads the WF-ESP8266 connection status; best suited to single-port use
 * Input      : none
 * Return  : 2, joined the AP and got an IP
 *         3, a connection is established
 *         4, the network connection dropped
 *				 5, not joined to an AP
 *         0, could not read the status
 *  			 6, WIFI_SSID_FAIL
 * Called by  : external code
 */
uint8_t ESP8266_Get_LinkStatus ( void )
{
	if ( ESP8266_Cmd ( "AT+CIPSTATUS", "OK", 0, 500 ) )
	{
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "STATUS:2\r\n" ) )
			return 2;
		
		else if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "STATUS:3\r\n" ) )
			return 3;
		
		else if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "STATUS:4\r\n" ) )
			return 4;	
		else if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "STATUS:5\r\n" ) )
			return 5;	 // no link

	}
	
	return 6;
	
}


/*
 * Function   : ESP8266_Get_IdLinkStatus
 * Description: reads the per-port (Id) connection status of the WF-ESP8266; best suited to multi-port use
 * Input      : none
 * Return  : the connection status per port (Id); the low 5 bits are valid and map to Id5~0, a set bit meaning that Id has a connection and a clear bit meaning it does not
 * Called by  : external code
 */
uint8_t ESP8266_Get_IdLinkStatus ( void )
{
	uint8_t ucIdLinkStatus = 0x00;
	
	
	if ( ESP8266_Cmd ( "AT+CIPSTATUS", "OK", 0, 500 ) )
	{
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:0," ) )
			ucIdLinkStatus |= 0x01;
		else 
			ucIdLinkStatus &= ~ 0x01;
		
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:1," ) )
			ucIdLinkStatus |= 0x02;
		else 
			ucIdLinkStatus &= ~ 0x02;
		
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:2," ) )
			ucIdLinkStatus |= 0x04;
		else 
			ucIdLinkStatus &= ~ 0x04;
		
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:3," ) )
			ucIdLinkStatus |= 0x08;
		else 
			ucIdLinkStatus &= ~ 0x08;
		
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTATUS:4," ) )
			ucIdLinkStatus |= 0x10;
		else 
			ucIdLinkStatus &= ~ 0x10;	

	}
	
	return ucIdLinkStatus;
	
}


//	AT+CIPSTAMAC?

//+CIPSTAMAC:"ec:fa:bc:40:e0:f0"

//OK
uint8_t ESP8266_Get_MAC(uint8_t * pStamac)
{
	char uc;
	char * pCh;
	char i;
	
	char pos;
	char datlen;
	uint8_t mac[6];
// GET MAC address
	ESP8266_Cmd ( "AT+CIPSTAMAC?", "OK", 0, 200 );	
	pos = 0;
	datlen = 0;
	pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTAMAC:\"" );
	
//	AT+CIPSTAMAC?

//+CIPSTAMAC:"ec:fa:bc:40:e0:f0"

//OK
	if( pCh )
		pCh += 11;
	
	else
	{
		return 0;
	}
	
	for ( uc = 0; uc < 40; uc ++ )
	{
		if(*( pCh + uc) == ':')
		{
			if(datlen == 2)
				pStamac[pos] = mac[0] * 16 + mac[1];
			pos++;
			datlen = 0;
		}
		else
		{
			if((pos == 5) && (* ( pCh + uc) == '\"'))
				break;
			
			if((* ( pCh + uc) >= '0') && (* ( pCh + uc) <= '9'))
				mac[datlen++] = * ( pCh + uc) - '0';
			else if((* ( pCh + uc) >= 'a') && (* ( pCh + uc) <= 'f'))
				mac[datlen++] = * ( pCh + uc) - 'a' + 10;
		}	
		
	}
	
	if(datlen == 2)
		pStamac[pos] = mac[0] * 16 + mac[1];
	return 1;
}


// AT+CIPSTAMAC_DEF
// AT+CIPSTAMAC_DEF="18:fe:35:98:d3:7b"
//
uint8_t ESP8266_Set_MAC(uint8_t * pStamac)
{
	char cStr[50];
	char ret;

	sprintf(cStr, "AT+CIPSTAMAC_DEF=\"%x:%x:%x:%x:%x:%x\"", pStamac[0],
	pStamac[1],pStamac[2],pStamac[3],pStamac[4],pStamac[5]);
//	sprintf(cStr, "AT+CIPSTAMAC_DEF=\"18:fe:35:98:d3:7b\"");
	ret = ESP8266_Cmd(cStr, "OK", 0, 1000);
	// 1 suc
	return ret;
}

char Get_SSID_RSSI(void)
{
	char uc;
	char * pCh;
	char i;
	
	char pos;
	char datlen;
	uint8_t rssi[3];

//AT+CWJAP_CUR? 
	//+CWJAP_CUR:"TEMCO_TEST_2.4G","40:a5:ef:5d:32:ca",13,-52
	ESP8266_Cmd ( "AT+CWJAP_CUR?", "OK", 0, 500 );	
	pos = 0;
	datlen = 0;
	pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CWJAP_CUR:" );
	
	if ( pCh )
		pCh += 12;
	else{
		// a send error can stop "busy s..." being received, so rule that case out
		pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "busy" );
		if ( pCh ) {
			return 2;}
		else {
			return 0;}
	}
	
	for ( uc = 0; uc < 60; uc ++ )
	{
		if(*( pCh + uc) == '"')
		{
			pCh += uc+1;
			datlen = 0;
			break;
		}
		else
		{
			SSID_Info.name[datlen++]=* ( pCh + uc);
		}
	}
	if(uc == 60) 
	{
			return 0;
	}
// now look for the SSID	
	if ( pCh )
		pCh += 21;
	else{
		return 0;}
	
	for ( uc = 0; uc < 5; uc ++ )
	{
		if(*( pCh + uc) == ',')
		{
			pCh += uc+1;
			datlen = 0;
			break;
		}
	}
	if(uc == 5) 
	{// error
			return 0;
	}
	// the signal strength follows the comma
	pos = 0;
	datlen = 0;
	for ( uc = 0; uc < 5; uc ++ )
	{
		if(*( pCh + uc) == '\0')
		{
			break;
		}
		if(*( pCh + uc) == '-')
		{
			datlen = 0;
		}
		else
		{
			if((* ( pCh + uc) >= '0') && (* ( pCh + uc) <= '9'))
				rssi[datlen++] = * ( pCh + uc) - '0';
		}
	}
	
	if(datlen == 1)
		SSID_Info.rssi = rssi[0];
	else if(datlen == 2)
		SSID_Info.rssi = rssi[0] * 10 + rssi[1];
	else if(datlen == 3)
		SSID_Info.rssi = rssi[0] * 100 + rssi[1] * 10 + rssi[2];	
	
	return 1;
}

/*
 * Function   : ESP8266_Inquire_ApIp
 * Description: reads the AP IP of the F-ESP8266
 * Input      : pApIp, start of the array holding the AP IP
 *         ucArrayLength, length of the array holding the AP IP
 * Return  : 0, failed
 *         1, succeeded
 * Called by  : external code
 */
uint8_t ESP8266_Inquire_ApIp (uint8_t * pStamac, uint8_t * pStaIp,uint8_t ucArrayLength )
{
	char uc;
	
	char * pCh;
	char i;
	
	char pos;
	char datlen;
	uint8_t ip[4];
	uint8_t mac[6];
	
  ESP8266_Cmd ( "AT+CIFSR", "OK", 0, 500 );
	
	pos = 0;
	datlen = 0;
	pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "STAIP,\"" );
	
	if ( pCh )
		pCh += 7;
	
	else
		return 0;
	
	for ( uc = 0; uc < ucArrayLength; uc ++ )
	{
		if(*( pCh + uc) == '.')
		{
			if(datlen == 1)
				pStaIp[pos] = ip[0];
			else if(datlen == 2)
				pStaIp[pos] = ip[0] * 10 + ip[1];
			else if(datlen == 3)
				pStaIp[pos] = ip[0] * 100 + ip[1] * 10 + ip[2];
			
			pos++;
			datlen = 0;
		}
		else
		{
			if((pos == 3) && (* ( pCh + uc) == '\"'))
				break;
			
			ip[datlen++] = * ( pCh + uc) - '0';
		}		

		
	}
	
	if(datlen == 1)
		pStaIp[pos] = ip[0];
	else if(datlen == 2)
		pStaIp[pos] = ip[0] * 10 + ip[1];
	else if(datlen == 3)
		pStaIp[pos] = ip[0] * 100 + ip[1] * 10 + ip[2];
	
// GET MAC address
	ESP8266_Cmd ( "AT+CIPSTAMAC?", "OK", 0, 500 );	
	pos = 0;
	datlen = 0;
	pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+CIPSTAMAC:\"" );
	
//	AT+CIPSTAMAC?

//+CIPSTAMAC:"ec:fa:bc:40:e0:f0"

//OK
	if ( pCh )
		pCh += 11;
	
	else
		return 0;
	
	for ( uc = 0; uc < ucArrayLength; uc ++ )
	{
		if(*( pCh + uc) == ':')
		{
			if(datlen == 2)
				pStamac[pos] = mac[0] * 16 + mac[1];
			
			pos++;
			datlen = 0;
		}
		else
		{
			if((pos == 5) && (* ( pCh + uc) == '\"'))
				break;
			
			if((* ( pCh + uc) >= '0') && (* ( pCh + uc) <= '9'))
				mac[datlen++] = * ( pCh + uc) - '0';
			else if((* ( pCh + uc) >= 'a') && (* ( pCh + uc) <= 'f'))
				mac[datlen++] = * ( pCh + uc) - 'a' + 10;
		}	
		
	}
	
	if(datlen == 2)
		pStamac[pos] = mac[0] * 16 + mac[1];	
	
	// GET SSID and password
	Get_SSID_RSSI();
	
	return 1;
	
}

//AT+CIPSTA_CUR?
//AT+CIPSTA_CUR?  query command, Station IP
//AT+CIPSTA_CUR? +CIPSTA_CUR:ip:"192.168.0.118"
//+CIPSTA_CUR:gateway:"192.168.0.4"
//+CIPSTA_CUR:netmask:"255.255.255.0"

/*
ret: 0 -- error
		1 -- ip dont change
		2 -- ip changed.

param:
type : 0 - dont check ip chagned
			 1 - check ip changed
*/
uint8_t ESP8266_CIPSTA_CUR(char type)
{
	char i;
	char datlen;
	char pos;
	char uc;	
	char * pCh;
	uint8_t temp[4];
	uint8_t Ip[4],gateway[4],netmask[4];
	
	ESP8266_Cmd ( "AT+CIPSTA_CUR?", "OK", 0, 1000 );
	pos = 0;
	datlen = 0;
	temp[0] = temp[1] = temp[2] = 0;
	// got the IP
	pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, ":ip:\"" );
	if ( pCh )
		pCh += 5;	
	else
	{
		return 0;
	}
	for ( uc = 0; uc < 40; uc ++ )
	{
		if(*( pCh + uc) == '.')
		{
			if(datlen == 1)
				Ip[pos] = temp[0];
			else if(datlen == 2)
				Ip[pos] = temp[0] * 10 + temp[1];
			else if(datlen == 3)
				Ip[pos] = temp[0] * 100 + temp[1] * 10 + temp[2];
			
			pos++;
			datlen = 0;
		}
		else
		{
			if((pos >= 3) && (* ( pCh + uc) == '\"'))
				break;
			if(datlen > 2)
			{
				return 0;
			}
				
			temp[datlen++] = * ( pCh + uc) - '0';			
		}			
	}
	
	if(uc == 40) 
	{Test[24]++;// error
			return 0;
	}
	// 161
	// work out the last byte, pos == 3
	if(datlen == 1)
		Ip[pos] = temp[0];
	else if(datlen == 2)
		Ip[pos] = temp[0] * 10 + temp[1];
	else if(datlen == 3)
		Ip[pos] = temp[0] * 100 + temp[1] * 10 + temp[2];

	if(type == 0)			
	{
		memcpy(&SSID_Info.ip_addr[0],Ip,4);
	}
	else
	{
		if((Ip[0] != SSID_Info.ip_addr[0]) || 
			(Ip[1] != SSID_Info.ip_addr[1])  ||
			(Ip[2] != SSID_Info.ip_addr[2]) ||
			(Ip[3] != SSID_Info.ip_addr[3]) )
		{
			memcpy(&SSID_Info.ip_addr[0],Ip,4);
			return 2;
		}
	}
	
	// get the gateway
	
	pos = 0;
	datlen = 0;
	temp[0] = temp[1] = temp[2] = 0;
	pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, ":gateway:\"" );
	
	if ( pCh )
		pCh += 10;	
	else
	{
		return 0;
	}
	
	for( uc = 0; uc < 40; uc ++ )
	{
		if(*( pCh + uc) == '.')
		{
			if(datlen == 1)
				gateway[pos] = temp[0];
			else if(datlen == 2)
				gateway[pos] = temp[0] * 10 + temp[1];
			else if(datlen == 3)
				gateway[pos] = temp[0] * 100 + temp[1] * 10 + temp[2];
			
			pos++;
			datlen = 0;
		}
		else
		{
			if((pos >= 3) && (* ( pCh + uc) == '\"'))
				break;
			if(datlen > 2)
			{
				return 0;
			}
				temp[datlen++] = * ( pCh + uc) - '0';
		}			
	}
	if(uc == 40) 
	{Test[25]++;// error
			return 0;
	}
	if(datlen == 1)
		gateway[pos] = temp[0];
	else if(datlen == 2)
		gateway[pos] = temp[0] * 10 + temp[1];
	else if(datlen == 3)
		gateway[pos] = temp[0] * 100 + temp[1] * 10 + temp[2];
	
	if(type == 0)			
	{
		memcpy(&SSID_Info.getway[0],gateway,4);
	}
	else
	{
		if((gateway[0] != SSID_Info.getway[0]) || 
			(gateway[1] != SSID_Info.getway[1]) ||
			(gateway[2] != SSID_Info.getway[2]) ||
			(gateway[3] != SSID_Info.getway[3]))
		{
			memcpy(&SSID_Info.getway[0],gateway,4);	
			return 2;
		}
	}
	
	// get the netmask

	pos = 0;
	datlen = 0;
	temp[0] = temp[1] = temp[2] = 0;
	pCh = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, ":netmask:\"" );
	
	if ( pCh )
		pCh += 10;	
	else
	{
		return 0;
	}
	
	for ( uc = 0; uc < 40; uc ++ )
	{
		if(*( pCh + uc) == '.')
		{
			if(datlen == 1)
				netmask[pos] = temp[0];
			else if(datlen == 2)
				netmask[pos] = temp[0] * 10 + temp[1];
			else if(datlen == 3)
				netmask[pos] = temp[0] * 100 + temp[1] * 10 + temp[2];
			
			pos++;
			datlen = 0;
		}
		else
		{
			if((pos >= 3) && (* ( pCh + uc) == '\"'))
				break;
			if(datlen > 2)
			{
				return 0;
			}
			temp[datlen++] = * ( pCh + uc) - '0';
		}			
	}
	if(uc == 40) 
	{// error
			return 0;
	}
	if(datlen == 1)
		netmask[pos] = temp[0];
	else if(datlen == 2)
		netmask[pos] = temp[0] * 10 + temp[1];
	else if(datlen == 3)
		netmask[pos] = temp[0] * 100 + temp[1] * 10 + temp[2];
	
	if(type == 0)	
	{
		memcpy(&SSID_Info.net_mask[0],netmask,4);
	}
	else
	{
		if((netmask[0] != SSID_Info.net_mask[0]) || 
			(netmask[1] != SSID_Info.net_mask[1])  ||
			(netmask[2] != SSID_Info.net_mask[2]) ||
			(netmask[3] != SSID_Info.net_mask[3]) )
		{
			memcpy(&SSID_Info.net_mask[0],netmask,4);
			return 2;
		}
	}
	return 1;
}

/*
 * Function   : ESP8266_UnvarnishSend
 * Description: puts the WF-ESP8266 module into transparent transmission mode
 * Input      : none
 * Return  : 1, configured
 *         0, configuration failed
 * Called by  : external code
 */
bool ESP8266_UnvarnishSend ( void )
{
	if ( ! ESP8266_Cmd ( "AT+CIPMODE=1", "OK", 0, 500 ) )
		return false;
	
	return 
	  ESP8266_Cmd ( "AT+CIPSEND", "OK", ">", 500 );
	
}


/*
 * Function   : ESP8266_ExitUnvarnishSend
 * Description: takes the WF-ESP8266 module out of transparent transmission mode
 * Input      : none
 * Return  : none
 * Called by  : external code
 */
void ESP8266_ExitUnvarnishSend ( void )
{
	delay_ms ( 1000 );
	
	macESP8266_Usart ( "+++" );
	
	delay_ms ( 500 ); 
	
}
extern uint16_t bacnet_wifi_len;
extern u8 rec_mstp_index;
extern u8 rec_mstp_index1;

/*
 * Function   : ESP8266_SendString
 * Description: sends a string from the WF-ESP8266 module
 * Input      : enumEnUnvarnishTx, states whether transparent mode is already on
 *       : pStr, the string to send
 *       : ulStrLength, the length of the string in bytes
 *       : ucId, which ID sends the string
 * Return  : 1, sent
 *         0, sending failed
 * Called by  : external code
 */
bool ESP8266_SendString ( FunctionalState enumEnUnvarnishTx, uint8_t * pStr, u32 ulStrLength, ENUM_ID_NO_TypeDef ucId )
{
	uint8_t cStr [600];
	u16 i,j;
  u16 temp_length; //Work out the length to send
									
	bool bRet = false;

	if(ulStrLength > 550) {
		return 0;}

	if ( enumEnUnvarnishTx )
	{
		macESP8266_Usart ( "%s", pStr );
		delay_ms ( 2 );
		bRet = true;		
	}
	else
	{
			if (ucId < 5)
					sprintf(cStr, "AT+CIPSEND=%d,%d\r\n", ucId, ulStrLength);
			else
					sprintf(cStr, "AT+CIPSEND=%d\r\n", ulStrLength);
			temp_length = strlen(cStr);
			if ((temp_length == 0) || (temp_length > 540))
			{
					return 0;
			}
			Send_Uart_Data(cStr, temp_length);
	
			delay_ms ( 2 );

			Send_Uart_Data(pStr, ulStrLength);
			delay_ms ( 1 );
			
			strEsp8266_Fram_Record .InfBit .FramLength = 0;
  }

	return bRet;

}


/*
 * Function   : ESP8266_ReceiveString
 * Description: receives a string on the WF-ESP8266 module
 * Input      : enumEnUnvarnishTx, states whether transparent mode is already on
 * Return  : the start address of the received string
 * Called by  : external code
 */
uint8_t * ESP8266_ReceiveString ( FunctionalState enumEnUnvarnishTx )
{
	uint8_t * pRecStr = NULL;
	uint16_t count;
	
	strEsp8266_Fram_Record .InfBit .FramLength = 0;
	strEsp8266_Fram_Record .InfBit .FramFinishFlag = 0;
	memset(&strEsp8266_Fram_Record,0,sizeof(strEsp8266_Fram_Record));
	
	count = 0;
	while ( (! strEsp8266_Fram_Record .InfBit .FramFinishFlag) && (count++ < 100))
	{
		delay_ms(10) ;
		IWDG_ReloadCounter();
	}
	strEsp8266_Fram_Record .Data_RX_BUF [ strEsp8266_Fram_Record .InfBit .FramLength ] = '\0';

	if ( enumEnUnvarnishTx )
		pRecStr = strEsp8266_Fram_Record .Data_RX_BUF;
	
	else 
	{
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+IPD" ) )
			pRecStr = strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+IPD" );
	}
	
	return pRecStr;
	
}

