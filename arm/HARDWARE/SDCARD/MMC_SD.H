#ifndef _MMC_SD_H_
#define _MMC_SD_H_

#include "bitmap.h"	 
 								    	 
// SD card type definitions  
#define SD_TYPE_ERR     0X00
#define SD_TYPE_MMC     0X01
#define SD_TYPE_V1      0X02
#define SD_TYPE_V2      0X04
#define SD_TYPE_V2HC    0X06	   
// SD card command table  	   
#define CMD0    0       //Reset the card
#define CMD1    1
#define CMD8    8       //CMD8, SEND_IF_COND
#define CMD9    9       //CMD9, read the CSD
#define CMD10   10      //CMD10, read the CID
#define CMD12   12      //CMD12, stop the data transfer
#define CMD16   16      //CMD16, set the sector size; should return 0x00
#define CMD17   17      //CMD17, read a sector
#define CMD18   18      //CMD18, read multiple sectors
#define CMD23   23      //CMD23, pre-erase N blocks before a multi-sector write
#define CMD24   24      //CMD24, write a sector
#define CMD25   25      //CMD25, write multiple sectors
#define CMD41   41      //CMD41, should return 0x00
#define CMD55   55      //CMD55, should return 0x01
#define CMD58   58      //CMD58, read the OCR
#define CMD59   59      //CMD59, enable or disable CRC; should return 0x00
//Meaning of the data write response token
#define MSD_DATA_OK                0x05
#define MSD_DATA_CRC_ERROR         0x0B
#define MSD_DATA_WRITE_ERROR       0x0D
#define MSD_DATA_OTHER_ERROR       0xFF
//SD card response tokens
#define MSD_RESPONSE_NO_ERROR      0x00
#define MSD_IN_IDLE_STATE          0x01
#define MSD_ERASE_RESET            0x02
#define MSD_ILLEGAL_COMMAND        0x04
#define MSD_COM_CRC_ERROR          0x08
#define MSD_ERASE_SEQUENCE_ERROR   0x10
#define MSD_ADDRESS_ERROR          0x20
#define MSD_PARAMETER_ERROR        0x40
#define MSD_RESPONSE_FAILURE       0xFF
 							   						 	 



extern u8 SD_Type;//SD card type
//Function declarations 
u8 SD_SPI_ReadWriteByte(u8 data);
void SD_SPI_SpeedLow(void);
void SD_SPI_SpeedHigh(void);
u8 SD_WaitReady(void);							//Wait for the SD card to be ready
u8 SD_GetResponse(u8 Response);					//Get the response
u8 SD_Initialize(void);							//Initialise
u8 SD_ReadDisk(u8*buf, u32 sector, u8 cnt);		//Read a block
u8 SD_WriteDisk(u8*buf, u32 sector, u8 cnt);	//Write a block
u32 SD_GetSectorCount(void);   					//Read the sector count
u8 SD_GetCID(u8 *cid_data);                     //Read the SD card CID
u8 SD_GetCSD(u8 *csd_data);                     //Read the SD card CSD
 
#endif




