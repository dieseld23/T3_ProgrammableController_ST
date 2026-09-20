#include "tcp_demo.h"
#include "stm32f10x.h"
#include "uip.h"
#include <string.h>
#include <stdio.h>	   


u8 tcp_client_databuf[500];   	//Transmit buffer	  
u8 tcp_client_sta;				//Client state
//[7]: 0 = not connected; 1 = connected;
//[6]: 0 = no data; 1 = data received from the client
//[5]: 0 = no data; 1 = data waiting to be sent

//This is the TCP client application callback.
//It is reached through UIP_APPCALL (tcp_demo_appcall) and provides the web client.
//When a uIP event happens UIP_APPCALL is called, and the port (1400) decides whether this runs.
//For example: a TCP connection is created, new data arrives, data has been acknowledged, data needs resending, and so on
void tcp_client_demo_appcall(void)
{		  
 	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
	if(uip_aborted())tcp_client_aborted();		//Connection aborted	   
	if(uip_timedout())tcp_client_timedout();	//Connection timed out   
	if(uip_closed())tcp_client_closed();		//Connection closed	   
 	if(uip_connected())tcp_client_connected();	//Connected	    
	if(uip_acked())tcp_client_acked();			//The data was delivered successfully 
 	//A new TCP packet has arrived 
	if(uip_newdata())
	{
		if((tcp_client_sta & (1 << 6)) == 0)	//No data received yet
		{
			if(uip_len > 499)
			{		   
				((u8*)uip_appdata)[499] = 0;
			}		    
	    	strcpy((char*)tcp_client_databuf, uip_appdata);				   	  		  
			tcp_client_sta |= 1 << 6;			//Means data was received from the client
		}				  
	}
	else if(tcp_client_sta & (1 << 5))			//There is data waiting to be sent
	{
		s->textptr = tcp_client_databuf;
		s->textlen = strlen((const char*)tcp_client_databuf);
		tcp_client_sta &= ~(1 << 5);			//Clear the flag
	}
	
	//Tell uIP to send data on a resend, on new data arriving, on a packet being delivered, and on a connection being established 
	if(uip_rexmit() || uip_newdata() || uip_acked() || uip_connected() || uip_poll())
	{
		tcp_client_senddata();
	}											   
}

//The server is assumed to be at 192.168.0.111
//Change this to match the server's actual address.
//Try to reconnect
void tcp_client_reconnect(void)
{
	uip_ipaddr_t ipaddr;
	uip_ipaddr(&ipaddr, 192, 168, 0, 124);		//Set the IP to 192.168.1.103
	uip_connect(&ipaddr, htons(1400)); 			//Port 1400
}

//Abort the connection				    
void tcp_client_aborted(void)
{
	tcp_client_sta &= ~(1 << 7);				//Flag: not connected
	tcp_client_reconnect();						//Try to reconnect
//	uip_log("tcp_client aborted!\r\n");			//print the log
}

//Connection timed out
void tcp_client_timedout(void)
{
	tcp_client_sta &= ~(1 << 7);				//Flag: not connected	   
//	uip_log("tcp_client timeout!\r\n");			//print the log
}

//Connection closed
void tcp_client_closed(void)
{
	tcp_client_sta &= ~(1 << 7);				//Flag: not connected
	tcp_client_reconnect();						//Try to reconnect
//	uip_log("tcp_client closed!\r\n");			//print the log
}

//Connection established
void tcp_client_connected(void)
{ 
	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
 	tcp_client_sta |= 1 << 7;					//Flag the connection as established
 // 	uip_log("tcp_client connected!\r\n");		//print the log
	s->state = STATE_CMD;				 		//Command state
	s->textlen = 0;
	s->textptr = "Demo Board Connected Successfully!\r\n";//The reply
	s->textlen = strlen((char *)s->textptr);	  
}

//The data was delivered successfully
void tcp_client_acked(void)
{											    
	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
	s->textlen = 0;								//Clear the send flag
//	uip_log("tcp_client acked!\r\n");			//means it was sent successfully		 
}

//Send data to the server
void tcp_client_senddata(void)
{
	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
	//s->textptr: pointer to the buffer being sent
	//s->textlen: the size of the packet in bytes		   
	if(s->textlen > 0)
		uip_send(s->textptr, s->textlen);//Send a TCP packet	 
}
