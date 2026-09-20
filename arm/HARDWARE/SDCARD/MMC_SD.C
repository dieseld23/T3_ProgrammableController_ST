#include "main.h"
#include "mmc_sd.h"			   
#include "spi.h"
#include "usart.h"
#include <stdio.h>

u8 SD_Type = 0;	//SD card type 

#if (SD_BUS == SPI_BUS_TYPE)
					   					   

////////////////////////////////////porting section///////////////////////////////////
//The interface to change when porting
//data: the data to write
//Return: the data read
u8 SD_SPI_ReadWriteByte(u8 dat)
{
	if((Modbus.mini_type == MINI_BIG) || (Modbus.mini_type == MINI_BIG_ARM) 
		|| (Modbus.mini_type == MINI_NEW_TINY) || (Modbus.mini_type == MINI_TINY_ARM) || (Modbus.mini_type == MINI_TINY_11I) || (Modbus.mini_type == MINI_NANO)) // PB3 PB4 PB5	
		return SPI3_ReadWriteByte(dat);
	else if((Modbus.mini_type == MINI_SMALL) || (Modbus.mini_type == MINI_SMALL_ARM) || (Modbus.mini_type == MINI_TSTAT10)|| (Modbus.mini_type == MINI_T10P))
		return SPI1_ReadWriteByte(dat);
}

//The SD card has to be initialised at low speed
void SD_SPI_SpeedLow(void)
{
	if((Modbus.mini_type == MINI_BIG) || (Modbus.mini_type == MINI_BIG_ARM) 
		|| (Modbus.mini_type == MINI_NEW_TINY) || (Modbus.mini_type == MINI_TINY_ARM) || (Modbus.mini_type == MINI_TINY_11I) || (Modbus.mini_type == MINI_NANO)) // PB3 PB4 PB5	
		SPI3_SetSpeed(SPI_BaudRatePrescaler_256);//Switch to low-speed mode	
	else if((Modbus.mini_type == MINI_SMALL) || (Modbus.mini_type == MINI_SMALL_ARM) || (Modbus.mini_type == MINI_TSTAT10) || (Modbus.mini_type == MINI_T10P))
		SPI1_SetSpeed(SPI_BaudRatePrescaler_256);//Switch to low-speed mode
}

//Once the SD card is running normally it can go fast
void SD_SPI_SpeedHigh(void)
{
	if((Modbus.mini_type == MINI_BIG) || (Modbus.mini_type == MINI_BIG_ARM) 
		|| (Modbus.mini_type == MINI_NEW_TINY) || (Modbus.mini_type == MINI_TINY_ARM) || (Modbus.mini_type == MINI_TINY_11I) || (Modbus.mini_type == MINI_NANO)) // PB3 PB4 PB5	
		SPI3_SetSpeed(SPI_BaudRatePrescaler_16);//Switch to high-speed mode	
	else if((Modbus.mini_type == MINI_SMALL) || (Modbus.mini_type == MINI_SMALL_ARM) || (Modbus.mini_type == MINI_TSTAT10) || (Modbus.mini_type == MINI_T10P))
		SPI1_SetSpeed(SPI_BaudRatePrescaler_16);//Switch to high-speed mode	
}

////SPI hardware layer initialisation
//void SD_SPI_Init(void)
//{
//  GPIO_InitTypeDef GPIO_InitStructure;

// 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	 //enable the PB port clock

//	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;				 //PB12 push-pull 
// 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //push-pull output
// 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
// 	GPIO_Init(GPIOB, &GPIO_InitStructure);
// 	GPIO_SetBits(GPIOA,GPIO_Pin_4);						 //PB12 pull-up
//	
//	SD_CS = 1;
//	
//}

////////////////////////////////////////////////////////////////////////////
//Deselect and release the SPI bus
void SD_DisSelect(void)
{
	if((Modbus.mini_type == MINI_BIG) || (Modbus.mini_type == MINI_BIG_ARM)) // PB3 PB4 PB5	
		SD_CS_BIG = 1;
	else if((Modbus.mini_type == MINI_NEW_TINY) || (Modbus.mini_type == MINI_TINY_ARM) || (Modbus.mini_type == MINI_TINY_11I) || (Modbus.mini_type == MINI_NANO))
		SD_CS_NEW_TINY = 1;
	else if((Modbus.mini_type == MINI_SMALL) || (Modbus.mini_type == MINI_SMALL_ARM))
		SD_CS_SMALL = 1;
	else if((Modbus.mini_type == MINI_TSTAT10) || (Modbus.mini_type == MINI_T10P))
		SD_CS_TSTAT10 = 1;

 	SD_SPI_ReadWriteByte(0xff);//Give it another 8 clocks
}

//Select the SD card and wait for it to be ready
//Return: 0 = success; 1 = failure;
u8 SD_Select(void)
{
	if((Modbus.mini_type == MINI_BIG) || (Modbus.mini_type == MINI_BIG_ARM)) // PB3 PB4 PB5	
		SD_CS_BIG = 0;
	else if((Modbus.mini_type == MINI_NEW_TINY) || (Modbus.mini_type == MINI_TINY_ARM) || (Modbus.mini_type == MINI_TINY_11I) || (Modbus.mini_type == MINI_NANO))
		SD_CS_NEW_TINY = 0;
	else if((Modbus.mini_type == MINI_SMALL) || (Modbus.mini_type == MINI_SMALL_ARM))
		SD_CS_SMALL = 0;	
	else if((Modbus.mini_type == MINI_TSTAT10) || (Modbus.mini_type == MINI_T10P))
		SD_CS_TSTAT10 = 0;
//#if ARM_UART_DEBUG
//	uart1_init(115200);
//	DEBUG_EN = 1;
//	printf("wait sd\r\n");
//#endif
	if(SD_WaitReady() == 0)
		return 0;//Wait succeeded
//#if ARM_UART_DEBUG
//	uart1_init(115200);
//	DEBUG_EN = 1;
//	printf("wait sd fail\r\n");
//#endif
	SD_DisSelect();
	return 1;//Wait failed
}

//Wait for the card to be ready
//Return: 0 = ready; other = error code
u8 SD_WaitReady(void)
{
	u32 t = 0;
	do
	{
		if(SD_SPI_ReadWriteByte(0XFF) == 0XFF)
			return 0;//OK
		t++;	
		//add watchdog
		IWDG_ReloadCounter(); 
	}while(t < 0X1FFF/*FF*/);//Wait 
	return 1;
}

//Wait for the SD card to respond
//Response: the response value expected
//Return: 0 = that response was received
//    other = the response was not received
u8 SD_GetResponse(u8 Response)
{
	u16 Count = 0xFFFF;//Number of attempts	   						  
	while ((SD_SPI_ReadWriteByte(0XFF) != Response) && Count)Count--;//Wait for exactly the right response 
	
	if(Count == 0)
		return MSD_RESPONSE_FAILURE;	//No response   
	else 
		return MSD_RESPONSE_NO_ERROR;	//Correct response
}

//Read one data packet from the SD card
//buf: data buffer
//len: number of bytes to read.
//Return: 0 = success; other = failure;	
u8 SD_RecvData(u8*buf, u16 len)
{			  	  
	if(SD_GetResponse(0xFE))return 1;//Wait for the SD card to send the 0xFE data start token
	
    while(len--)//Start receiving data
    {
        *buf = SD_SPI_ReadWriteByte(0xFF);//SPI1_ReadWriteByte(0xFF);
        buf++;
    }
    //Two dummy CRC bytes follow
    SD_SPI_ReadWriteByte(0xFF);
    SD_SPI_ReadWriteByte(0xFF);									  					    
    return 0;//Read succeeded
}

//Write one 512-byte data packet to the SD card
//buf: data buffer
//cmd: the command
//Return: 0 = success; other = failure;	
u8 SD_SendBlock(u8*buf, u8 cmd)
{	
	u16 t;		  	  
	if(SD_WaitReady())return 1;//Wait for ready to drop
	SD_SPI_ReadWriteByte(cmd);
	if(cmd != 0XFD)//Not the stop command
	{
		for(t = 0; t < 512; t++)  
			SD_SPI_ReadWriteByte(buf[t]);//SPI1_ReadWriteByte(buf[t]);//faster, with less time spent passing arguments
	    SD_SPI_ReadWriteByte(0xFF);//Ignore the CRC
	    SD_SPI_ReadWriteByte(0xFF);
		t=SD_SPI_ReadWriteByte(0xFF);//Receive the response
		if((t & 0x1F) != 0x05)return 2;//Response error									  					    
	}						 									  					    
    return 0;//Write succeeded
}

//Send a command to the SD card
//Input: u8 cmd   the command 
//      u32 arg  the command argument
//      u8 crc   the CRC value	   
//Return: the response from the SD card		
extern U16_T far Test[50];
u8 SD_SendCmd(u8 cmd, u32 arg, u8 crc)
{
    u8 r1;	
	u8 Retry = 0; 
	SD_DisSelect();//Drop the previous chip select
	if(SD_Select())	return 0XFF;//Chip select inactive 
	//Send
//#if ARM_UART_DEBUG
//	uart1_init(115200);
//	DEBUG_EN = 1;
//	printf("sd select ok\r\n");
//#endif
	SD_SPI_ReadWriteByte(cmd | 0x40);//Write the command bytes one by one
	SD_SPI_ReadWriteByte(arg >> 24);
	SD_SPI_ReadWriteByte(arg >> 16);
	SD_SPI_ReadWriteByte(arg >> 8);
	SD_SPI_ReadWriteByte(arg);	  
	SD_SPI_ReadWriteByte(crc); 
	
	if(cmd == CMD12)
		SD_SPI_ReadWriteByte(0xff);//Skip a stuff byte when stop reading
    //Wait for a response, or give up on timeout
	Retry = 0X1F;
	do
	{
		r1 = SD_SPI_ReadWriteByte(0xFF);
	}while((r1 & 0X80) && Retry--);	 
	//Return the status
    return r1;
}

//Read the SD card CID, which includes the manufacturer details
//Input: u8 *cid_data (memory for the CID, at least 16 bytes)	  
//Return: 0 = NO_ERR
//		 1 = error														   
u8 SD_GetCID(u8 *cid_data)
{
    u8 r1;	   
    //Send CMD10 to read the CID
    r1 = SD_SendCmd(CMD10, 0, 0x01);
    if(r1 == 0x00)
	{
		r1 = SD_RecvData(cid_data, 16);//Receive 16 bytes of data	 
    }
	SD_DisSelect();//Deselect the chip
	if(r1)
		return 1;
	else 
		return 0;
}

//Read the SD card CSD, which includes the capacity and speed
//Input: u8 *cid_data (memory for the CID, at least 16 bytes)	    
//Return: 0 = NO_ERR
//		 1 = error														   
u8 SD_GetCSD(u8 *csd_data)
{
    u8 r1;	 
    r1 = SD_SendCmd(CMD9, 0, 0x01);//Send CMD9 to read the CSD
    if(r1 == 0)
	{
    	r1 = SD_RecvData(csd_data, 16);//Receive 16 bytes of data 
    }
	SD_DisSelect();//Deselect the chip
	if(r1)
		return 1;
	else 
		return 0;
}

//Get the total number of sectors on the SD card   
//Return: 0 = could not read the capacity 
//       other = the card capacity in 512-byte sectors
//The sector size must be 512; anything else fails initialisation.														  
u32 SD_GetSectorCount(void)
{
    u8 csd[16];
    u32 Capacity;  
    u8 n;
	u16 csize;  					    
	//Read the CSD, returning 0 on any error
    if(SD_GetCSD(csd) != 0) return 0;	    
    //For an SDHC card, work it out as below
    if((csd[0] & 0xC0) == 0x40)	 //A V2.00 card
    {	
		csize = csd[9] + ((u16)csd[8] << 8) + 1;
		Capacity = (u32)csize << 10;//Get the sector count	 		   
    }
	else//A V1.XX card
    {	
		n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
		csize = (csd[8] >> 6) + ((u16)csd[7] << 2) + ((u16)(csd[6] & 3) << 10) + 1;
		Capacity= (u32)csize << (n - 9);//Get the sector count   
    }
    return Capacity;
}

//Initialise the SD card
extern u16 Test[50];
u8 SD_Initialize(void)
{
	u8 r1;      // holds the value returned by the SD card
	u16 retry;  // used to count towards the timeout
	u8 buf[4];  
	u16 i;
	if((Modbus.mini_type == MINI_BIG) || (Modbus.mini_type == MINI_BIG_ARM) 
		|| (Modbus.mini_type == MINI_NEW_TINY) || (Modbus.mini_type == MINI_TINY_ARM)  || (Modbus.mini_type == MINI_TINY_11I)
		|| (Modbus.mini_type == MINI_NANO)) // PB3 PB4 PB5	
		SPI3_Init();
	else if((Modbus.mini_type == MINI_SMALL) || (Modbus.mini_type == MINI_SMALL_ARM) || (Modbus.mini_type == MINI_TSTAT10) || (Modbus.mini_type == MINI_T10P))
	{
		SPI1_Init(1);
	}
	
#if (ASIX_MINI || ASIX_CM5)
	SD_SPI_Init();		//Initialise the IO
#endif
	
	
 	SD_SPI_SpeedLow();	//Switch to low-speed mode 

	
 	for(i = 0; i < 10; i++)	
		SD_SPI_ReadWriteByte(0XFF);//Send at least 74 clock pulses
	
	retry = 2;
	do
	{
		r1 = SD_SendCmd(CMD0, 0, 0x95);//Enter the IDLE state
		
	}while((r1 != 0X01) && retry--);

 	SD_Type = 0;//No card by default
	if(r1 == 0X01)
	{
		if(SD_SendCmd(CMD8, 0x1AA, 0x87) == 1)//SD V2.0
		{
			for(i = 0; i < 4; i++)buf[i] = SD_SPI_ReadWriteByte(0XFF);	//Get trailing return value of R7 resp
			if(buf[2] == 0X01 && buf[3] == 0XAA)//Whether the card supports 2.7~3.6V
			{
				retry = 0XFFFE;
				do
				{
					SD_SendCmd(CMD55, 0, 0X01);	//Send CMD55
					r1=SD_SendCmd(CMD41, 0x40000000, 0X01);//Send CMD41
				}while(r1 && retry--);
				
				if(retry && SD_SendCmd(CMD58, 0, 0X01) == 0)//Start identifying the SD 2.0 card version
				{
					for(i = 0; i < 4; i++)buf[i] = SD_SPI_ReadWriteByte(0XFF);//Read the OCR
					if(buf[0] & 0x40)
						SD_Type = SD_TYPE_V2HC;    //Check CCS
					else 
						SD_Type = SD_TYPE_V2;   
				}
			}
		}
		else//SD V1.x/ MMC	V3
		{
			SD_SendCmd(CMD55, 0, 0X01);		//Send CMD55
			r1 = SD_SendCmd(CMD41, 0, 0X01);//Send CMD41
			if(r1 <= 1)
			{		
				SD_Type = SD_TYPE_V1;
				retry = 0XFFFE;
				do //Wait for the card to leave IDLE mode
				{
					SD_SendCmd(CMD55, 0, 0X01);		//Send CMD55
					r1 = SD_SendCmd(CMD41, 0, 0X01);//Send CMD41
				}while(r1 && retry--);
			}
			else
			{
				SD_Type = SD_TYPE_MMC;//MMC V3
				retry = 0XFFFE;
				do //Wait for the card to leave IDLE mode
				{											    
					r1 = SD_SendCmd(CMD1, 0, 0X01);//Send CMD1
				}while(r1 && retry--);  
			}
			if(retry == 0 || SD_SendCmd(CMD16, 512, 0X01) != 0)
				SD_Type = SD_TYPE_ERR;//Wrong card
		}
	}
	SD_DisSelect();//Deselect the chip
	SD_SPI_SpeedHigh();//High speed
//#if (ASIX_MINI || ASIX_CM5)_UART_DEBUG
//	uart1_init(115200);
//	DEBUG_EN = 1;
//	printf("sd type = %d\r\n",SD_Type);
//#endif
	if(SD_Type)
	{
		return 0;
	}
	else if(r1)
	{
		return r1;
	}
	return 0xaa;//Other error
}

//Read the SD card
//buf: data buffer
//sector: the sector
//cnt: sector count
//Return: 0 = ok; other = failure.
u8 SD_ReadDisk(u8*buf, u32 sector, u8 cnt)
{
	u8 r1;
	if(SD_Type != SD_TYPE_V2HC)sector <<= 9;//Convert to a byte address
	if(cnt == 1)
	{
		r1 = SD_SendCmd(CMD17, sector, 0X01);//Read command
		if(r1 == 0)//Command sent successfully
		{
			r1 = SD_RecvData(buf, 512);//Receive 512 bytes	   
		}
	}
	else
	{
		r1 = SD_SendCmd(CMD18, sector, 0X01);//Multiple-block read command
		do
		{
			r1 = SD_RecvData(buf, 512);//Receive 512 bytes	 
			buf += 512;  
		}while(--cnt && r1 == 0);
		
		SD_SendCmd(CMD12, 0, 0X01);	//Send the stop command
	}   
	SD_DisSelect();//Deselect the chip
	return r1;//
}

//Write the SD card
//buf: data buffer
//sector: the first sector
//cnt: sector count
//Return: 0 = ok; other = failure.
u8 SD_WriteDisk(u8*buf, u32 sector, u8 cnt)
{
	u8 r1;
	if(SD_Type != SD_TYPE_V2HC)sector *= 512;//Convert to a byte address
	if(cnt == 1)
	{
		r1 = SD_SendCmd(CMD24, sector, 0X01);//Read command
		if(r1 == 0)//Command sent successfully
		{
			r1 = SD_SendBlock(buf, 0xFE);//Write 512 bytes	   
		}
	}else
	{
		if(SD_Type != SD_TYPE_MMC)
		{
			SD_SendCmd(CMD55, 0, 0X01);	
			SD_SendCmd(CMD23, cnt, 0X01);//Send the command	
		}
 		r1 = SD_SendCmd(CMD25, sector, 0X01);//Multiple-block read command
		if(r1 == 0)
		{
			do
			{
				r1 = SD_SendBlock(buf, 0xFC);//Receive 512 bytes	 
				buf += 512;  
			}while(--cnt && r1 == 0);
			
			r1 = SD_SendBlock(0, 0xFD);//Receive 512 bytes 
		}
	}   
	SD_DisSelect();//Deselect the chip
	return r1;//
}

#endif