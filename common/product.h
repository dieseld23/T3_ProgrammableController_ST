#ifndef PRODUCT_H
#define PRODUCT_H

#include "types.h"


#define SW_REV	6808

#define ARM_MINI 0
#define ARM_CM5  0
#define ARM_TSTAT_WIFI 1

#define MSTP_UPDATE 0//1

#define ASIX_MINI 0
#define ASIX_CM5  0


#define HANDLE_REBOOT_FLAG 0  // building with this set to 1 produces a special version for the customer that disables the prg at power-up, to get round a badly written program that reboots continually;
                              // normally leave this at 0;
#define SAVE_LOCAL_VAR_TO_E2 0 // Only for PLC

extern U8_T cpu_type;

#define BIG_MAX_AIS 32
#define BIG_MAX_DOS 12
#define BIG_MAX_AOS 12
#define BIG_MAX_AVS 128
#define BIG_MAX_DIS 0
#define BIG_MAX_SCS 8

#define SMALL_MAX_AIS 16
#define SMALL_MAX_DOS 6
#define SMALL_MAX_AOS 4
#define SMALL_MAX_AVS 128
#define SMALL_MAX_DIS 0
#define SMALL_MAX_SCS 8

#define TINY_MAX_AIS 11
#define TINY_MAX_DOS 6
#define TINY_MAX_AOS 2
#define TINY_MAX_AVS 128
#define TINY_MAX_DIS 0
#define TINY_MAX_SCS 8

#define NEW_TINY_MAX_AIS 8
#define NEW_TINY_MAX_DOS 8
#define NEW_TINY_MAX_AOS 6
#define NEW_TINY_MAX_AVS 128
#define NEW_TINY_MAX_DIS 0
#define NEW_TINY_MAX_SCS 8

#define TINY_11I_MAX_AIS 11
#define TINY_11I_MAX_DOS 6
#define TINY_11I_MAX_AOS 5
#define TINY_11I_MAX_AVS 128
#define TINY_11I_MAX_DIS 0
#define TINY_11I_MAX_SCS 8

#define VAV_MAX_AIS 6
#define VAV_MAX_DOS 1
#define VAV_MAX_AOS 2
#define VAV_MAX_AVS 5
#define VAV_MAX_DIS 0
#define VAV_MAX_SCS 8

#define CM5_MAX_AIS 24
#define CM5_MAX_DOS 10
#define CM5_MAX_AOS 0
#define CM5_MAX_AVS 128
#define CM5_MAX_DIS 8
#define CM5_MAX_SCS 8


#define TSTAT10_MAX_AIS 13
#define TSTAT10_MAX_DOS 5
#define TSTAT10_MAX_AOS 4
#define TSTAT10_MAX_AVS 128
#define TSTAT10_MAX_DIS 8
#define TSTAT10_MAX_SCS 8

#define T10P_MAX_AIS 17
#define T10P_MAX_DOS 5
#define T10P_MAX_AOS 4
#define T10P_MAX_AVS 128
#define T10P_MAX_DIS 8
#define T10P_MAX_SCS 8

#define T3OEM_12I_MAX_AIS 21
#define T3OEM_12I_MAX_DOS 7
#define T3OEM_12I_MAX_AOS 4
#define T3OEM_12I_MAX_AVS 128
#define T3OEM_12I_MAX_DIS 8
#define T3OEM_12I_MAX_SCS 8


#define MINI_CM5  			0
#define MINI_BIG	 			1
#define MINI_SMALL  		2
#define MINI_TINY	 			3			// ASIX CORE
#define MINI_NEW_TINY	 	4  // ARM CORE
#define MINI_BIG_ARM	 	5
#define MINI_SMALL_ARM  6
#define MINI_TINY_ARM		7
#define MINI_NANO    		8
#define MINI_TSTAT10 		9
#define MINI_T10P	 			11
#define MINI_VAV	 			10   // no used
#define MINI_TINY_11I		12
#define MINI_T3OEM_12I	14

#define MAX_MINI_TYPE 	14


#define STM_TINY_REV 7



#define Broadcast_Addr 0xffffffff


#if (ASIX_MINI || ASIX_CM5)

//#define TEST_RST_PIN 1



#define DEBUG_UART1 0
//#define CHAMBER
#if (DEBUG_UART1)

#include <stdio.h>

#define UART_SUB1 0

void uart_init_send_com(unsigned char port);
extern unsigned char far debug_str[200];
void uart_send_string(unsigned char *p, unsigned int length,unsigned char port);

#endif

#define MSTP 0  // < 1k
#define ALARM_SYNC  0 // ~ 2k
#define TIME_SYNC 0 // >1k
#define REM_CONNECTION 0

#define OUTPUT_DEATMASTER  1
#define INCLUDE_DNS_CLIENT  1  // 3k
#define INCLUDE_DHCP_CLIENT 1

#endif



#if (ARM_MINI || ARM_CM5 || ARM_TSTAT_WIFI)

#define OUTPUT_DEATMASTER  1

#define SD_BUS_TYPE 0
#define SPI_BUS_TYPE 1

#define REBOOT_PER_WEEK 1
#define SD_BUS SPI_BUS_TYPE

#define far
#define code
#define bit U8_T
#define data

#define sTaskCreate xTaskCreate
#define cQueueSend xQueueSend
#define cQueueReceive xQueueReceive
#define vSemaphoreCreateBinary xQueueCreateMutex
//#define xSemaphoreHandle QueueHandle_t
#define cSemaphoreTake xQueueTakeMutexRecursive
#define cSemaphoreGive xQueueGiveMutexRecursive


#define MSTP 1  // < 1k
#define ALARM_SYNC  1 // ~ 2k
#define TIME_SYNC 1 // >1k

#endif



#if (ARM_MINI || ASIX_MINI )

#define T3_MAP  1
#define STORE_TO_SD  1  // > 20k
#define USER_SYNC 0//1

#define ADJUST_NG2_AO 0
#define COV   1
#define SMTP  1
#define NETWORK_MODBUS_BAC 	1
//#define ETHERNET_DEBUG  0  //network port debug output, printed to 192.168.0.38 port 1115;
#define ARM_UART_DEBUG 0
#define DEBUG_EN  UART0_TXEN_BIG 
//#define DEBUG_EN  UART0_TXEN_TINY  //use this because the Tiny has a different pinout

#define PING  0


#endif

#if ARM_CM5



#define T3_MAP  1
#define STORE_TO_SD  1  // > 20k
#define USER_SYNC 0//1

#define MSTP 1
#define PING  0

#define ARM_UART_DEBUG 0
#define DEBUG_EN  UART0_TXEN

#endif


#if ASIX_CM5


#define MSTP 0

#define TIME_SYNC 0

#define T3_MAP  0
#define STORE_TO_SD  0
#define ALARM_SYNC  0




#define PING  0


#endif


#if ARM_TSTAT_WIFI


#define SD_BUS_TYPE 0
#define SPI_BUS_TYPE 1


#define SD_BUS SPI_BUS_TYPE


#define STORE_TO_SD  1 

#define NETWORK_MODBUS_BAC 0

#define ARM_UART_DEBUG 0


#define DEBUG_EN  UART0_TXEN_BIG


#endif








#endif

