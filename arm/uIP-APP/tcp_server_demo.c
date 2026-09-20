#include "tcp_demo.h"
#include "uip.h"
#include <string.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "led.h"

#include "modbus.h"

#if 0
u8 tcp_server_databuf[500];   	//Transmit buffer	  
u8 tcp_server_sta;				//Server state
//[7]: 0 = not connected; 1 = connected;
//[6]: 0 = no data; 1 = data received from the client
//[5]: 0 = no data; 1 = data waiting to be sent

 	   
//This is the TCP server application callback.
//It is reached through UIP_APPCALL (tcp_demo_appcall) and provides the web server.
//When a uIP event happens UIP_APPCALL is called, and the port (1200) decides whether this runs.
//For example: a TCP connection is created, new data arrives, data has been acknowledged, data needs resending, and so on
void tcp_server_demo_appcall(void)
{
 	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
	if(uip_aborted())tcp_server_aborted();		//Connection aborted
 	if(uip_timedout())tcp_server_timedout();	//Connection timed out   
	if(uip_closed())tcp_server_closed();		//Connection closed	   
 	if(uip_connected())tcp_server_connected();	//Connected	    
	if(uip_acked())tcp_server_acked();			//The data was delivered successfully 
	//A new TCP packet has arrived 
	if(uip_newdata())//Data arrived from the client
	{
		if((tcp_server_sta & (1 << 6)) == 0)	//No data received yet
		{
//			if(uip_len > 499)
//			{		   
//				((u8*)uip_appdata)[499] = 0;
//			}
		    
//	    	strcpy((char*)tcp_server_databuf, uip_appdata);
			modbus_data_cope((u8*)uip_appdata, uip_len, 1);
			tcp_server_sta |= 1 << 6;			//Means data was received from the client
			
			sprintf((char*)tcp_server_databuf, "TCP Server OK.............\r\n");	 
			tcp_server_sta |= 1 << 5;			//Flag that there is data to send
		}
	}
	
	/*else*/ if(tcp_server_sta & (1 << 5))			//There is data waiting to be sent
	{
		s->textptr = tcp_server_databuf;
		s->textlen = strlen((const char*)tcp_server_databuf);
		tcp_server_sta &= ~(1 << 5);			//Clear the flag
	}
	
	//Tell uIP to send data on a resend, on new data arriving, on a packet being delivered, and on a connection being established 
	if(uip_rexmit() || uip_newdata() || uip_acked() || uip_connected() || uip_poll())
	{
		tcp_server_senddata();
	}
}

//Abort the connection				    
void tcp_server_aborted(void)
{
	tcp_server_sta &= ~(1 << 7);				//Flag: not connected
	uip_log("tcp_server aborted!\r\n");			//Print the log
}

//Connection timed out
void tcp_server_timedout(void)
{
	tcp_server_sta &= ~(1 << 7);				//Flag: not connected
	uip_log("tcp_server timeout!\r\n");			//Print the log
}

//Connection closed
void tcp_server_closed(void)
{
	tcp_server_sta &= ~(1 << 7);				//Flag: not connected
	uip_log("tcp_server closed!\r\n");			//Print the log
}

//Connection established
void tcp_server_connected(void)
{								  
	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
	//struct uip_conn has an "appstate" field pointing at the application's own structure.
	//The s pointer is declared purely for convenience.
 	//There is no need to allocate memory per uip_conn; uIP has already done it.
	//The relevant code in uip.c is:
	//		struct uip_conn *uip_conn;
	//		struct uip_conn uip_conns[UIP_CONNS]; //UIP_CONNS defaults to 10
	//That array of connections allows several to exist at once.
	//uip_conn is a global pointer to the current TCP or UDP connection.
	tcp_server_sta |= 1 << 7;					//Flag the connection as established
  	uip_log("tcp_server connected!\r\n");		//Print the log
	s->state = STATE_CMD; 						//Command state
	s->textlen = 0;
	s->textptr = "Connect to STM32 Board Successfully!\r\n";
	s->textlen = strlen((char *)s->textptr);
}

//The data was delivered successfully
void tcp_server_acked(void)
{						    	 
	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
	s->textlen = 0;								//Clear the send flag
	uip_log("tcp_server acked!\r\n");			//Means it was sent successfully		 
}

//Send data to the client
void tcp_server_senddata(void)
{
	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
	//s->textptr : pointer to the buffer being sent
	//s->textlen : the size of the packet in bytes		   
	if(s->textlen > 0)
		uip_send(s->textptr, s->textlen);//Send a TCP packet	 
}
#endif