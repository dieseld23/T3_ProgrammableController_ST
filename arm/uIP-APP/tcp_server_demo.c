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

 	   
//这是一个TCP 服务器应用回调函数。
//该函数通过UIP_APPCALL(tcp_demo_appcall)调用,实现Web Server的功能.
//当uip事件发生时，UIP_APPCALL函数会被调用,根据所属端口(1200),确定是否执行该函数。
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
	if(uip_newdata())//收到客户端发过来的数据
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
			tcp_server_sta |= 1 << 5;			//标记有数据需要发送
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
	//uip_conn结构体有一个"appstate"字段指向应用程序自定义的结构体。
	//声明一个s指针，是为了便于使用。
 	//不需要再单独为每个uip_conn分配内存，这个已经在uip中分配好了。
	//在uip.c 中 的相关代码如下：
	//		struct uip_conn *uip_conn;
	//		struct uip_conn uip_conns[UIP_CONNS]; //UIP_CONNS缺省=10
	//定义了1个连接的数组，支持同时创建几个连接。
	//uip_conn是一个全局的指针，指向当前的tcp或udp连接。
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
	uip_log("tcp_server acked!\r\n");			//表示成功发送		 
}

//发送数据给客户端
void tcp_server_senddata(void)
{
	struct tcp_demo_appstate *s = (struct tcp_demo_appstate *)&uip_conn->appstate;
	//s->textptr : 发送的数据包缓冲区指针
	//s->textlen ：数据包的大小（单位字节）		   
	if(s->textlen > 0)
		uip_send(s->textptr, s->textlen);//Send a TCP packet	 
}
#endif