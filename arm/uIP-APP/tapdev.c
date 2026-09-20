/*
 * Copyright (c) 2001, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: tapdev.c,v 1.8 2006/06/07 08:39:58 adam Exp $
 */	    
#include "tapdev.h"
#include "uip.h"
#include "enc28j60.h"
#include <stdio.h>
#include "tcpip.h"
#include "define.h"

//#define DHCP_ENABLE

//Used to set the IP once the fixed-IP option is on; this example does not use it
#define UIP_DRIPADDR0   192
#define UIP_DRIPADDR1   168
#define UIP_DRIPADDR2   0
#define UIP_DRIPADDR3   111

void uip_polling(void);
//MAC address, which must be unique
//const u8 mymac[6]={0x04, 0x02, 0x35, 0x0F, 0x00, 0x01};	//MAC address
extern uint32 multicast_addr;																				  
//Configure the network hardware and set the MAC address 
//Return: 0 = ok; 1 = failed;
u8 tapdev_init(void)
{  
	u32 wait_count;	
	u8 i, res = 0;
#ifndef	DHCP_ENABLE
	uip_ipaddr_t ipaddr;
#endif
		
	res = ENC28J60_Init((u8*)Modbus.mac_addr);	//Initialise the ENC28J60					  
	//Write the IP and MAC addresses into the buffer
	for(i = 0; i < 6; i++)
	{
		uip_ethaddr.addr[i] = Modbus.mac_addr[i];
	}
    //LED state: 0x476 is PHLCON, LEDA (green) = link status, LEDB (red) = receive/transmit
 	//PHLCON: the PHY module LED control register	    
	ENC28J60_PHY_Write(PHLCON, 0x0476);
	uip_init();							//uIP initialisation	
	if(Modbus.tcp_type == 0)	
	{
		U8_T temp[4];
		uip_ipaddr(ipaddr, Modbus.ip_addr[0], Modbus.ip_addr[1], Modbus.ip_addr[2], Modbus.ip_addr[3]);	//Set the local IP address
		uip_sethostaddr(ipaddr);					    
		uip_ipaddr(ipaddr, Modbus.getway[0], Modbus.getway[1], Modbus.getway[2], Modbus.getway[3]); 	//Set the gateway IP address, which is your router's address
		uip_setdraddr(ipaddr);						 
		uip_ipaddr(ipaddr, Modbus.subnet[0], Modbus.subnet[1], Modbus.subnet[2], Modbus.subnet[3]);	//Set the netmask
		uip_setnetmask(ipaddr);
//		flag_dhcp_configured = 2;

	temp[0] = Modbus.ip_addr[0];
	temp[1] = Modbus.ip_addr[1];
	temp[2] = Modbus.ip_addr[2];
	temp[3] = Modbus.ip_addr[3];
	
	temp[0] |= (255 - Modbus.subnet[0]);
	temp[1] |= (255 - Modbus.subnet[1]);
	temp[2] |= (255 - Modbus.subnet[2]);
	temp[3] |= (255 - Modbus.subnet[3]);
	
	uip_ipaddr(uip_hostaddr_submask,temp[0], temp[1],temp[2] ,temp[3]);
	multicast_addr = temp[3] + (U16_T)(temp[2] << 8) \
		+ ((U32_T)temp[1] << 16) + ((U32_T)temp[0] << 24);
	delay_ms(1);
		
	}
	else  // DHCP
	{
//		flag_dhcp_configured = 0;
		dhcpc_init(Modbus.mac_addr, 6);
		dhcpc_request();	
	}
	
//	printf("res=%u\n\r",res);
	return res;	
}
//Read one packet  
uint16_t tapdev_read(void)
{	
	return  ENC28J60_Packet_Receive(MAX_FRAMELEN, uip_buf);
}

//Send one packet  
u8 tapdev_send(void)
{
	return ENC28J60_Packet_Send(uip_len, uip_buf);
}
