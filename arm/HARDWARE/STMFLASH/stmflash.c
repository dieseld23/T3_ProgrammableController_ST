#include "stmflash.h"
#include "delay.h"
#include "usart.h"
u16 iapbuf[1024]; 

//V1.1 change log
//Fixed an address offset bug in STMFLASH_Write.
//////////////////////////////////////////////////////////////////////////////////

//Unlock the STM32 FLASH
void STMFLASH_Unlock(void)
{
	FLASH->KEYR = FLASH_KEY1;//Write the unlock sequence.
	FLASH->KEYR = FLASH_KEY2;
}

//Lock the flash
void STMFLASH_Lock(void)
{
  FLASH->CR |= 1 << 7;//Lock
}

//Read the FLASH status
u8 STMFLASH_GetStatus(void)
{	
	u32 res;		
	res = FLASH->SR; 
	if(res & (1 << 0))
		return 1;		    //Busy
	else if(res & (1 << 2))
		return 2;			//Programming error
	else if(res & (1 << 4))
		return 3;			//Write protection error
	return 0;				//Operation complete
}

//Wait for the operation to finish
//time: how long to wait
//Return: the status.
u8 STMFLASH_WaitDone(u16 time)
{
	u8 res;
	do
	{
		res = STMFLASH_GetStatus();
		if(res != 1)
			break;//Not busy, so no wait is needed and we return.
		delay_us(1);
		time--;
	 }while(time);
	
	 if(time == 0)
		 res = 0xff;//TIMEOUT
	 return res;
}

//Erase a page
//paddr: the page address
//Return: the result
u8 STMFLASH_ErasePage(u32 paddr)
{
	u8 res = 0;
	res = STMFLASH_WaitDone(0X5FFF);//Wait for the previous operation to finish, >20ms    
	if(res == 0)
	{ 
		FLASH->CR |= 1 << 1;			//Page erase
		FLASH->AR = paddr;				//Set the page address 
		FLASH->CR |= 1<<6;				//Start the erase		  
		res = STMFLASH_WaitDone(0X5FFF);//Wait for the operation to finish, >20ms  
		if(res != 1)//Not busy
		{
			FLASH->CR &= ~(1 << 1);//Clear the page erase bit.
		}
	}
	return res;
}

//Write a half word at the given FLASH address
//faddr: the address (it must be a multiple of 2!!)
//dat: the data to write
//Return: the result of the write
u8 STMFLASH_WriteHalfWord(u32 faddr, u16 dat)
{
	u8 res;	   	    
	res = STMFLASH_WaitDone(0XFFF);	 
	if(res == 0)//OK
	{
		FLASH->CR |= 1 << 0;//Programming enable
		*(vu16*)faddr = dat;//Write data
		res = STMFLASH_WaitDone(0XFFF);//Wait for the operation to finish
		if(res != 1)//Operation succeeded
		{
			FLASH->CR &= ~(1 << 0);//Clear the PG bit.
		}
	} 
	return res;
}
 
//Read the half word (16 bits) at the given address 
//faddr: the address to read 
//Return: the data there.
u16 STMFLASH_ReadHalfWord(u32 faddr)
{
	return *(vu16*)faddr; 
}

u8 STMFLASH_BYTE(u32 faddr)
{
//	u16 temp = *(vu16*)faddr;
	if(faddr%2 == 0)
	return (*(vu16*)faddr) &0xff; 
	else 
	return ((*(vu16*)(faddr-1))>>8) &0xff;	
}

#if STM32_FLASH_WREN	//If writing is enabled   
//Write without checking
//WriteAddr: start address
//pBuffer: data pointer
//NumToWrite: number of half-words (16-bit)   
void STMFLASH_Write_NoCheck(u32 WriteAddr, u16 *pBuffer, u16 NumToWrite)   
{ 			 		 
	u16 i;
	for(i = 0; i < NumToWrite; i++)
	{
		STMFLASH_WriteHalfWord(WriteAddr, pBuffer[i]);
	    WriteAddr += 2;									//Advance the address by 2.
	}  
}
 
//Write a given number of bytes starting at a given address
//WriteAddr: the start address (it must be a multiple of 2!!)
//pBuffer: data pointer
//NumToWrite: the number of half words (16-bit values) to write.
#if STM32_FLASH_SIZE < 256
#define STM_SECTOR_SIZE 1024 //Bytes
#else 
#define STM_SECTOR_SIZE	2048
#endif		 
u16 STMFLASH_BUF[STM_SECTOR_SIZE / 2];//At most 2K bytes
void STMFLASH_Write(u32 WriteAddr, u16 *pBuffer, u16 NumToWrite)	
{
	u32 secpos;		//Sector address
	u16 secoff;		//Offset within the sector, counted in 16-bit words
	u16 secremain;	//Space left in the sector, counted in 16-bit words	   
 	u16 i;   

	u32 offaddr;	//The address with 0x08000000 removed
	if(WriteAddr < STM32_FLASH_BASE || (WriteAddr >= (STM32_FLASH_BASE + 1024 * STM32_FLASH_SIZE)))
		return;		//Invalid address
	//__disable_irq();
	//STMFLASH_Unlock();							//unlock
	offaddr = WriteAddr - STM32_FLASH_BASE;		//The real offset address.
	secpos = offaddr / STM_SECTOR_SIZE;			//Sector address, 0~127 for the STM32F103RBT6
	secoff = (offaddr%STM_SECTOR_SIZE) / 2;		//Offset within the sector, in units of 2 bytes.
	secremain = STM_SECTOR_SIZE / 2 - secoff;	//Remaining space in the sector   
	if(NumToWrite <= secremain)
		secremain = NumToWrite;					//No further than the end of the sector

	while(1) 
	{			
		STMFLASH_Read(secpos * STM_SECTOR_SIZE + STM32_FLASH_BASE, STMFLASH_BUF, STM_SECTOR_SIZE / 2);	//Read the whole sector
		for(i = 0; i < secremain; i++)			//Verify the data
		{
			if(STMFLASH_BUF[secoff + i] != 0XFFFF)
				break;							//Erase required  	  
		}
		
		if(i < secremain)						//Erase required
		{
			STMFLASH_ErasePage(secpos * STM_SECTOR_SIZE + STM32_FLASH_BASE);	//Erase this sector
			for(i = 0; i < secremain; i++)		//Copy
			{
				STMFLASH_BUF[i + secoff] = pBuffer[i];	  
			}
			STMFLASH_Write_NoCheck(secpos * STM_SECTOR_SIZE + STM32_FLASH_BASE, STMFLASH_BUF, STM_SECTOR_SIZE / 2);	//Write the whole sector  
		}
		else
		{
			STMFLASH_Write_NoCheck(WriteAddr, pBuffer, secremain);	//Already erased, so write straight into the rest of the sector. 				   
		}
		
		if(NumToWrite == secremain)
		{
			break;	//Write finished
		}
		else		//Write not finished
		{
			secpos++;								//Increment the sector address
			secoff = 0;								//Offset is 0 	 
		   	pBuffer += secremain;  					//Pointer offset
			WriteAddr += secremain*2;				//Advance the write address (16-bit addressing, so multiply by 2)	   
		   	NumToWrite -= secremain;				//Decrement the half word count
			if(NumToWrite >(STM_SECTOR_SIZE / 2))
				secremain = STM_SECTOR_SIZE / 2;	//The next sector still will not hold all of it
			else
				secremain = NumToWrite;				//The next sector can hold the rest
		}	 
	};
	
	//STMFLASH_Lock();	//lock
	//__enable_irq();
}
#endif

//Read a given number of bytes starting at a given address
//ReadAddr: the start address
//pBuffer: data pointer
//NumToWrite: number of half-words (16-bit)
void STMFLASH_Read(u32 ReadAddr, u16 *pBuffer, u16 NumToRead)   	
{
	u16 i;
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i] = STMFLASH_ReadHalfWord(ReadAddr);	//Read 2 bytes.
		ReadAddr += 2;									//Offset by 2 bytes.	
	}
}
extern u16 far Test[50];

void STMFLASH_MUL_Read(u32 ReadAddr, u8 *pBuffer, u16 NumToRead)   	
{
	u16 i;
	for(i=0;i < NumToRead;i++)
	{
		if(ReadAddr >= 0x80080000) 
		{
			break;
		}
		pBuffer[i] = STMFLASH_BYTE(ReadAddr);	//Read 1 byte.
		ReadAddr ++;			//Offset by 2 bytes.	
	}
}
//////////////////////////////////////////for testing///////////////////////////////////////////
//WriteAddr: start address
//WriteData: the data to write
void Test_Write(u32 WriteAddr, u16 WriteData)   	
{
	STMFLASH_Write(WriteAddr, &WriteData, 1);	//Write one word 
}

//Set the stack top address
//addr: the stack top address
__asm void MSR_MSP(u32 addr) 
{
    MSR MSP, r0    //set Main Stack value
    BX r14
}

void iap_write_appbin(u32 appxaddr,u8 *appbuf,u32 appsize)
{
	u16 t;
	u16 i=0;
	u16 temp;
	u32 fwaddr=appxaddr;//The address being written
	u8 *dfu=appbuf;
	if(appxaddr < STM32_FLASH_BASE || (appxaddr >= (STM32_FLASH_BASE + 1024 * STM32_FLASH_SIZE)))
		return;		//Invalid address
						//Unlock 
	for(t=0;t<appsize;t+=2)
	{						    
		temp=(u16)dfu[1]<<8;
		temp+=(u16)dfu[0];	  
		dfu+=2;//Offset by 2 bytes
		iapbuf[i++]=temp;	    
		if(i==1024)
		{
			i=0;
 			STMFLASH_Write(fwaddr,iapbuf,1024);	
 			//STMFLASH_Write_NoCheck(fwaddr, iapbuf,1024);
			fwaddr+=2048;//Offset 2048; 16=2*8, so multiply by 2.
		}
	}
	if(i)
	{
 		STMFLASH_Write(fwaddr,iapbuf,i);//Write the last few bytes.
 		//STMFLASH_Write_NoCheck(fwaddr, iapbuf,i);
	}
}

typedef  void (*iapfun)(void);				//Declare a function pointer.

iapfun jump2app; 

void iap_load_app(u32 appxaddr)
{
	if(((*(vu32*)appxaddr)&0x2FFE0000)==0x20000000)	//Check that the stack top address is valid.
	{ 
		jump2app=(iapfun)*(vu32*)(appxaddr+4);		//The second word of the user code area is the program start (reset) address		
		MSR_MSP(*(vu32*)appxaddr);					//Set up the application stack pointer (the first word of the user code area holds the stack top)
		jump2app();									//Jump to the application.
		
	}
}	
