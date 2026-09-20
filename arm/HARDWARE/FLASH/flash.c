#include "flash.h" 
#include "spi.h"
#include "delay.h"	   
#include "usart.h"	   

u16 SPI_FLASH_TYPE = AT45D161D;

//1Page=512Bytes
//1Block=8Pages
//1Sector=32Blocks
//1Chip=16Sectors
//AT45D161D
//2M bytes of capacity
													 
//Initialise the SPI FLASH IO pins
void SPI_Flash_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);	//Enable the PORTC clock

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;			//PC4-CS
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  	//PC4 push-pull output 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_Init(GPIOC, &GPIO_InitStructure);				//Initialise GPIOA
	GPIO_SetBits(GPIOC, GPIO_Pin_4);  					//Drive PB7 high

//	RCC->APB2ENR |= (1<<2) | (1<<4);//enable the PORTA and PORTC clocks 	    
//	GPIOC->CRL &= 0XFFF0FFFF; 
//	GPIOC->CRL |= 0X00030000;
//	SPI_FLASH_CS = 1;	    
	
//	GPIOA->CRL &= 0X000FFFFF;
//	GPIOA->CRL |= 0XBBB00000;

	SPI_FLASH_TYPE = SPI_Flash_ReadID();//Read the FLASH ID.
}  

//Read the SPI_FLASH status register
//BIT7  6   5   4   3   2   1   0
//SPR   RV  TB BP2 BP1 BP0 WEL BUSY
//SPR: 0 by default; status register protect bit, used together with WP
//TB,BP2,BP1,BP0: FLASH region write protection settings
//WEL: write enable latch
//BUSY: busy flag (1 = busy; 0 = idle)
//Default: 0x00
u8 SPI_Flash_ReadSR(void)   
{  
	u8 byte=0;   
	SPI_FLASH_CS=0;                            //Enable the device   
	SPI1_ReadWriteByte(W25X_ReadStatusReg);    //Send the read status register command    
	byte=SPI1_ReadWriteByte(0Xff);             //Read one byte  
	SPI_FLASH_CS=1;                            //Deselect the chip     
	return byte;   
} 
//Write the SPI_FLASH status register
//Only SPR, TB, BP2, BP1 and BP0 (bits 7,5,4,3,2) are writable!!!
void SPI_FLASH_Write_SR(u8 sr)   
{   
	SPI_FLASH_CS=0;                            //Enable the device   
	SPI1_ReadWriteByte(W25X_WriteStatusReg);   //Send the write status register command    
	SPI1_ReadWriteByte(sr);               //Write one byte  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
}   
//SPI_FLASH write enable	
//Sets WEL   
void SPI_FLASH_Write_Enable(void)   
{
	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_WriteEnable);      //Send write enable  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
} 
//SPI_FLASH write disable	
//Clears WEL  
void SPI_FLASH_Write_Disable(void)   
{  
	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_WriteDisable);     //Send the write disable command    
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
}
  	  
u16 SPI_Flash_ReadID(void)
{
	u16 Temp = 0;	  
	SPI_FLASH_CS=0;				    
	SPI1_ReadWriteByte(0x9F);//Send the read ID command	    	     	 			   
	Temp = SPI1_ReadWriteByte(0xFF);  
	Temp = (Temp << 8) | SPI1_ReadWriteByte(0xFF);
	SPI1_ReadWriteByte(0xFF);  
	SPI1_ReadWriteByte(0xFF);	 
	SPI_FLASH_CS=1;				     
	return Temp;
}   		    
//Read the SPI FLASH  
//Read a given number of bytes starting at the given address
//pBuffer: data buffer
//ReadAddr: address to start reading from (24-bit)
//NumByteToRead: number of bytes to read (max 65535)
void SPI_Flash_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)   
{ 
 	u16 i;   										    
	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_ReadData);         //Send the read command   
    SPI1_ReadWriteByte((u8)((ReadAddr)>>16));  //Send the 24-bit address    
    SPI1_ReadWriteByte((u8)((ReadAddr)>>8));   
    SPI1_ReadWriteByte((u8)ReadAddr);   
    for(i=0;i<NumByteToRead;i++)
	{ 
        pBuffer[i]=SPI1_ReadWriteByte(0XFF);   //Read in a loop  
    }
	SPI_FLASH_CS=1;  				    	      
}  
//Write fewer than 256 bytes within one SPI page (0~65535)
//Write at most 256 bytes starting at the given address
//pBuffer: data buffer
//WriteAddr: start address to write (24-bit)
//NumByteToWrite: number of bytes to write (max 256); it must not exceed the bytes left in the page!!!	 
void SPI_Flash_Write_Page(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
 	u16 i;  
    SPI_FLASH_Write_Enable();                  //SET WEL 
	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_PageProgram);      //Send the page program command   
    SPI1_ReadWriteByte((u8)((WriteAddr)>>16)); //Send the 24-bit address    
    SPI1_ReadWriteByte((u8)((WriteAddr)>>8));   
    SPI1_ReadWriteByte((u8)WriteAddr);   
    for(i=0;i<NumByteToWrite;i++)SPI1_ReadWriteByte(pBuffer[i]);//Write in a loop  
	SPI_FLASH_CS=1;                            //Deselect the chip 
	SPI_Flash_Wait_Busy();					   //Wait for the write to finish
} 
//Write the SPI FLASH without checking 
//Every byte in the address range must already be 0xFF, or the write will fail where it is not!
//Handles page crossing automatically 
//Write a given number of bytes at the given address, but the address must stay in range!
//pBuffer: data buffer
//WriteAddr: start address to write (24-bit)
//NumByteToWrite: number of bytes to write (max 65535)
//CHECK OK
void SPI_Flash_Write_NoCheck(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)   
{ 			 		 
	u16 pageremain;	   
	pageremain=256-WriteAddr%256; //Bytes left in the page		 	    
	if(NumByteToWrite<=pageremain)pageremain=NumByteToWrite;//No more than 256 bytes
	while(1)
	{	   
		SPI_Flash_Write_Page(pBuffer,WriteAddr,pageremain);
		if(NumByteToWrite==pageremain)break;//Write finished
	 	else //NumByteToWrite>pageremain
		{
			pBuffer+=pageremain;
			WriteAddr+=pageremain;	

			NumByteToWrite-=pageremain;			  //Subtract the bytes already written
			if(NumByteToWrite>256)pageremain=256; //256 bytes can be written at once
			else pageremain=NumByteToWrite; 	  //Fewer than 256 bytes left
		}
	};	    
} 
//Write the SPI FLASH  
//Write a given number of bytes starting at the given address
//This function erases as well!
//pBuffer: data buffer
//WriteAddr: start address to write (24-bit)						
//NumByteToWrite: number of bytes to write (max 65535)   
u8 SPI_FLASH_BUFFER[4096];		 
void SPI_Flash_Write(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)   
{ 
	u32 secpos;
	u16 secoff;
	u16 secremain;	   
 	u16 i;    
	u8 * SPI_FLASH_BUF;	  
   	SPI_FLASH_BUF=SPI_FLASH_BUFFER;	     
 	secpos=WriteAddr/4096;//Sector address  
	secoff=WriteAddr%4096;//Offset within the sector
	secremain=4096-secoff;//Remaining space in the sector   
 	//printf("ad:%X,nb:%X\r\n",WriteAddr,NumByteToWrite);//for testing
 	if(NumByteToWrite<=secremain)secremain=NumByteToWrite;//No more than 4096 bytes
	while(1) 
	{	
		SPI_Flash_Read(SPI_FLASH_BUF,secpos*4096,4096);//Read the whole sector
		for(i=0;i<secremain;i++)//Verify the data
		{
			if(SPI_FLASH_BUF[secoff+i]!=0XFF)break;//Erase required  	  
		}
		if(i<secremain)//Erase required
		{
			SPI_Flash_Erase_Sector(secpos);//Erase this sector
			for(i=0;i<secremain;i++)	   //Copy
			{
				SPI_FLASH_BUF[i+secoff]=pBuffer[i];	  
			}
			SPI_Flash_Write_NoCheck(SPI_FLASH_BUF,secpos*4096,4096);//Write the whole sector  

		}else SPI_Flash_Write_NoCheck(pBuffer,WriteAddr,secremain);//Already erased, so write straight into the rest of the sector. 				   
		if(NumByteToWrite==secremain)break;//Write finished
		else//Write not finished
		{
			secpos++;//Increment the sector address
			secoff=0;//Offset is 0 	 

		   	pBuffer+=secremain;  //Pointer offset
			WriteAddr+=secremain;//Advance the write address	   
		   	NumByteToWrite-=secremain;				//Decrement the byte count
			if(NumByteToWrite>4096)secremain=4096;	//The next sector still will not hold all of it
			else secremain=NumByteToWrite;			//The next sector can hold the rest
		}	 
	};	 
}
//Erase the whole chip		  
//This takes a very long time...
void SPI_Flash_Erase_Chip(void)   
{                                   
    SPI_FLASH_Write_Enable();                  //SET WEL 
    SPI_Flash_Wait_Busy();   
  	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_ChipErase);        //Send the chip erase command  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
	SPI_Flash_Wait_Busy();   				   //Wait for the chip erase to finish
}   
//Erase one sector
//Dst_Addr: sector address, set to suit the actual capacity
//A sector erase takes at least 150ms
void SPI_Flash_Erase_Sector(u32 Dst_Addr)   
{  
	//Watch the flash erase; for testing   
// 	printf("fe:%x\r\n",Dst_Addr);	  
 	Dst_Addr*=4096;
    SPI_FLASH_Write_Enable();                  //SET WEL 	 
    SPI_Flash_Wait_Busy();   
  	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_SectorErase);      //Send the sector erase command 
    SPI1_ReadWriteByte((u8)((Dst_Addr)>>16));  //Send the 24-bit address    
    SPI1_ReadWriteByte((u8)((Dst_Addr)>>8));   
    SPI1_ReadWriteByte((u8)Dst_Addr);  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
    SPI_Flash_Wait_Busy();   				   //Wait for the erase to finish
}  
//Wait until idle
void SPI_Flash_Wait_Busy(void)   
{   
	while((SPI_Flash_ReadSR()&0x01)==0x01);   // wait for the BUSY bit to clear
}  
//Enter power-down mode
void SPI_Flash_PowerDown(void)   
{ 
  	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_PowerDown);        //Send the power-down command  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
    delay_us(3);                               //Wait for TPD  
}   
//Wake up
void SPI_Flash_WAKEUP(void)   
{  
  	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_ReleasePowerDown);   //  send W25X_PowerDown command 0xAB    
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
    delay_us(3);                               //Wait for TRES1
}
