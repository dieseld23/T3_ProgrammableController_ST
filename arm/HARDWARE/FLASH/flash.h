#ifndef __FLASH_H
#define __FLASH_H
			    
#include "bitmap.h" 


//List of W25X and Q series parts	   
//W25Q80 ID  0XEF13
//W25Q16 ID  0XEF14
//W25Q32 ID  0XEF15
//W25Q32 ID  0XEF16	
#define W25Q80 		0XEF13 	
#define W25Q16 		0XEF14
#define W25Q32 		0XEF15
#define W25Q64 		0XEF16
#define AT45D161D	0X1F26

extern u16 SPI_FLASH_TYPE;		//The flash part we use		   
#define	SPI_FLASH_CS PCout(4)  //Select the FLASH	
				 
////////////////////////////////////////////////////////////////////////////
 
//Command table
#define W25X_WriteEnable		0x06 
#define W25X_WriteDisable		0x04 
#define W25X_ReadStatusReg		0x05 
#define W25X_WriteStatusReg		0x01 
#define W25X_ReadData			0x03 
#define W25X_FastReadData		0x0B 
#define W25X_FastReadDual		0x3B 
#define W25X_PageProgram		0x02 
#define W25X_BlockErase			0xD8 
#define W25X_SectorErase		0x20 
#define W25X_ChipErase			0xC7 
#define W25X_PowerDown			0xB9 
#define W25X_ReleasePowerDown	0xAB 
#define W25X_DeviceID			0xAB 
#define W25X_ManufactDeviceID	0x90 
#define W25X_JedecDeviceID		0x9F 

void SPI_Flash_Init(void);
u16  SPI_Flash_ReadID(void);  	    //Read the FLASH ID
u8	 SPI_Flash_ReadSR(void);        //Read the status register 
void SPI_FLASH_Write_SR(u8 sr);  	//Write the status register
void SPI_FLASH_Write_Enable(void);  //Write enable 
void SPI_FLASH_Write_Disable(void);	//Write protect
void SPI_Flash_Write_NoCheck(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite);
void SPI_Flash_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead);   //Read flash
void SPI_Flash_Write(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite);//Write flash
void SPI_Flash_Erase_Chip(void);    	  //Chip erase
void SPI_Flash_Erase_Sector(u32 Dst_Addr);//Sector erase
void SPI_Flash_Wait_Busy(void);           //Wait until idle
void SPI_Flash_PowerDown(void);           //Enter power-down mode
void SPI_Flash_WAKEUP(void);			  //Wake up
#endif
















