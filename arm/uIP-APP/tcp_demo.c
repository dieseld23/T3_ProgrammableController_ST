#include "stm32f10x.h"
#include "usart.h"	 		   
#include "uip.h"	    
#include "enc28j60.h"
#include "httpd.h"
#include "tcp_demo.h"

//The TCP application entry point (UIP_APPCALL)
//Provides the TCP services, both server and client, and the HTTP service


//Used for logging
void uip_log(char *m)
{
	//printf("uIP log:%s\r\n", m);
}
