#include "product.h"

#if (SD_BUS == SPI_BUS_TYPE)


#include "sdcard.h"
#include "mmc_sd.h"
#include "diskio.h"
//#include "flash.h"
#include<stdlib.h>
//#include "usart.h"		 		   

 
#define SD_CARD		0	//SD card, volume 0
//#define EX_FLASH	1	//external flash, volume 1

#define FLASH_SECTOR_SIZE 	512			  
//For the W25Q64 
//The first 6M bytes are for FatFs, 6M to 6M+500K is for the user, and everything past 6M+500K holds the font library, which takes 1.5M.		 			    
u16	    FLASH_SECTOR_COUNT = 2048*6;	//6M bytes, the default for the W25Q64
#define FLASH_BLOCK_SIZE  	8     		//Each block has 8 sectors

extern U16_T far Test[50];

//Initialise the disk
DSTATUS disk_initialize (
	BYTE drv				/* Physical drive nmuber (0..) */
)
{	
	u8 res = 0;	    
	switch(drv)
	{
		case SD_CARD:	//SD card
			res = SD_Initialize();//SD_Init(); 	
 			break;
//		case EX_FLASH:	//external flash
//			SPI_Flash_Init();
//			if(SPI_FLASH_TYPE == W25Q64)
//				FLASH_SECTOR_COUNT = 2048 * 6;		//W25Q64
//			else
//				FLASH_SECTOR_COUNT = 2048 * 2;		//other
// 			break;
		default:
			res = 1;
			break;
	}	

#if (SD_BUS == SPI_BUS_TYPE)	
	if(res)
		return  STA_NOINIT;
#else
	if(res != SD_OK)
		return  STA_NOINIT;
#endif
	else
		return 0;									//Initialisation succeeded
}

//Get the disk status
DSTATUS disk_status (
	BYTE drv		/* Physical drive nmuber (0..) */
)
{		   
    return 0;
}

 //Read sectors
 //drv: drive number 0~9
 //*buff: start of the receive buffer
 //sector: sector address
 //count: number of sectors to read
DRESULT disk_read (
	BYTE drv,		/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	BYTE count		/* Number of sectors to read (1..255) */
)
{
	u8 res = 1; 
    
	if (!count)
		return RES_PARERR;				//count must not be 0, otherwise a parameter error is returned

	switch(drv)
	{
		case SD_CARD://SD card			
			res = SD_ReadDisk(buff, sector, count);
			break;
		
//		case EX_FLASH://external flash
//			for(; count > 0; count--)
//			{
//				SPI_Flash_Read(buff, sector * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
//				sector++;
//				buff += FLASH_SECTOR_SIZE;
//			}
//			res = 0;
//			break;
			
		default:
			res = 1;
			break;
	}
	
   //Translate the return value from SPI_SD_driver.c into an ff.c return value
//	Test[25] = res;
    if(res == 0x00)
			return RES_OK;	 
    else
			return RES_ERROR;	   
} 

 //Write sectors
 //drv: drive number 0~9
 //*buff: start of the data to send
 //sector: sector address
 //count: number of sectors to write	    
#if _READONLY == 0
DRESULT disk_write (
	BYTE drv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	        /* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	BYTE count			/* Number of sectors to write (1..255) */
)
{
	u8 res = 0;  
	u8 retry = 0X1F;		//Number of retries when a write fails
    if(!count)
		return RES_PARERR;	//count must not be 0, otherwise a parameter error is returned		 	 
	
	switch(drv)
	{
		case SD_CARD://SD card
			while(retry)
			{
				res = SD_WriteDisk((u8*)buff, sector, count);
				if(res == 0)
					break;
				retry--;
			}
			break;
			
//		case EX_FLASH://external flash
//			for(; count > 0; count--)
//			{										    
//				SPI_Flash_Write((u8*)buff, sector * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
//				sector++;
//				buff += FLASH_SECTOR_SIZE;
//			}
//			res = 0;
//			break;
			
		default:
			res = 1;
			break;
	}
	
    //Translate the return value from SPI_SD_driver.c into an ff.c return value
    if(res == 0x00)
		return RES_OK;	 
    else
		return RES_ERROR;		 
}
#endif /* _READONLY */

//Fetch other parameters
 //drv: drive number 0~9
 //ctrl: control code
 //*buff: pointer to the send/receive buffer
DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive nmuber (0..) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{	
	DRESULT res;						  			     
	if(drv == SD_CARD)//SD card
	{
	    switch(ctrl)
	    {
		    case CTRL_SYNC:	    
 		        res = RES_OK;
		        break;	 
		    case GET_SECTOR_SIZE:
		        *(WORD*)buff = 512;
		        res = RES_OK;
		        break;	 
		    case GET_BLOCK_SIZE:
		        *(WORD*)buff = 8;
		        res = RES_OK;
		        break;	 
		    case GET_SECTOR_COUNT:
 		        res = RES_OK;
		        break;
		    default:
		        res = RES_PARERR;
		        break;
	    }
	}
//	else if(drv == EX_FLASH)	//external FLASH  
//	{
//	    switch(ctrl)
//	    {
//		    case CTRL_SYNC:
//				res = RES_OK; 
//		        break;	 
//		    case GET_SECTOR_SIZE:
//		        *(WORD*)buff = FLASH_SECTOR_SIZE;
//		        res = RES_OK;
//		        break;	 
//		    case GET_BLOCK_SIZE:
//		        *(WORD*)buff = FLASH_BLOCK_SIZE;
//		        res = RES_OK;
//		        break;	 
//		    case GET_SECTOR_COUNT:
//		        *(DWORD*)buff = FLASH_SECTOR_COUNT;
//		        res = RES_OK;
//		        break;
//		    default:
//		        res = RES_PARERR;
//		        break;
//	    }
//	}
	else
	{
		res = RES_ERROR;		//Anything else is unsupported
	}
	
    return res;
}

//Get the time
//User defined function to give a current time to fatfs module      */
//31-25: Year(0-127 org.1980), 24-21: Month(1-12), 20-16: Day(1-31) */                                                                                                                                                                                                                                          
//15-11: Hour(0-23), 10-5: Minute(0-59), 4-0: Second(0-29 *2) */                                                                                                                                                                                                                                                
DWORD get_fattime (void)
{				 
	return 0;
}

//Dynamically allocate memory
void *ff_memalloc (UINT size)			
{
	return (void*)malloc(size);
}

//Free memory
void ff_memfree (void* mf)		 
{
	free(mf);
}


#endif
