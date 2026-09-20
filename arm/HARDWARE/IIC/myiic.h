#ifndef __MYIIC_H
#define __MYIIC_H

#include "bitmap.h"
#include "product.h"   
////IO direction control
//static void SDA_IN(void)
//{
//	GPIO_InitTypeDef GPIO_InitStructure; 
//	GPIO_InitStructure.GPIO_Pin = gpio_map[EEP_SDA].GPIO_Pin_X;
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
//	GPIO_Init(gpio_map[EEP_SDA].GPIOX, &GPIO_InitStructure);
//	GPIO_SetBits(gpio_map[EEP_SDA].GPIOX, gpio_map[EEP_SDA].GPIO_Pin_X);
//}
//static void SDA_OUT(void)
//{
//	GPIO_InitTypeDef GPIO_InitStructure; 
//	GPIO_InitStructure.GPIO_Pin = gpio_map[EEP_SDA].GPIO_Pin_X;
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(gpio_map[EEP_SDA].GPIOX, &GPIO_InitStructure);
//	GPIO_SetBits(gpio_map[EEP_SDA].GPIOX, gpio_map[EEP_SDA].GPIO_Pin_X);
//}

//IO direction control

#if (ARM_MINI || ARM_CM5)
//#define SDA_IN()	{GPIOA->CRH &= 0XFFFFFFF0; GPIOA->CRH |= ((u32)8 << 0);}  // A8
//#define SDA_OUT()	{GPIOA->CRH &= 0XFFFFFFF0; GPIOA->CRH |= ((u32)3 << 0);}

//IO operation macros	 
//#define IIC_SCL		PAout(15)	//SCL
//#define IIC_SDA		PAout(8)	//SDA	 
//#define READ_SDA	PAin(8)		//SDA as input 
//#define IIC_WP		PCout(5) // big
//#define IIC_WP_TINY		PDout(10) // tiny
#endif

#if ARM_TSTAT_WIFI
#define SDA_IN()	{GPIOA->CRL &= 0XFFFFF0FF; GPIOA->CRL |= ((u32)8 << 8);}  // A2
#define SDA_OUT()	{GPIOA->CRL &= 0XFFFFF0FF; GPIOA->CRL |= ((u32)3 << 8);}

#define SCL_IN()	{GPIOA->CRL &= 0XFFFF0FFF; GPIOA->CRL |= ((u32)8 << 12);}  // A3
#define SCL_OUT()	{GPIOA->CRL &= 0XFFFF0FFF; GPIOA->CRL |= ((u32)3 << 12);}

//IO operation macros	 
//#define IIC_SCL		PAout(3)	//SCL
//#define IIC_SDA		PAout(2)	//SDA	 
//#define READ_SDA	PAin(2)		//SDA as input 
//#define READ_SCL	PAin(3)		// SCL input
//#define IIC_WP		PCout(5) // big

void IIC_SCL(u8 status);
void IIC_SDA(u8 status);
u8 READ_SDA();

#endif


//All the IIC operations
void IIC_Init(void);				//Initialise the IIC IO pins				 
void IIC_Start(void);				//Send an IIC START
void IIC_Stop(void);				//Send an IIC STOP
void IIC_Send_Byte(u8 txd);			//Send one byte over IIC
u8 IIC_Read_Byte(unsigned char ack);//Read one byte over IIC
u8 IIC_Wait_Ack(void);				//Wait for an IIC ACK
u8 IIC_Wait_Ack1(void);
void IIC_Ack(void);					//Send an IIC ACK
void IIC_NAck(void);				//Send an IIC NACK

void IIC_Write_One_Byte(u8 daddr,u8 addr,u8 data);
u8 IIC_Read_One_Byte(u8 daddr,u8 addr);
  
#endif
