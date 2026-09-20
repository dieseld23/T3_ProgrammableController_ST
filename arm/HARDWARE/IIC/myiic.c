#include "product.h"
#include "24cxx.h" 
#include "delay.h" 										 

#if (ARM_MINI || ARM_CM5)
//IO direction control
static void SDA_IN(void)
{
#if (ARM_MINI || ARM_CM5)
	GPIO_InitTypeDef GPIO_InitStructure; 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_8);
#else
	GPIO_InitTypeDef GPIO_InitStructure; 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_2);
	
#endif
}
static void SDA_OUT(void)
{
#if (ARM_MINI || ARM_CM5)
	GPIO_InitTypeDef GPIO_InitStructure; 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_8);
#else
	GPIO_InitTypeDef GPIO_InitStructure; 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_2);
#endif
}
#endif
//IO operation macros	 
void IIC_SCL(u8 status)
{
#if (ARM_MINI || ARM_CM5)
	if(status) 
		GPIO_SetBits(GPIOA, GPIO_Pin_15);
	else
		GPIO_ResetBits(GPIOA, GPIO_Pin_15); 
#else
	if(status) 
		GPIO_SetBits(GPIOA, GPIO_Pin_3);
	else
		GPIO_ResetBits(GPIOA, GPIO_Pin_3); 
	
#endif
}
void IIC_SDA(u8 status)
{
#if (ARM_MINI || ARM_CM5)
	if(status) 
		GPIO_SetBits(GPIOA, GPIO_Pin_8);
	else
		GPIO_ResetBits(GPIOA, GPIO_Pin_8); 
#else
	if(status) 
		GPIO_SetBits(GPIOA, GPIO_Pin_2);
	else
		GPIO_ResetBits(GPIOA, GPIO_Pin_2); 
#endif
}	
u8 READ_SDA(void)
{
	u8 status;
#if (ARM_MINI || ARM_CM5)
	status = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8);
#else
	status = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2);  		  
#endif
	return status;
}

u8 READ_SCL()
{
	u8 status;
#if (ARM_MINI || ARM_CM5)
	status = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_15);
#else
	status = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3);  		  
#endif
	return status;
}

//Initialise the IIC bus
void IIC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
#if (ARM_MINI || ARM_CM5)
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOD, ENABLE);//Enable SCL
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_15 ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_8 | GPIO_Pin_15 );
#else	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);//Enable SCL
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_2 ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_3 | GPIO_Pin_2 );
#endif
}

//Generate an IIC START condition
void IIC_Start(void)
{
	SDA_OUT();		//SDA line as output
	IIC_SDA(1);	  	  
	IIC_SCL(1);
	delay_us(4);
 	IIC_SDA(0);	//START:when CLK is high,DATA change form high to low 
	delay_us(4);
	IIC_SCL(0);	//Hold the I2C bus low, ready to send or receive data 
}
	  
//Generate an IIC STOP condition
void IIC_Stop(void)
{
	SDA_OUT();		//SDA line as output
	IIC_SCL(0);
	IIC_SDA(0);	//STOP:when CLK is high DATA change form low to high
 	delay_us(4);
	IIC_SCL(1); 
	IIC_SDA(1);	//Send the I2C bus STOP signal
	delay_us(4);							   	
}


#if !(ARM_TSTAT_WIFI )	
//Wait for the slave to acknowledge
//Return: 1 = no ACK received
//        0 = ACK received
u8 IIC_Wait_Ack1(void)
{
	u8 i;
	IIC_SCL(1);
	delay_us(2);
	IIC_SCL(0);
	for (i=0; i<100; i++)
	{
		SDA_OUT();
		IIC_SDA(1);
	//c=I2C_SDA;
		SDA_IN();
		if (READ_SDA() == 0){
	// if data line is low, pulse the clock.
			delay_us(5);
		
		return 0;
		}		
	}
	IIC_SCL(0);
	return 1;
}
#endif
//#else
u8 IIC_Wait_Ack(void)
{
	u8 ucErrTime = 0;
	IIC_SDA(1);
	SDA_IN();		//Set SDA as an input  
	
	delay_us(1);	   
	IIC_SCL(1);
	delay_us(1);	 
	while(READ_SDA())
	{
		ucErrTime++;
		if(ucErrTime > 250)
		{
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL(0);	//Drive the clock low 	   
	return 0;  
}
 //#endif
//Send an ACK
void IIC_Ack(void)
{
	IIC_SCL(0);
	SDA_OUT();
	IIC_SDA(0);
	delay_us(2);
	IIC_SCL(1);
	delay_us(2);
	IIC_SCL(0);
}

//Send a NACK		    
void IIC_NAck(void)
{
	IIC_SCL(0);
	SDA_OUT();
	IIC_SDA(1);
	delay_us(2);
	IIC_SCL(1);
	delay_us(2);
	IIC_SCL(0);
}
					 				     
//Send one byte over IIC
//Returns whether the slave acknowledged
//1 = ACK
//0 = no ACK			  
void IIC_Send_Byte(u8 txd)
{                        
    u8 t;   
	SDA_OUT(); 	    
    IIC_SCL(0);		//Pull the clock low to start the data transfer
    for(t = 0; t < 8; t++)
    {              
        IIC_SDA((txd & 0x80) >> 7);
        txd <<= 1; 	  
		delay_us(2);   //All three delays are required for the TEA5767
		IIC_SCL(1);
		delay_us(2); 
		IIC_SCL(0);	
		delay_us(2);
    }	 
}
	    
//Read one byte; ack=1 sends ACK, ack=0 sends NACK   
u8 IIC_Read_Byte(unsigned char ack)
{
	unsigned char i, receive = 0;

	SDA_IN();			//Set SDA as an input

  for(i = 0; i < 8; i++)
	{
		IIC_SCL(0); 
		delay_us(2);
		IIC_SCL(1);
		receive <<= 1;
		if(READ_SDA())
			receive++;   
		delay_us(1); 
	}
					 
	if(!ack)
			IIC_NAck();//Send NACK
	else
			IIC_Ack(); //Send ACK
		 
	return receive;
}
