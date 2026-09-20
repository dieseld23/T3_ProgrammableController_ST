#ifndef __SPI_H
#define __SPI_H

#include "stm32f10x.h"

//这部分应根据具体的连线来修改!
//Mini STM32使用的是PB12作为SD卡的CS脚.
#define	SD_CS_BIG	PCout(5) //SD卡片选引脚					    	  
#define	SD_CS_SMALL	PDout(3)
#define	SD_CS_NEW_TINY	PDout(9)
#define SD_CS_TSTAT10   PGout(12)


void SPI1_Init(u8 type);							//Initialise the SPI1 port
void SPI1_SetSpeed(u8 SPI_BaudRatePrescaler);	//Set the SPI1 speed   
u8 SPI1_ReadWriteByte(u8 TxData);				//Read and write one byte on the SPI1 bus

void SPI2_Init(void);							//Initialise the SPI1 port
void SPI2_SetSpeed(u8 SPI_BaudRatePrescaler);	//Set the SPI1 speed   
u8 SPI2_ReadWriteByte(u8 TxData);				//Read and write one byte on the SPI1 bus
void SPI_Select_SD(void);
void SPI_Select_TOP(void);

void SPI3_Init(void);							//Initialise the SPI1 port
u8 SPI3_ReadWriteByte(u8 TxData);
void SPI3_SetSpeed(u8 SPI_BaudRatePrescaler);



#endif
