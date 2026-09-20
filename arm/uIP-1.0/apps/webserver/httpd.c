/**
 * \addtogroup apps
 * @{
 */

/**
 * \defgroup httpd Web server
 * @{
 * The uIP web server is a very simplistic implementation of an HTTP
 * server. It can serve web pages and files from a read-only ROM
 * filesystem, and provides a very small scripting language.

 */

/**
 * \file
 *         Web server
 * \author
 *         Adam Dunkels <adam@sics.se>
 */


/*
 * Copyright (c) 2004, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * This file is part of the uIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: httpd.c,v 1.2 2006/06/11 21:46:38 adam Exp $
 */

#include "uip.h"
#include "httpd.h"
#include "httpd-fs.h"
#include "httpd-cgi.h"
#include "http-strings.h"

#include <string.h>
#include <stdio.h>

#define STATE_WAITING 0
#define STATE_OUTPUT  1

#define ISO_nl      0x0a
#define ISO_space   0x20
#define ISO_bang    0x21
#define ISO_percent 0x25
#define ISO_period  0x2e
#define ISO_slash   0x2f
#define ISO_colon   0x3a

#include "led.h"

/*---------------------------------------------------------------------------*/
static unsigned short
generate_part_of_file(void *state)
{
  struct httpd_state *s = (struct httpd_state *)state;

  if(s->file.len > uip_mss()) {
    s->len = uip_mss();
  } else {
    s->len = s->file.len;
  }
  memcpy(uip_appdata, s->file.data, s->len);
  
  return s->len;
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_file(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sout);
  
  do {
    PSOCK_GENERATOR_SEND(&s->sout, generate_part_of_file, s);
    s->file.len -= s->len;
    s->file.data += s->len;
  } while(s->file.len > 0);
      
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_part_of_file(struct httpd_state *s))
{
  PSOCK_BEGIN(&s->sout);

  PSOCK_SEND(&s->sout, s->file.data, s->len);
  
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static void
next_scriptstate(struct httpd_state *s)
{
  char *p;
  p = strchr(s->scriptptr, ISO_nl) + 1;
  s->scriptlen -= (unsigned short)(p - s->scriptptr);
  s->scriptptr = p;
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_script(struct httpd_state *s))
{
  char *ptr;
  
  PT_BEGIN(&s->scriptpt);


  while(s->file.len > 0) {

    /* Check if we should start executing a script. */
    if(*s->file.data == ISO_percent &&
       *(s->file.data + 1) == ISO_bang) {
      s->scriptptr = s->file.data + 3;
      s->scriptlen = s->file.len - 3;
      if(*(s->scriptptr - 1) == ISO_colon) {
	httpd_fs_open(s->scriptptr + 1, &s->file);
	PT_WAIT_THREAD(&s->scriptpt, send_file(s));
      } else {
	PT_WAIT_THREAD(&s->scriptpt,
		       httpd_cgi(s->scriptptr)(s, s->scriptptr));
      }
      next_scriptstate(s);
      
      /* The script is over, so we reset the pointers and continue
	 sending the rest of the file. */
      s->file.data = s->scriptptr;
      s->file.len = s->scriptlen;
    } else {
      /* See if we find the start of script marker in the block of HTML
	 to be sent. */

      if(s->file.len > uip_mss()) {
	s->len = uip_mss();
      } else {
	s->len = s->file.len;
      }

      if(*s->file.data == ISO_percent) {
	ptr = strchr(s->file.data + 1, ISO_percent);
      } else {
	ptr = strchr(s->file.data, ISO_percent);
      }
      if(ptr != NULL &&
	 ptr != s->file.data) {
	s->len = (int)(ptr - s->file.data);
	if(s->len >= uip_mss()) {
	  s->len = uip_mss();
	}
      }
      PT_WAIT_THREAD(&s->scriptpt, send_part_of_file(s));
      s->file.data += s->len;
      s->file.len -= s->len;
      
    }
  }
  
  PT_END(&s->scriptpt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(send_headers(struct httpd_state *s, const char *statushdr))
{
  char *ptr;

  PSOCK_BEGIN(&s->sout);

  PSOCK_SEND_STR(&s->sout, statushdr);

  ptr = strrchr(s->filename, ISO_period);
  if(ptr == NULL) {
    PSOCK_SEND_STR(&s->sout, http_content_type_binary);
  } else if(strncmp(http_html, ptr, 5) == 0 ||
	    strncmp(http_shtml, ptr, 6) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_html);
  } else if(strncmp(http_css, ptr, 4) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_css);
  } else if(strncmp(http_png, ptr, 4) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_png);
  } else if(strncmp(http_gif, ptr, 4) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_gif);
  } else if(strncmp(http_jpg, ptr, 4) == 0) {
    PSOCK_SEND_STR(&s->sout, http_content_type_jpg);
  } else {
    PSOCK_SEND_STR(&s->sout, http_content_type_plain);
  }
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_output(struct httpd_state *s))
{
	char *ptr;		  
	PT_BEGIN(&s->outputpt);		  
	if(!httpd_fs_open(s->filename,&s->file))//The HTML file could not be opened 
	{
		httpd_fs_open(http_404_html, &s->file);
		strcpy(s->filename, http_404_html);
		PT_WAIT_THREAD(&s->outputpt,
		send_headers(s,http_header_404));	//Send the 404 page
		PT_WAIT_THREAD(&s->outputpt,send_file(s));
	}else //The HTML file opened
	{
		PT_WAIT_THREAD(&s->outputpt,send_headers(s,http_header_200));
		ptr=strchr(s->filename, ISO_period);
		if(ptr != NULL && strncmp(ptr,http_shtml,6) == 0)//Check whether the extension is .SHTML 
		{
			PT_INIT(&s->scriptpt);
			PT_WAIT_THREAD(&s->outputpt, handle_script(s));
		}else
		{
			PT_WAIT_THREAD(&s->outputpt,send_file(s));
		}
	}
	PSOCK_CLOSE(&s->sout);
	PT_END(&s->outputpt);
}
/*---------------------------------------------------------------------------*/
extern unsigned char data_index_html[];	//Defined in httpd-fsdata.c, which holds the HTML page source
extern void get_temperature(u8 *temp);	//Implemented in main; returns the temperature string
extern void get_time(u8 *time);		    //Implemented in main; returns the time string

const char * LED0_ON_PIC_ADDR="http://www.openedv.com/upload/2012/9/27/ad65ee9f478ca11241933beed5b5dbcc_971.gif";	//LED0 on, icon address
const char * LED1_ON_PIC_ADDR="http://www.openedv.com/upload/2012/9/27/bab5bef0379dc50129202157c2739c57_775.gif";	//LED1 on, icon address
const char * LED_OFF_PIC_ADDR="http://www.openedv.com/upload/2012/9/27/ccecf4ebeb84b095545b8feb0cecc671_254.gif";	//LED off, icon address
//Handle incoming HTTP data
static PT_THREAD(handle_input(struct httpd_state *s))
{		    	  
	char *strx;
	u8 dbuf[17];
	PSOCK_BEGIN(&s->sin);		 
	PSOCK_READTO(&s->sin, ISO_space);	 
	if(strncmp(s->inputbuf, http_get, 4)!=0)PSOCK_CLOSE_EXIT(&s->sin);	//Check whether what the browser sent is a web request, "GET " 	   
	PSOCK_READTO(&s->sin, ISO_space);		     						//" "
	if(s->inputbuf[0] != ISO_slash)PSOCK_CLOSE_EXIT(&s->sin);		 	//Check whether the first item after the IP address is "/"
	if(s->inputbuf[1] == ISO_space||s->inputbuf[1] == '?') 				//The second item is a space or a question mark
	{ 
		if(s->inputbuf[1]=='?'&&s->inputbuf[6]==0x31)//LED1  
		{
			printf("Change LED0 Status\r\n");
			LED2 = !LED2;	
			strx=strstr((const char*)(data_index_html+13),"LED2×´Ì¬");  
			if(strx)//The "LED0 state" string is present
			{
				strx=strstr((const char*)strx,"color:#");//Found the "color:#" string
				if(LED0)//LED0 off
				{
					strncpy(strx+7,"5B5B5B",6);	//Grey
					strncpy(strx+24,"Ãð",2);	//off
					strx=strstr((const char*)strx,"http:");//Found the "http:" string 
					strncpy(strx,(const char*)LED_OFF_PIC_ADDR,strlen((const char*)LED_OFF_PIC_ADDR));//LED0 off image	  
				}else
				{
					strncpy(strx+7,"FF0000",6);	//Red
					strncpy(strx+24,"ÁÁ",2);	//"on"
					strx=strstr((const char*)strx,"http:");//Found the "http:" string 
					strncpy(strx,(const char*)LED0_ON_PIC_ADDR,strlen((const char*)LED0_ON_PIC_ADDR));//LED0 on image	  
				}	
			}  
		}
		else if(s->inputbuf[1]=='?'&&s->inputbuf[6]==0x32)//LED2  
		{
			printf("Change LED1 Status\r\n");
			LED3 = !LED3;	
			strx=strstr((const char*)(data_index_html+13),"LED3×´Ì¬");  
			if(strx)//The "LED1 state" string is present
			{
				strx=strstr((const char*)strx,"color:#");//Found the "color:#" string
				if(LED3)//LED1 off
				{
					strncpy(strx+7,"5B5B5B",6);	//Grey
					strncpy(strx+24,"Ãð",2);	//off
					strx=strstr((const char*)strx,"http:");//Found the "http:" string 
					strncpy(strx,(const char*)LED_OFF_PIC_ADDR,strlen((const char*)LED_OFF_PIC_ADDR));//LED1 off image	  
				}else
				{
					strncpy(strx+7,"00FF00",6);	//Green
					strncpy(strx+24,"ÁÁ",2);	//"on"
					strx=strstr((const char*)strx,"http:");//Found the "http:" string 
					strncpy(strx,(const char*)LED1_ON_PIC_ADDR,strlen((const char*)LED1_ON_PIC_ADDR));//LED1 on image	  
				}	
			} 
		}
		
		strx = strstr((const char*)(data_index_html+13),"¡æ");//Found the degree-C character
		if(strx)
		{
//			get_temperature(dbuf);	//get the temperature	  
			strncpy(strx-4,(const char*)dbuf,4);	//Update the temperature
			printf("Update Temperature\r\n");			
		}
		strx=strstr((const char*)strx,"RTCÊ±¼ä:");//Found the RTC time marker
		if(strx)
		{
//			get_time(dbuf);			//get the time  
			strncpy(strx+33,(const char*)dbuf,16);	//Update the time
			printf("Update RTC\r\n");
		}
		strncpy(s->filename, http_index_html, sizeof(s->filename));
	}
	else //If it is neither ' ' nor '?'
	{
		s->inputbuf[PSOCK_DATALEN(&s->sin)-1] = 0;
		strncpy(s->filename,&s->inputbuf[0],sizeof(s->filename));
	}   
	s->state = STATE_OUTPUT;	    
	while(1) 
	{
		PSOCK_READTO(&s->sin, ISO_nl);
		if(strncmp(s->inputbuf, http_referer, 8) == 0) 
		{
			s->inputbuf[PSOCK_DATALEN(&s->sin) - 2] = 0;		   
		}
	}								    
	PSOCK_END(&s->sin);
}
/*---------------------------------------------------------------------------*/
//Parse the HTTP data
static void handle_connection(struct httpd_state *s)
{
	handle_input(s);  //Handle the incoming HTTP data
	if(s->state==STATE_OUTPUT)handle_output(s);//Output state, and handling of the outgoing data 
}
/*---------------------------------------------------------------------------*/
//HTTP (web) service handling
void httpd_appcall(void)
{
	struct httpd_state *s = (struct httpd_state *)&(uip_conn->appstate);//Read the connection state
	if(uip_closed() || uip_aborted() || uip_timedout())//Error handling 
	{
	}
	else if(uip_connected())//Connected 
	{
		PSOCK_INIT(&s->sin, s->inputbuf, sizeof(s->inputbuf) - 1);
		PSOCK_INIT(&s->sout, s->inputbuf, sizeof(s->inputbuf) - 1);
		PT_INIT(&s->outputpt);
		s->state = STATE_WAITING;
		/* timer_set(&s->timer, CLOCK_SECOND * 100); */
		s->timer = 0;
		handle_connection(s);//Handle it
	}
	else if(s!=NULL) 
	{
		if(uip_poll()) 
		{
			++s->timer;
			if(s->timer >= 20)uip_abort(); 
		 	else s->timer = 0; 
		}
		handle_connection(s);
	}
	else uip_abort();//	 
}
/*---------------------------------------------------------------------------*/
/**
 * \brief      Initialize the web server
 *
 *             This function initializes the web server and should be
 *             called at system boot-up.
 */
void
httpd_init(void)
{
  uip_listen(HTONS(80));
}
/*---------------------------------------------------------------------------*/
/** @} */
