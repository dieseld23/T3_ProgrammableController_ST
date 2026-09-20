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
//容量为2M字节
													 
//初始化SPI FLASH的IO口
void SPI_Flash_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);	//PORTC时钟使能

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;			//PC4-CS
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  	//PC4推挽输出 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_Init(GPIOC, &GPIO_InitStructure);				//初始化GPIOA
	GPIO_SetBits(GPIOC, GPIO_Pin_4);  					//PB7输出高

//	RCC->APB2ENR |= (1<<2) | (1<<4);//使能PORTA,PORTC时钟 	    
//	GPIOC->CRL &= 0XFFF0FFFF; 
//	GPIOC->CRL |= 0X00030000;
//	SPI_FLASH_CS = 1;	    
	
//	GPIOA->CRL &= 0X000FFFFF;
//	GPIOA->CRL |= 0XBBB00000;

	SPI_FLASH_TYPE = SPI_Flash_ReadID();//读取FLASH ID.
}  

//读取SPI_FLASH的状态寄存器
//BIT7  6   5   4   3   2   1   0
//SPR   RV  TB BP2 BP1 BP0 WEL BUSY
//SPR:默认0,状态寄存器保护位,配合WP使用
//TB,BP2,BP1,BP0:FLASH区域写保护设置
//WEL:写使能锁定
//BUSY:忙标记位(1,忙;0,空闲)
//默认:0x00
u8 SPI_Flash_ReadSR(void)   
{  
	u8 byte=0;   
	SPI_FLASH_CS=0;                            //Enable the device   
	SPI1_ReadWriteByte(W25X_ReadStatusReg);    //发送读取状态寄存器命令    
	byte=SPI1_ReadWriteByte(0Xff);             //读取一个字节  
	SPI_FLASH_CS=1;                            //Deselect the chip     
	return byte;   
} 
//写SPI_FLASH状态寄存器
//只有SPR,TB,BP2,BP1,BP0(bit 7,5,4,3,2)可以写!!!
void SPI_FLASH_Write_SR(u8 sr)   
{   
	SPI_FLASH_CS=0;                            //Enable the device   
	SPI1_ReadWriteByte(W25X_WriteStatusReg);   //发送写取状态寄存器命令    
	SPI1_ReadWriteByte(sr);               //写入一个字节  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
}   
//SPI_FLASH写使能	
//将WEL置位   
void SPI_FLASH_Write_Enable(void)   
{
	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_WriteEnable);      //发送写使能  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
} 
//SPI_FLASH写禁止	
//将WEL清零  
void SPI_FLASH_Write_Disable(void)   
{  
	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_WriteDisable);     //发送写禁止指令    
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
}
  	  
u16 SPI_Flash_ReadID(void)
{
	u16 Temp = 0;	  
	SPI_FLASH_CS=0;				    
	SPI1_ReadWriteByte(0x9F);//发送读取ID命令	    	     	 			   
	Temp = SPI1_ReadWriteByte(0xFF);  
	Temp = (Temp << 8) | SPI1_ReadWriteByte(0xFF);
	SPI1_ReadWriteByte(0xFF);  
	SPI1_ReadWriteByte(0xFF);	 
	SPI_FLASH_CS=1;				     
	return Temp;
}   		    
//读取SPI FLASH  
//在指定地址开始读取指定长度的数据
//pBuffer: data buffer
//ReadAddr:开始读取的地址(24bit)
//NumByteToRead:要读取的字节数(最大65535)
void SPI_Flash_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)   
{ 
 	u16 i;   										    
	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_ReadData);         //发送读取命令   
    SPI1_ReadWriteByte((u8)((ReadAddr)>>16));  //Send the 24-bit address    
    SPI1_ReadWriteByte((u8)((ReadAddr)>>8));   
    SPI1_ReadWriteByte((u8)ReadAddr);   
    for(i=0;i<NumByteToRead;i++)
	{ 
        pBuffer[i]=SPI1_ReadWriteByte(0XFF);   //循环读数  
    }
	SPI_FLASH_CS=1;  				    	      
}  
//SPI在一页(0~65535)内写入少于256个字节的数据
//在指定地址开始写入最大256字节的数据
//pBuffer: data buffer
//WriteAddr: start address to write (24-bit)
//NumByteToWrite:要写入的字节数(最大256),该数不应该超过该页的剩余字节数!!!	 
void SPI_Flash_Write_Page(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
 	u16 i;  
    SPI_FLASH_Write_Enable();                  //SET WEL 
	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_PageProgram);      //发送写页命令   
    SPI1_ReadWriteByte((u8)((WriteAddr)>>16)); //Send the 24-bit address    
    SPI1_ReadWriteByte((u8)((WriteAddr)>>8));   
    SPI1_ReadWriteByte((u8)WriteAddr);   
    for(i=0;i<NumByteToWrite;i++)SPI1_ReadWriteByte(pBuffer[i]);//循环写数  
	SPI_FLASH_CS=1;                            //Deselect the chip 
	SPI_Flash_Wait_Busy();					   //等待写入结束
} 
//无检验写SPI FLASH 
//必须确保所写的地址范围内的数据全部为0XFF,否则在非0XFF处写入的数据将失败!
//具有自动换页功能 
//在指定地址开始写入指定长度的数据,但是要确保地址不越界!
//pBuffer: data buffer
//WriteAddr: start address to write (24-bit)
//NumByteToWrite: number of bytes to write (max 65535)
//CHECK OK
void SPI_Flash_Write_NoCheck(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)   
{ 			 		 
	u16 pageremain;	   
	pageremain=256-WriteAddr%256; //单页剩余的字节数		 	    
	if(NumByteToWrite<=pageremain)pageremain=NumByteToWrite;//不大于256个字节
	while(1)
	{	   
		SPI_Flash_Write_Page(pBuffer,WriteAddr,pageremain);
		if(NumByteToWrite==pageremain)break;//Write finished
	 	else //NumByteToWrite>pageremain
		{
			pBuffer+=pageremain;
			WriteAddr+=pageremain;	

			NumByteToWrite-=pageremain;			  //减去已经写入了的字节数
			if(NumByteToWrite>256)pageremain=256; //一次可以写入256个字节
			else pageremain=NumByteToWrite; 	  //不够256个字节了
		}
	};	    
} 
//写SPI FLASH  
//在指定地址开始写入指定长度的数据
//该函数带擦除操作!
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
	secoff=WriteAddr%4096;//在扇区内的偏移
	secremain=4096-secoff;//Remaining space in the sector   
 	//printf("ad:%X,nb:%X\r\n",WriteAddr,NumByteToWrite);//测试用
 	if(NumByteToWrite<=secremain)secremain=NumByteToWrite;//不大于4096个字节
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
			WriteAddr+=secremain;//写地址偏移	   
		   	NumByteToWrite-=secremain;				//字节数递减
			if(NumByteToWrite>4096)secremain=4096;	//The next sector still will not hold all of it
			else secremain=NumByteToWrite;			//The next sector can hold the rest
		}	 
	};	 
}
//擦除整个芯片		  
//等待时间超长...
void SPI_Flash_Erase_Chip(void)   
{                                   
    SPI_FLASH_Write_Enable();                  //SET WEL 
    SPI_Flash_Wait_Busy();   
  	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_ChipErase);        //发送片擦除命令  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
	SPI_Flash_Wait_Busy();   				   //等待芯片擦除结束
}   
//擦除一个扇区
//Dst_Addr:扇区地址 根据实际容量设置
//擦除一个山区的最少时间:150ms
void SPI_Flash_Erase_Sector(u32 Dst_Addr)   
{  
	//监视falsh擦除情况,测试用   
// 	printf("fe:%x\r\n",Dst_Addr);	  
 	Dst_Addr*=4096;
    SPI_FLASH_Write_Enable();                  //SET WEL 	 
    SPI_Flash_Wait_Busy();   
  	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_SectorErase);      //发送扇区擦除指令 
    SPI1_ReadWriteByte((u8)((Dst_Addr)>>16));  //Send the 24-bit address    
    SPI1_ReadWriteByte((u8)((Dst_Addr)>>8));   
    SPI1_ReadWriteByte((u8)Dst_Addr);  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
    SPI_Flash_Wait_Busy();   				   //等待擦除完成
}  
//等待空闲
void SPI_Flash_Wait_Busy(void)   
{   
	while((SPI_Flash_ReadSR()&0x01)==0x01);   // 等待BUSY位清空
}  
//进入掉电模式
void SPI_Flash_PowerDown(void)   
{ 
  	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_PowerDown);        //发送掉电命令  
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
    delay_us(3);                               //等待TPD  
}   
//唤醒
void SPI_Flash_WAKEUP(void)   
{  
  	SPI_FLASH_CS=0;                            //Enable the device   
    SPI1_ReadWriteByte(W25X_ReleasePowerDown);   //  send W25X_PowerDown command 0xAB    
	SPI_FLASH_CS=1;                            //Deselect the chip     	      
    delay_us(3);                               //等待TRES1
}
