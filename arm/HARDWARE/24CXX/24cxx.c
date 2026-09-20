#include "24cxx.h" 
#include "delay.h" 										 

U8_T Get_Mini_Type(void);
//Initialise the IIC interface
void AT24CXX_Init(void)
{
	IIC_Init();
}

//Read one byte from the given address in the AT24CXX
//ReadAddr: address to start reading from  
//Return  : the data read
u8 AT24CXX_ReadOneByte(u16 ReadAddr)
{				  
	u8 temp = 0;
		
  IIC_Start();  
	if(EE_TYPE > AT24C16)
	{
		IIC_Send_Byte(0XA0);			//Send the write command
		IIC_Wait_Ack();
		IIC_Send_Byte(ReadAddr >> 8);	//Send the high address byte	    
	}
	else
	{
		IIC_Send_Byte(0XA0 + ((ReadAddr / 256) << 1));	//Send device address 0xA0, write data 	   
	}
	
	IIC_Wait_Ack(); 
  IIC_Send_Byte(ReadAddr % 256);		//Send the low address byte
	IIC_Wait_Ack();
	    
	IIC_Start();  	 	   
	IIC_Send_Byte(0XA1);				//Enter receive mode			   
	IIC_Wait_Ack();	 
  temp = IIC_Read_Byte(0);
			   
  IIC_Stop();							//Generate a STOP condition	 
 
	return temp;
}

//Write one byte to the given address in the AT24CXX
//WriteAddr  : destination address for the data    
//DataToWrite: the data to write
void AT24CXX_WriteOneByte(u16 WriteAddr, u8 DataToWrite)
{				   	  	    																 
//    if((Get_Mini_Type() != MINI_NEW_TINY) && (Get_Mini_Type() != MINI_TINY_ARM) && (Get_Mini_Type() != MINI_TINY_11I))
//		IIC_WP = 0 ;
	IIC_Start();  
	if(EE_TYPE > AT24C16)
	{
		IIC_Send_Byte(0XA0);			//Send the write command
		IIC_Wait_Ack();
		IIC_Send_Byte(WriteAddr >> 8);	//Send the high address byte	  
	}
	else
	{
		IIC_Send_Byte(0XA0 + ((WriteAddr / 256) << 1));	//Send device address 0xA0, write data 	 
	}
	
	IIC_Wait_Ack();	   
    IIC_Send_Byte(WriteAddr % 256);		//Send the low address byte
	IIC_Wait_Ack(); 	 										  		   
	IIC_Send_Byte(DataToWrite);			//Send a byte							   
	IIC_Wait_Ack();
	  		    	   
    IIC_Stop();							//Generate a STOP condition 
//	if((Get_Mini_Type() != MINI_NEW_TINY) && (Get_Mini_Type() != MINI_TINY_ARM) && (Get_Mini_Type() != MINI_TINY_11I))
//		IIC_WP = 0 ;
	delay_ms(5);	 
}

//Write Len bytes starting at the given address in the AT24CXX
//This function is used to write 16-bit or 32-bit values.
//WriteAddr  : address to start writing at  
//DataToWrite: start of the data array
//Len        : length of the data to write, 2 or 4
void AT24CXX_WriteLenByte(u16 WriteAddr, u32 DataToWrite, u8 Len)
{  	
	u8 t;
	for(t = 0; t < Len; t++)
	{
		AT24CXX_WriteOneByte(WriteAddr + t, (DataToWrite >> (8 * t)) & 0xff);
	}												    
}

//Read Len bytes starting at the given address in the AT24CXX
//This function is used to read 16-bit or 32-bit values.
//ReadAddr   : address to start reading from 
//Return     : the data
//Len        : length of the data to read, 2 or 4
u32 AT24CXX_ReadLenByte(u16 ReadAddr, u8 Len)
{  	
	u8 t;
	u32 temp=0;
	for(t = 0; t < Len; t++)
	{
		temp <<= 8;
		temp += AT24CXX_ReadOneByte(ReadAddr + Len - t - 1); 	 				   
	}
	return temp;												    
}

//Check whether the AT24CXX is working
//The last address of the 24XX (255) is used to hold the flag word.
//If a different 24C part is used, this address must be changed
//Returns 1: check failed
//Returns 0: check passed
u8 AT24CXX_Check(void)
{
	u8 temp;
	temp = AT24CXX_ReadOneByte(255);	//Avoid writing to the AT24CXX on every power-up			   
	if(temp == 0X55)
		return 0;		   
	else								//Excluding the very first initialisation
	{
		AT24CXX_WriteOneByte(255, 0X55);
	    temp = AT24CXX_ReadOneByte(255);	  
		if(temp == 0X55)return 0;
	}
	return 1;											  
}

//Read a given number of bytes starting at the given address in the AT24CXX
//ReadAddr : address to start reading from, 0~255 for the 24c02
//pBuffer  : start of the data array
//NumToRead: number of bytes to read
void AT24CXX_Read(u16 ReadAddr, u8 *pBuffer, u16 NumToRead)
{
	while(NumToRead)
	{
		*pBuffer++ = AT24CXX_ReadOneByte(ReadAddr++);	
		NumToRead--;
	}
}
  
//Write a given number of bytes starting at the given address in the AT24CXX
//WriteAddr : address to start writing at, 0~255 for the 24c02
//pBuffer   : start of the data array
//NumToWrite: number of bytes to write
void AT24CXX_Write(u16 WriteAddr, u8 *pBuffer, u16 NumToWrite)
{
	while(NumToWrite--)
	{
		AT24CXX_WriteOneByte(WriteAddr, *pBuffer);
		WriteAddr++;
		pBuffer++;
	}
}
