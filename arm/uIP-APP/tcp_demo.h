#ifndef __TCP_DEMO_H__
#define __TCP_DEMO_H__		 
/* Since this file will be included by uip.h, we cannot include uip.h
   here. But we might need to include uipopt.h if we need the u8_t and
   u16_t datatypes. */
#include "uipopt.h"
#include "psock.h"
#include "bitmap.h"

//Communication state word (define your own)  
enum
{
	STATE_CMD		= 0,	//Command receive state 
	STATE_TX_TEST	= 1,	//Continuous send state (speed test)  
	STATE_RX_TEST	= 2		//Continuous receive state (speed test)  
};	 
//Defines the uip_tcp_appstate_t type; add whatever members your application
//needs. Do not rename the type, because uIP refers to it by name.
//struct uip_conn in uip.h refers to uip_tcp_appstate_t		  
struct tcp_demo_appstate
{
	u8_t state;
	u8_t *textptr;
	int textlen;
};	 
typedef struct tcp_demo_appstate uip_tcp_appstate_t;

void tcp_appcall(void);
void tcp_client_demo_appcall(void);
void tcp_server_demo_appcall(void);

//Define the application callback 
#ifndef UIP_APPCALL
#define UIP_APPCALL tcp_appcall //The callback is tcp_demo_appcall 
#endif
/////////////////////////////////////TCP SERVER/////////////////////////////////////
extern u8 tcp_server_databuf[];   		//Transmit buffer	 
extern u8 tcp_server_sta;				//Server state  


extern u8_t uip_server_time[UIP_CONF_MAX_LISTENPORTS]; // added by chelsea

//TCP server functions
void tcp_server_aborted(void);
void tcp_server_timedout(void);
void tcp_server_closed(void);
void tcp_server_connected(void);
void tcp_server_newdata(void);
void tcp_server_acked(void);
void tcp_server_senddata(void);
/////////////////////////////////////TCP CLIENT/////////////////////////////////////
extern u8 tcp_client_databuf[];   		//Transmit buffer	 
//extern u8 tcp_client_sta;				//client state   
void tcp_client_reconnect(void);
void tcp_client_connected(void);
void tcp_client_aborted(void);
void tcp_client_timedout(void);
void tcp_client_closed(void);
void tcp_client_acked(void);
void tcp_client_senddata(void);
////////////////////////////////////////////////////////////////////////////////////


#endif
























