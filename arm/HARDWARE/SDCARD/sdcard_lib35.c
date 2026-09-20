#include <string.h>
#include "sdcard.h"

#define SDIO_CLKEN_SW(X)	(BIT_ADDR(SDIO->CLKCR, 8) = X)


static u8 CardType = SDIO_STD_CAPACITY_SD_CARD_V1_1;		//SD card type (a 1.x card by default)
static u32 CSD_Tab[4], CID_Tab[4], RCA=0;					//SD card CSD, CID and relative address (RCA) data
static u8 DeviceMode = SD_DMA_MODE;		   					//Operating mode. Note it only takes effect through SD_SetDeviceMode; this is just a default (SD_DMA_MODE)
static u8 StopCondition = 0; 								//Whether to send the stop-transfer command; used for multi-block DMA reads and writes  
volatile SD_Error TransferError = SD_OK;					//Data transfer error flag, used for DMA reads and writes	    
volatile u8 TransferEnd = 0;								//Transfer complete flag, used for DMA reads and writes
SD_CardInfo SDCardInfo;										//SD card details

//A buffer just for SD_ReadDisk and SD_WriteDisk, used when the caller's buffer address is not 4-byte aligned,
//so that the data buffer address is guaranteed to be 4-byte aligned.
__align(4) u8 SDIO_DATA_BUFFER[512];


//SDIO_InitTypeDef SDIO_InitStructure;
//SDIO_CmdInitTypeDef SDIO_CmdInitStructure;
//SDIO_DataInitTypeDef SDIO_DataInitStructure;


void Sdio_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);

	/* Configure PC.08, PC.09, PC.10, PC.11, PC.12 pin: D0, D1, D2, D3, CLK pin */  	 
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC, GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12);
	
	/* Configure PD2,    pin: cmd */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	GPIO_SetBits(GPIOD, GPIO_Pin_2);

//	RCC->AHBENR |= (1 << 1) | (1 << 10);    //enable the DMA2 and SDIO clocks
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2 | RCC_AHBPeriph_SDIO, ENABLE);
}

//void SDIO_DeInit(void)
//{
//	SDIO->POWER = 0x00000000;
//	SDIO->CLKCR = 0x00000000;
//	SDIO->ARG = 0x00000000;
//	SDIO->CMD = 0x00000000;
//	SDIO->DTIMER = 0x00000000;
//	SDIO->DLEN = 0x00000000;
//	SDIO->DCTRL = 0x00000000;
//	SDIO->ICR = 0x00C007FF;
//	SDIO->MASK = 0x00000000;
//
//	MY_NVIC_Init(0, 0, SDIO_IRQn, 2);
//}

//SDIO clock initialisation
//clkdiv: the clock divider
//CK = SDIOCLK/[clkdiv+2]; (SDIOCLK is the AHB clock, normally 72MHz)
void SDIO_Clock_Set(u8 clkdiv)
{
  	SDIO->CLKCR &= 0XFFFFFF00;
 	SDIO->CLKCR |= clkdiv; 
} 

//SDIO command send function
//cmdindex: the command index; the low six bits are used
//waitrsp: the expected response. 00/10 = none; 01 = short; 11 = long
//arg: the argument
void SDIO_Send_Cmd(u8 cmdindex, u8 waitrsp, u32 arg)
{						    
	SDIO->ARG = arg;
	SDIO->CMD &= 0XFFFFF800;		//Clear index and waitrsp
	SDIO->CMD |= cmdindex & 0X3F;	//Set the new index			 
	SDIO->CMD |= waitrsp << 6;		//Set the new wait rsp 
	SDIO->CMD |= 0 << 8;			//No waiting
  	SDIO->CMD |= 1 << 10;			//Enable the command path state machine
}

//SDIO data transfer configuration
//datatimeout: the timeout value
//datalen: the transfer length; the low 25 bits are used and it must be a whole number of blocks
//blksize: the block size; the real size is 2^blksize bytes
//dir: transfer direction; 0 = controller to card; 1 = card to controller;
void SDIO_Send_Data_Cfg(u32 datatimeout, u32 datalen, u8 blksize, u8 dir)
{
	SDIO->DTIMER = datatimeout;
  	SDIO->DLEN = datalen & 0X1FFFFFF;	//The low 25 bits are used
	SDIO->DCTRL &= 0xFFFFFF08;			//Clear the previous settings.
	SDIO->DCTRL |= blksize << 4;		//Set the block size
	SDIO->DCTRL |= 0 << 2;				//Block data transfer
	SDIO->DCTRL |= (dir & 0X01) << 1;	//Direction control
	SDIO->DCTRL |= 1 << 0;				//Enable the data transfer, DPSM state machine
}  

//Configure the SDIO DMA  
//mbuf: the memory address
//bufsize: the transfer size
//dir: direction; 1 = memory-->SDIO (write); 0 = SDIO-->memory (read);
void SD_DMA_Config(u32*mbuf, u32 bufsize, u8 dir)
{				  
 	DMA2->IFCR |= (0XF << 12);				//Clear the DMA2 channel 4 flags
 	DMA2_Channel4->CCR &= ~(1 << 0);		//Disable DMA channel 4
  	DMA2_Channel4->CCR &= ~(0X7FF << 4);	//Clear the previous settings: DIR, CIRC, PINC, MINC, PSIZE, MSIZE, PL, MEM2MEM
 	DMA2_Channel4->CCR |= dir << 4;  		//Read from memory   
	DMA2_Channel4->CCR |= 0 << 5;  			//Normal mode
	DMA2_Channel4->CCR |= 0 << 6; 			//Peripheral address, no increment
	DMA2_Channel4->CCR |= 1 << 7;  			//Memory increment mode
	DMA2_Channel4->CCR |= 2 << 8;  			//Peripheral data width 32 bits
	DMA2_Channel4->CCR |= 2 << 10; 			//Memory data width 32 bits
	DMA2_Channel4->CCR |= 2 << 12; 			//High priority	  
  	DMA2_Channel4->CNDTR = bufsize / 4;   	//DMA2, transfer count	  
 	DMA2_Channel4->CPAR = (u32)&SDIO->FIFO;	//DMA2 peripheral address 
	DMA2_Channel4->CMAR = (u32)mbuf; 		//DMA2, memory address
 	DMA2_Channel4->CCR |= 1 << 0; 			//Enable the DMA channel		
}

//Check the result of CMD0
//Return: SD card error code
SD_Error CmdError(void)
{
	SD_Error errorstatus = SD_OK;
	u32 timeout = SDIO_CMD0TIMEOUT;
	
	while(timeout--)
	{
		if(SDIO->STA & (1 << 7)) break;	//Command sent (no response needed)	 
	}
	
	if(timeout == 0)
		return SD_CMD_RSP_TIMEOUT; 
	
	SDIO->ICR = 0X5FF;					//Clear the flag
	return errorstatus;
}	 
//Check the R7 response for errors
//Return: SD card error code
SD_Error CmdResp7Error(void)
{
	SD_Error errorstatus = SD_OK;
	u32 status;
	u32 timeout = SDIO_CMD0TIMEOUT;
 	while(timeout--)
	{
		status = SDIO->STA;
		if(status & ((1 << 0) | (1 << 2) | (1 << 6))) break;	//CRC error / command response timeout / response received (CRC check passed)	
	}
	
 	if((timeout == 0) || (status & (1 << 2)))					//Response timeout
	{																				    
		errorstatus = SD_CMD_RSP_TIMEOUT;						//This card is not 2.0 compatible, or does not support the voltage range given
		SDIO->ICR |= 1 << 2;									//Clear the command response timeout flag
		return errorstatus;
	}
	
	if(status & (1 << 6))										//Response received
	{								   
		errorstatus = SD_OK;
		SDIO->ICR |= 1 << 6;									//Clear the response flags
 	}
	return errorstatus;
}	   
//Check the R1 response for errors
//cmd: the current command
//Return: SD card error code
SD_Error CmdResp1Error(u8 cmd)
{	  
   	u32 status;
	while(1)
	{
		status = SDIO->STA;
		if(status & ((1 << 0) | (1 << 2) | (1 << 6)))break;//CRC error / command response timeout / response received (CRC check passed)	
	}
	
 	if(status & (1 << 2))					//Response timeout
	{																				    
 		SDIO->ICR = 1 << 2;					//Clear the command response timeout flag
		return SD_CMD_RSP_TIMEOUT;
	}
	
 	if(status & (1 << 0))					//CRC error
	{																				    
 		SDIO->ICR = 1<<0;					//Clear the flags
		return SD_CMD_CRC_FAIL;
	}
	
	if(SDIO->RESPCMD != cmd)
		return SD_ILLEGAL_CMD;				//Command mismatch 
	
  	SDIO->ICR = 0X5FF;	 					//Clear the flag
	return (SD_Error)(SDIO->RESP1 & SD_OCR_ERRORBITS);//Return the card response
}
//Check the R3 response for errors
//Return: error status
SD_Error CmdResp3Error(void)
{
	u32 status;						 
 	while(1)
	{
		status = SDIO->STA;
		if(status & ((1 << 0) | (1 << 2) | (1 << 6)))break;//CRC error / command response timeout / response received (CRC check passed)	
	}
	
 	if(status & (1 << 2))					//Response timeout
	{											 
		SDIO->ICR |= 1 << 2;				//Clear the command response timeout flag
		return SD_CMD_RSP_TIMEOUT;
	}
	
   	SDIO->ICR = 0X5FF;	 				//Clear the flag
 	return SD_OK;								  
}
//Check the R2 response for errors
//Return: error status
SD_Error CmdResp2Error(void)
{
	SD_Error errorstatus = SD_OK;
	u32 status;
	u32 timeout = SDIO_CMD0TIMEOUT;
 	while(timeout--)
	{
		status = SDIO->STA;
		if(status & ((1 << 0) | (1 << 2) | (1 << 6)))break;//CRC error / command response timeout / response received (CRC check passed)	
	}
	
  	if((timeout == 0) || (status & (1 << 2)))	//Response timeout
	{																				    
		errorstatus = SD_CMD_RSP_TIMEOUT; 
		SDIO->ICR |= 1 << 2;				//Clear the command response timeout flag
		return errorstatus;
	}	
	
	if(status & (1 << 0))						//CRC error
	{								   
		errorstatus = SD_CMD_CRC_FAIL;
		SDIO->ICR |= 1 << 0;				//Clear the response flags
 	}
	
	SDIO->ICR = 0X5FF;	 				//Clear the flag
 	return errorstatus;								    		 
} 
//Check the R6 response for errors
//cmd: the command that was sent
//prca: the RCA address returned by the card
//Return: error status
SD_Error CmdResp6Error(u8 cmd, u16*prca)
{
	SD_Error errorstatus = SD_OK;
	u32 status;					    
	u32 rspr1;
 	while(1)
	{
		status = SDIO->STA;
		if(status & ((1 << 0) | (1 << 2) | (1 << 6)))break;//CRC error / command response timeout / response received (CRC check passed)	
	}
	
	if(status & (1 << 2))					//Response timeout
	{																				    
 		SDIO->ICR |= 1 << 2;				//Clear the command response timeout flag
		return SD_CMD_RSP_TIMEOUT;
	}
	
	if(status & (1 << 0))					//CRC error
	{								   
		SDIO->ICR |= (1 << 0);				//Clear the response flags
 		return SD_CMD_CRC_FAIL;
	}
	
	if(SDIO->RESPCMD != cmd)				//Check whether the command was answered
	{
 		return SD_ILLEGAL_CMD; 		
	}
	
	SDIO->ICR = 0X5FF;	 				//Clear all flags
	rspr1 = SDIO->RESP1;				//Response received 	 
	if(SD_ALLZERO == (rspr1 & (SD_R6_GENERAL_UNKNOWN_ERROR | SD_R6_ILLEGAL_CMD | SD_R6_COM_CRC_FAILED)))
	{
		*prca = (u16)(rspr1 >> 16);			//Shift right 16 bits to get the RCA
		return errorstatus;
	}
	
   	if(rspr1 & SD_R6_GENERAL_UNKNOWN_ERROR)
		return SD_GENERAL_UNKNOWN_ERROR;
	
   	if(rspr1 & SD_R6_ILLEGAL_CMD)
		return SD_ILLEGAL_CMD;
	
   	if(rspr1 & SD_R6_COM_CRC_FAILED)
		return SD_COM_CRC_FAILED;
	
	return errorstatus;
}

//Power up the card
//Find every card on the SDIO interface, query its voltage and set up the clock
//Return: error code (0 = no error)
SD_Error SD_PowerON(void)
{
	u8 i = 0;
	SD_Error errorstatus = SD_OK;
	u32 response = 0, count = 0, validvoltage = 0;
	u32 SDType = SD_STD_CAPACITY;
	
	// configure the CLKCR register 
	SDIO->CLKCR = 0;					//Clear the previous CLKCR settings
	SDIO->CLKCR |= 0 << 9;				//Not power-save mode
	SDIO->CLKCR |= 0 << 10;				//Bypass off, so CK comes from the divider
	SDIO->CLKCR |= 0 << 11;				//1-bit data width
	SDIO->CLKCR |= 0 << 13;				//SDIOCK is generated on the rising edge of SDIOCLK
	SDIO->CLKCR |= 0 << 14;				//Hardware flow control off    
	SDIO_Clock_Set(SDIO_INIT_CLK_DIV);	//Set the clock frequency (during init it must not exceed 400kHz)			 
 	SDIO->POWER = 0X03;					//Powered up, so start the card clock    
 	SDIO->CLKCR |= 1 << 8;				//Enable SDIOCK
//	SDIO_InitStructure.SDIO_ClockDiv = SDIO_INIT_CLK_DIV;
//	SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
//	SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
//	SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
//	SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_1b;
//	SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
//	SDIO_Init(&SDIO_InitStructure);
//
//	/*!< Set Power State to ON */
//	SDIO_SetPowerState(SDIO_PowerState_ON);
//
//	/*!< Enable SDIO Clock */
//	SDIO_ClockCmd(ENABLE);
		
	for(i = 0; i < 74; i++)
	{
		SDIO_Send_Cmd(SD_CMD_GO_IDLE_STATE, 0, 0);	//Send CMD0 to enter the IDLE STAGE.												  
//		SDIO_CmdInitStructure.SDIO_Argument = 0x00;
//		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_GO_IDLE_STATE;
//		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_No;
//		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//		SDIO_SendCommand(&SDIO_CmdInitStructure);
		errorstatus = CmdError();
		if(errorstatus == SD_OK)
			break;
	}
	if(errorstatus != SD_OK)
		return errorstatus;//Return the error status
	
	SDIO_Send_Cmd(SDIO_SEND_IF_COND, 1, SD_CHECK_PATTERN);	//Send CMD8, short response, to check the SD card interface conditions.
															//arg[11:8]: 01, supported voltage range, 2.7~3.6V
															//arg[7:0]: 0xAA by default
															//Returns response 7
//	SDIO_CmdInitStructure.SDIO_Argument = SD_CHECK_PATTERN;
//	SDIO_CmdInitStructure.SDIO_CmdIndex = SDIO_SEND_IF_COND;
//	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
//	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//	SDIO_SendCommand(&SDIO_CmdInitStructure);
  	errorstatus = CmdResp7Error();							//Wait for an R7 response
 	if(errorstatus == SD_OK) 								//R7 response ok
	{
		CardType = SDIO_STD_CAPACITY_SD_CARD_V2_0;			//SD 2.0 card
		SDType = SD_HIGH_CAPACITY;			   				//High capacity card
	}

	SDIO_Send_Cmd(SD_CMD_APP_CMD, 1, 0);					//Send CMD55, short response	 
//	/*!< CMD55 */
//	SDIO_CmdInitStructure.SDIO_Argument = 0x00;
//	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
//	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
//	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//	SDIO_SendCommand(&SDIO_CmdInitStructure);
	errorstatus = CmdResp1Error(SD_CMD_APP_CMD); 		 	//Wait for an R1 response   
	if(errorstatus == SD_OK)								//SD2.0/SD 1.1,·ñÔòÎªMMC¿¨
	{																  
		//SD¿¨,·¢ËÍACMD41 SD_APP_OP_COND,²ÎÊýÎª:0x80100000 
		while((!validvoltage) && (count < SD_MAX_VOLT_TRIAL))
		{	   										   
			SDIO_Send_Cmd(SD_CMD_APP_CMD, 1, 0);				//Send CMD55, short response	 
//			/*!< SEND CMD55 APP_CMD with RCA as 0 */
//			SDIO_CmdInitStructure.SDIO_Argument = 0x00;
//			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_APP_CMD;
//			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
//			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//			SDIO_SendCommand(&SDIO_CmdInitStructure);
			errorstatus = CmdResp1Error(SD_CMD_APP_CMD); 	 	//Wait for an R1 response   
 			if(errorstatus != SD_OK)
				return errorstatus;   							//ÏìÓ¦´íÎ

			SDIO_Send_Cmd(SD_CMD_SD_APP_OP_COND, 1, SD_VOLTAGE_WINDOW_SD | SDType);//·¢ËÍACMD41,¶ÌÏìÓ¦	 
//			SDIO_CmdInitStructure.SDIO_Argument = SD_VOLTAGE_WINDOW_SD | SDType;
//			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SD_APP_OP_COND;
//			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
//			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//			SDIO_SendCommand(&SDIO_CmdInitStructure);
			errorstatus = CmdResp3Error(); 						//Wait for an R3 response   
 			if(errorstatus != SD_OK)
				return errorstatus;   							//Response error 
			
			response = SDIO_GetResponse(SDIO_RESP1);			//Response received
			validvoltage = (((response >> 31) == 1) ? 1 : 0);	//ÅÐ¶ÏSD¿¨ÉÏµçÊÇ·ñÍê³É
			count++;
		}
		
		if(count >= SD_MAX_VOLT_TRIAL)
		{
			errorstatus = SD_INVALID_VOLTRANGE;
			return errorstatus;
		}
		
		if(response &= SD_HIGH_CAPACITY)
		{
			CardType = SDIO_HIGH_CAPACITY_SD_CARD;
		}
 	}
	else//MMC¿¨
	{
		//MMC¿¨,·¢ËÍCMD1 SDIO_SEND_OP_COND,²ÎÊýÎª:0x80FF8000 
		while((!validvoltage) && (count < SD_MAX_VOLT_TRIAL))
		{	   										   				   
			SDIO_Send_Cmd(SD_CMD_SEND_OP_COND, 1, SD_VOLTAGE_WINDOW_MMC);	//·¢ËÍCMD1,¶ÌÏìÓ¦	 
//			SDIO_CmdInitStructure.SDIO_Argument = SD_VOLTAGE_WINDOW_MMC;
//			SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SEND_OP_COND;
//			SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
//			SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//			SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//			SDIO_SendCommand(&SDIO_CmdInitStructure);
			errorstatus = CmdResp3Error(); 									//Wait for an R3 response   
 			if(errorstatus != SD_OK)
				return errorstatus;   										//Response error
			
			response = SDIO_GetResponse(SDIO_RESP1);			   			//Response received
			validvoltage = (((response >> 31) == 1) ? 1 : 0);
			count++;
		}
		
		if(count >= SD_MAX_VOLT_TRIAL)
		{
			errorstatus = SD_INVALID_VOLTRANGE;
			return errorstatus;
		}
		
		CardType = SDIO_MULTIMEDIA_CARD;	  
  	}
	
	return errorstatus;
}

//SD¿¨ Power OFF
//Return: error code (0 = no error)
SD_Error SD_PowerOFF(void)
{
	SDIO->POWER &= ~(3 << 0);	//SDIOµçÔ´¹Ø±Õ,Ê±ÖÓÍ£Ö¹	
	/*!< Set Power State to OFF */
//	SDIO_SetPowerState(SDIO_PowerState_OFF);
	return SD_OK;		  
}

//³õÊ¼»¯ËùÓÐµÄ¿¨,²¢ÈÃ¿¨½øÈë¾ÍÐ÷×´Ì¬
//Return: error code
SD_Error SD_InitializeCards(void)
{
 	SD_Error errorstatus = SD_OK;
	u16 rca = 0x01;
	
 	if(SDIO_GetPowerState() == SDIO_PowerState_OFF)
		return SD_REQUEST_NOT_APPLICABLE;				//¼ì²éµçÔ´×´Ì¬,È·±£ÎªÉÏµç×´Ì¬
	
 	if(SDIO_SECURE_DIGITAL_IO_CARD != CardType)			//Not a SECURE_DIGITAL_IO_CARD
	{
		SDIO_Send_Cmd(SD_CMD_ALL_SEND_CID, 3, 0);		//·¢ËÍCMD2,È¡µÃCID,³¤ÏìÓ¦	 
//		/*!< Send CMD2 ALL_SEND_CID */
//		SDIO_CmdInitStructure.SDIO_Argument = 0x0;
//		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_ALL_SEND_CID;
//		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Long;
//		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//		SDIO_SendCommand(&SDIO_CmdInitStructure);
		errorstatus = CmdResp2Error(); 					//Wait for an R2 response   
		if(errorstatus != SD_OK)
			return errorstatus;   						//Response error
	    
		CID_Tab[0] = SDIO_GetResponse(SDIO_RESP1);
		CID_Tab[1] = SDIO_GetResponse(SDIO_RESP2);
		CID_Tab[2] = SDIO_GetResponse(SDIO_RESP3);
		CID_Tab[3] = SDIO_GetResponse(SDIO_RESP4);
	}
	
	//ÅÐ¶Ï¿¨ÀàÐÍ
	if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_SECURE_DIGITAL_IO_COMBO_CARD == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
	{
		SDIO_Send_Cmd(SD_CMD_SET_REL_ADDR, 1, 0);					//Send CMD3, short response 
//		/*!< Send CMD3 SET_REL_ADDR with argument 0 */
//		/*!< SD Card publishes its RCA. */
//		SDIO_CmdInitStructure.SDIO_Argument = 0x00;
//		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_REL_ADDR;
//		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
//		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//		SDIO_SendCommand(&SDIO_CmdInitStructure);
		errorstatus = CmdResp6Error(SD_CMD_SET_REL_ADDR, &rca);		//µÈ´ýR6ÏìÓ¦ 
		if(errorstatus != SD_OK)
			return errorstatus;   									//Response error		    
	}
	
    if(SDIO_MULTIMEDIA_CARD == CardType)
    {
 		SDIO_Send_Cmd(SD_CMD_SET_REL_ADDR, 1, (u32)(rca << 16));	//Send CMD3, short response 	   
//		/*!< Send CMD3 SET_REL_ADDR with argument 0 */
//		/*!< SD Card publishes its RCA. */
//		SDIO_CmdInitStructure.SDIO_Argument = (u32)(rca << 16);
//		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_REL_ADDR;
//		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
//		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//		SDIO_SendCommand(&SDIO_CmdInitStructure);
		errorstatus = CmdResp2Error(); 								//Wait for an R2 response   
		if(errorstatus != SD_OK)
			return errorstatus;   									//Response error	 
    }
	
	if(SDIO_SECURE_DIGITAL_IO_CARD != CardType)						//Not a SECURE_DIGITAL_IO_CARD
	{
		RCA = rca;
		SDIO_Send_Cmd(SD_CMD_SEND_CSD, 3, (u32)(rca << 16));		//·¢ËÍCMD9+¿¨RCA,È¡µÃCSD,³¤ÏìÓ¦ 	   
//		/*!< Send CMD9 SEND_CSD with argument as card's RCA */
//		SDIO_CmdInitStructure.SDIO_Argument = (uint32_t)(rca << 16);
//		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SEND_CSD;
//		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Long;
//		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
//		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
//		SDIO_SendCommand(&SDIO_CmdInitStructure);
		errorstatus = CmdResp2Error(); 								//Wait for an R2 response   
		if(errorstatus != SD_OK)
			return errorstatus;   									//Response error	
	    
		CSD_Tab[0] = SDIO_GetResponse(SDIO_RESP1);
		CSD_Tab[1] = SDIO_GetResponse(SDIO_RESP2);
		CSD_Tab[2] = SDIO_GetResponse(SDIO_RESP3);
		CSD_Tab[3] = SDIO_GetResponse(SDIO_RESP4);					    
	}
	
	return SD_OK;//¿¨³õÊ¼»¯³É¹¦
}

//µÃµ½¿¨ÐÅÏ¢
//cardinfo:¿¨ÐÅÏ¢´æ´¢Çø
//Return: error status
SD_Error SD_GetCardInfo(SD_CardInfo *cardinfo)
{
 	SD_Error errorstatus = SD_OK;
	u8 tmp = 0;
	
	cardinfo->CardType = (u8)CardType; 						//¿¨ÀàÐÍ
	cardinfo->RCA = (u16)RCA;								//¿¨RCAÖµ
	/*!< Byte 0 */
	tmp = (u8)((CSD_Tab[0] & 0xFF000000) >> 24);
	cardinfo->SD_csd.CSDStruct = (tmp & 0xC0) >> 6;			//CSD½á¹¹
	cardinfo->SD_csd.SysSpecVersion = (tmp & 0x3C) >> 2;	//2.0Ð­Òé»¹Ã»¶¨ÒåÕâ²¿·Ö(Îª±£Áô),Ó¦¸ÃÊÇºóÐøÐ­Òé¶¨ÒåµÄ
	cardinfo->SD_csd.Reserved1 = tmp & 0x03;				//2¸ö±£ÁôÎ»
	/*!< Byte 1 */
	tmp = (u8)((CSD_Tab[0] & 0x00FF0000) >> 16);			//Byte 1
	cardinfo->SD_csd.TAAC = tmp;				   			//Êý¾Ý¶ÁÊ±¼ä1
	/*!< Byte 2 */
	tmp = (u8)((CSD_Tab[0] & 0x0000FF00) >> 8);	  			//Byte 2
	cardinfo->SD_csd.NSAC = tmp;		  					//Êý¾Ý¶ÁÊ±¼ä2
	/*!< Byte 3 */
	tmp = (u8)(CSD_Tab[0] & 0x000000FF);					//Byte 3
	cardinfo->SD_csd.MaxBusClkFrec = tmp;		  			//´«ÊäËÙ¶È	
	/*!< Byte 4 */
	tmp = (u8)((CSD_Tab[1] & 0xFF000000) >> 24);			//Byte 4
	cardinfo->SD_csd.CardComdClasses = tmp << 4;    		//¿¨Ö¸ÁîÀà¸ßËÄÎ»
	/*!< Byte 5 */
	tmp = (u8)((CSD_Tab[1] & 0x00FF0000) >> 16);	 		//Byte 5
	cardinfo->SD_csd.CardComdClasses |= (tmp & 0xF0) >> 4;	//¿¨Ö¸ÁîÀàµÍËÄÎ»
	cardinfo->SD_csd.RdBlockLen = tmp & 0x0F;	    		//×î´ó¶ÁÈ¡Êý¾Ý³¤¶È
	/*!< Byte 6 */
	tmp = (u8)((CSD_Tab[1] & 0x0000FF00) >> 8);				//Byte 6
	cardinfo->SD_csd.PartBlockRead = (tmp & 0x80) >> 7;		//ÔÊÐí·Ö¿é¶Á
	cardinfo->SD_csd.WrBlockMisalign = (tmp & 0x40) >> 6;	//Ð´¿é´íÎ»
	cardinfo->SD_csd.RdBlockMisalign = (tmp & 0x20) >> 5;	//¶Á¿é´íÎ»
	cardinfo->SD_csd.DSRImpl = (tmp & 0x10) >> 4;
	cardinfo->SD_csd.Reserved2 = 0; 						//±£Áô
	
 	if((CardType == SDIO_STD_CAPACITY_SD_CARD_V1_1) || (CardType == SDIO_STD_CAPACITY_SD_CARD_V2_0) || (SDIO_MULTIMEDIA_CARD == CardType))//±ê×¼1.1/2.0¿¨/MMC¿¨
	{
		cardinfo->SD_csd.DeviceSize = (tmp & 0x03) << 10;						//C_SIZE(12Î»)
		/*!< Byte 7 */
	 	tmp = (u8)(CSD_Tab[1] & 0x000000FF); 									//Byte 7	
		cardinfo->SD_csd.DeviceSize |= (tmp) << 2;
		/*!< Byte 8 */
 		tmp = (u8)((CSD_Tab[2] & 0xFF000000) >> 24);							//Byte 8	
		cardinfo->SD_csd.DeviceSize |= (tmp & 0xC0) >> 6;
 		cardinfo->SD_csd.MaxRdCurrentVDDMin = (tmp & 0x38) >> 3;
		cardinfo->SD_csd.MaxRdCurrentVDDMax = (tmp & 0x07);
		/*!< Byte 9 */
 		tmp = (u8)((CSD_Tab[2]&0x00FF0000) >> 16);								//Byte 9	
		cardinfo->SD_csd.MaxWrCurrentVDDMin = (tmp & 0xE0) >> 5;
		cardinfo->SD_csd.MaxWrCurrentVDDMax = (tmp & 0x1C) >> 2;
		cardinfo->SD_csd.DeviceSizeMul = (tmp & 0x03) << 1;						//C_SIZE_MULT
		/*!< Byte 10 */
 		tmp = (u8)((CSD_Tab[2] & 0x0000FF00) >> 8);	  							//Byte 10	
		cardinfo->SD_csd.DeviceSizeMul |= (tmp & 0x80) >> 7;
 		cardinfo->CardCapacity = (cardinfo->SD_csd.DeviceSize + 1);				//Work out the card capacity
		cardinfo->CardCapacity *= (1 << (cardinfo->SD_csd.DeviceSizeMul + 2));
		cardinfo->CardBlockSize = 1 << (cardinfo->SD_csd.RdBlockLen);			//¿é´óÐ¡
		cardinfo->CardCapacity *= cardinfo->CardBlockSize;
	}
	else if(CardType == SDIO_HIGH_CAPACITY_SD_CARD)								//High capacity card
	{
		/*!< Byte 7 */
 		tmp = (u8)(CSD_Tab[1] & 0x000000FF); 									//Byte 7	
		cardinfo->SD_csd.DeviceSize = (tmp & 0x3F) << 16;						//C_SIZE
		/*!< Byte 8 */
 		tmp = (u8)((CSD_Tab[2] & 0xFF000000) >> 24); 							//Byte 8	
 		cardinfo->SD_csd.DeviceSize |= (tmp << 8);
		/*!< Byte 9 */
 		tmp = (u8)((CSD_Tab[2] & 0x00FF0000) >> 16);							//Byte 9	
 		cardinfo->SD_csd.DeviceSize |= (tmp);
		/*!< Byte 10 */
 		tmp = (u8)((CSD_Tab[2] & 0x0000FF00) >> 8); 							//Byte 10	
 		cardinfo->CardCapacity = (long long)(cardinfo->SD_csd.DeviceSize + 1) * 512 * 1024;	//Work out the card capacity
		cardinfo->CardBlockSize = 512; 											//¿é´óÐ¡¹Ì¶¨Îª512×Ö½Ú
	}	  
	cardinfo->SD_csd.EraseGrSize = (tmp & 0x40) >> 6;
	cardinfo->SD_csd.EraseGrMul = (tmp & 0x3F) << 1;	   
	/*!< Byte 11 */
	tmp = (u8)(CSD_Tab[2] & 0x000000FF);										//Byte 11	
	cardinfo->SD_csd.EraseGrMul |= (tmp & 0x80) >> 7;
	cardinfo->SD_csd.WrProtectGrSize = (tmp & 0x7F);
	/*!< Byte 12 */
 	tmp = (u8)((CSD_Tab[3] & 0xFF000000) >> 24);								//Byte 12	
	cardinfo->SD_csd.WrProtectGrEnable = (tmp & 0x80) >> 7;
	cardinfo->SD_csd.ManDeflECC = (tmp & 0x60) >> 5;
	cardinfo->SD_csd.WrSpeedFact = (tmp & 0x1C) >> 2;
	cardinfo->SD_csd.MaxWrBlockLen = (tmp & 0x03) << 2;	 
	/*!< Byte 13 */
	tmp = (u8)((CSD_Tab[3] & 0x00FF0000) >> 16);								//Byte 13
	cardinfo->SD_csd.MaxWrBlockLen |= (tmp & 0xC0) >> 6;
	cardinfo->SD_csd.WriteBlockPaPartial = (tmp & 0x20) >> 5;
	cardinfo->SD_csd.Reserved3 = 0;
	cardinfo->SD_csd.ContentProtectAppli = (tmp & 0x01);  
	/*!< Byte 14 */
	tmp = (u8)((CSD_Tab[3] & 0x0000FF00) >> 8);									//Byte 14
	cardinfo->SD_csd.FileFormatGrouop = (tmp & 0x80) >> 7;
	cardinfo->SD_csd.CopyFlag = (tmp & 0x40) >> 6;
	cardinfo->SD_csd.PermWrProtect = (tmp & 0x20) >> 5;
	cardinfo->SD_csd.TempWrProtect = (tmp & 0x10) >> 4;
	cardinfo->SD_csd.FileFormat = (tmp & 0x0C) >> 2;
	cardinfo->SD_csd.ECC = (tmp & 0x03);  
	/*!< Byte 15 */
	tmp = (u8)(CSD_Tab[3] & 0x000000FF);										//Byte 15
	cardinfo->SD_csd.CSD_CRC = (tmp & 0xFE) >> 1;
	cardinfo->SD_csd.Reserved4 = 1;		 


	// CID
	/*!< Byte 0 */
	tmp = (u8)((CID_Tab[0] & 0xFF000000) >> 24);								//µÚ0¸ö×Ö½Ú
	cardinfo->SD_cid.ManufacturerID = tmp;		    
	/*!< Byte 1 */
	tmp = (u8)((CID_Tab[0] & 0x00FF0000) >> 16);								//Byte 1
	cardinfo->SD_cid.OEM_AppliID = tmp << 8;	  
	/*!< Byte 2 */
	tmp = (u8)((CID_Tab[0] & 0x000000FF00) >> 8);								//Byte 2
	cardinfo->SD_cid.OEM_AppliID |= tmp;	    
	/*!< Byte 3 */
	tmp = (u8)(CID_Tab[0] & 0x000000FF);										//Byte 3	
	cardinfo->SD_cid.ProdName1 = tmp << 24;				  
	/*!< Byte 4 */
	tmp = (u8)((CID_Tab[1] & 0xFF000000) >> 24); 								//Byte 4
	cardinfo->SD_cid.ProdName1 |= tmp << 16;	  
	/*!< Byte 5 */
	tmp = (u8)((CID_Tab[1] & 0x00FF0000) >> 16);	   							//Byte 5
	cardinfo->SD_cid.ProdName1 |= tmp << 8;		 
	/*!< Byte 6 */
	tmp = (u8)((CID_Tab[1] & 0x0000FF00) >> 8);									//Byte 6
	cardinfo->SD_cid.ProdName1 |= tmp;		   
	/*!< Byte 7 */
	tmp = (u8)(CID_Tab[1] & 0x000000FF);	  									//Byte 7
	cardinfo->SD_cid.ProdName2 = tmp;			  
	/*!< Byte 8 */
	tmp = (u8)((CID_Tab[2] & 0xFF000000) >> 24); 								//Byte 8
	cardinfo->SD_cid.ProdRev = tmp;		 
	/*!< Byte 9 */
	tmp = (u8)((CID_Tab[2] & 0x00FF0000) >> 16);								//Byte 9
	cardinfo->SD_cid.ProdSN = tmp << 24;	   
	/*!< Byte 10 */
	tmp = (u8)((CID_Tab[2] & 0x0000FF00) >> 8); 								//Byte 10
	cardinfo->SD_cid.ProdSN |= tmp << 16;	   
	/*!< Byte 11 */
	tmp = (u8)(CID_Tab[2] & 0x000000FF);   										//Byte 11
	cardinfo->SD_cid.ProdSN |= tmp << 8;		   
	/*!< Byte 12 */
	tmp = (u8)((CID_Tab[3] & 0xFF000000) >> 24); 								//Byte 12
	cardinfo->SD_cid.ProdSN |= tmp;			     
	/*!< Byte 13 */
	tmp = (u8)((CID_Tab[3] & 0x00FF0000) >> 16);	 							//Byte 13
	cardinfo->SD_cid.Reserved1 |= (tmp & 0xF0) >> 4;
	cardinfo->SD_cid.ManufactDate = (tmp & 0x0F) << 8;    
	/*!< Byte 14 */
	tmp = (u8)((CID_Tab[3] & 0x0000FF00) >> 8);									//Byte 14
	cardinfo->SD_cid.ManufactDate |= tmp;		 	  
	/*!< Byte 15 */
	tmp = (u8)(CID_Tab[3] & 0x000000FF);										//Byte 15
	cardinfo->SD_cid.CID_CRC=(tmp & 0xFE) >> 1;
	cardinfo->SD_cid.Reserved2 = 1;
	 
	return errorstatus;
}

//SDIOÊ¹ÄÜ¿í×ÜÏßÄ£Ê½
//enx:0,²»Ê¹ÄÜ;1,Ê¹ÄÜ;
//Return: error status
SD_Error SDEnWideBus(u8 enx)
{
	SD_Error errorstatus = SD_OK;
 	u32 scr[2] = {0, 0};
	u8 arg = 0X00;
	
	if(enx)
		arg = 0X02;
	else
		arg = 0X00;
	
 	if(SDIO->RESP1 & SD_CARD_LOCKED)
		return SD_LOCK_UNLOCK_FAILED;						//SD¿¨´¦ÓÚLOCKED×´Ì¬
	
 	errorstatus = FindSCR(RCA, scr);						//µÃµ½SCR¼Ä´æÆ÷Êý¾Ý
 	if(errorstatus != SD_OK)
		return errorstatus;
	
	if((scr[1] & SD_WIDE_BUS_SUPPORT) != SD_ALLZERO)		//Ö§³Ö¿í×ÜÏß
	{
	 	SDIO_Send_Cmd(SD_CMD_APP_CMD, 1, (u32)RCA << 16);	//·¢ËÍCMD55+RCA,¶ÌÏìÓ¦											  
	 	errorstatus = CmdResp1Error(SD_CMD_APP_CMD);
	 	if(errorstatus != SD_OK)
			return errorstatus;
		
	 	SDIO_Send_Cmd(SD_CMD_APP_SD_SET_BUSWIDTH, 1, arg);	//·¢ËÍACMD6,¶ÌÏìÓ¦,²ÎÊý:10,4Î»;00,1Î».											  
		errorstatus = CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);
		return errorstatus;
	}
	else
	{
		return SD_REQUEST_NOT_APPLICABLE;				//²»Ö§³Ö¿í×ÜÏßÉèÖÃ
	}		
}												   

//ÉèÖÃSDIO×ÜÏß¿í¶È(MMC¿¨²»Ö§³Ö4bitÄ£Ê½)
//wmode:Î»¿íÄ£Ê½.0,1Î»Êý¾Ý¿í¶È;1,4Î»Êý¾Ý¿í¶È;2,8Î»Êý¾Ý¿í¶È
//·µ»ØÖµ:SD¿¨´íÎó×´Ì¬
SD_Error SD_EnableWideBusOperation(u32 wmode)
{
  	SD_Error errorstatus = SD_OK;
	
 	if(SDIO_MULTIMEDIA_CARD == CardType)
	{
		return SD_UNSUPPORTED_FEATURE;				//MMC¿¨²»Ö§³Ö
	}
 	else if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
	{
		if(SDIO_BusWide_8b == wmode)
		{
			return SD_UNSUPPORTED_FEATURE;			//²»Ö§³Ö8Î»Ä£Ê½
		}
 		else   
		{
			errorstatus = SDEnWideBus(ENABLE);
 			if(SD_OK == errorstatus)
			{
				/*!< Configure the SDIO peripheral */
				SDIO_InitStructure.SDIO_ClockDiv = SDIO_TRANSFER_CLK_DIV; 
				SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
				SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
				SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
				SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_4b;
				SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
				SDIO_Init(&SDIO_InitStructure);			//²»¿ªÆôÓ²¼þÁ÷¿ØÖÆ
			}
		}  
	}
	else
	{
		errorstatus = SDEnWideBus(DISABLE);
		if(SD_OK == errorstatus)
		{
			/*!< Configure the SDIO peripheral */
			SDIO_InitStructure.SDIO_ClockDiv = SDIO_TRANSFER_CLK_DIV; 
			SDIO_InitStructure.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
			SDIO_InitStructure.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
			SDIO_InitStructure.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
			SDIO_InitStructure.SDIO_BusWide = SDIO_BusWide_1b;
			SDIO_InitStructure.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
			SDIO_Init(&SDIO_InitStructure);
		}
	}
	
	return errorstatus; 
}

//ÉèÖÃSD¿¨¹¤×÷Ä£Ê½
//Mode:
//Return: error status
SD_Error SD_SetDeviceMode(u32 Mode)
{
	SD_Error errorstatus = SD_OK;
 	if((Mode == SD_DMA_MODE) || (Mode == SD_POLLING_MODE))
		DeviceMode = Mode;
	else
		errorstatus = SD_INVALID_PARAMETER;
	
	return errorstatus;	    
}

//Ñ¡¿¨
//·¢ËÍCMD7,Ñ¡ÔñÏà¶ÔµØÖ·(rca)ÎªaddrµÄ¿¨,È¡ÏûÆäËû¿¨.Èç¹ûÎª0,Ôò¶¼²»Ñ¡Ôñ.
//addr:¿¨µÄRCAµØÖ·
SD_Error SD_SelectDeselect(u32 addr)
{
// 	SDIO_Send_Cmd(SD_CMD_SEL_DESEL_CARD, 1, addr);	//·¢ËÍCMD7,Ñ¡Ôñ¿¨,¶ÌÏìÓ¦	 	   
	/*!< Send CMD7 SDIO_SEL_DESEL_CARD */
	SDIO_CmdInitStructure.SDIO_Argument = addr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SEL_DESEL_CARD;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);
   	return CmdResp1Error(SD_CMD_SEL_DESEL_CARD);	  
}

//µÃµ½NumberOfBytesÒÔ2Îªµ×µÄÖ¸Êý.
//NumberOfBytes:×Ö½ÚÊý.
//·µ»ØÖµ:ÒÔ2Îªµ×µÄÖ¸ÊýÖµ
u8 convert_from_bytes_to_power_of_two(u16 NumberOfBytes)
{
	u8 count = 0;
	while(NumberOfBytes != 1)
	{
		NumberOfBytes >>= 1;
		count++;
	}
	return count;
} 

//SD¿¨¶ÁÈ¡Ò»¸ö¿é 
//buf:¶ÁÊý¾Ý»º´æÇø(±ØÐë4×Ö½Ú¶ÔÆë!!)
//addr: address to read
//blksize: block size
SD_Error SD_ReadBlock(u8 *buf, u32 addr, u16 blksize)
{	  
	SD_Error errorstatus = SD_OK;
	u8 power;
   	u32 count = 0, *tempbuff = (u32*)buf;	//Cast to a u32 pointer 
	u32 timeout = 0;
	
   	if(NULL == buf)
		return SD_INVALID_PARAMETER;
	
   	SDIO->DCTRL = 0x0;								//Clear the data control register (DMA off)   
	if(CardType == SDIO_HIGH_CAPACITY_SD_CARD)		//High capacity card
	{
		blksize = 512;
		addr >>= 9;
	}
	   
//	SDIO_Send_Data_Cfg(SD_DATATIMEOUT, 0, 0, 0);	//Çå³ýDPSM×´Ì¬»úÅäÖÃ
	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = 0;
	SDIO_DataInitStructure.SDIO_DataBlockSize = 0;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToCard;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);
	if(SDIO->RESP1 & SD_CARD_LOCKED)
		return SD_LOCK_UNLOCK_FAILED;				//The card is locked
	
	if((blksize > 0) && (blksize <= 2048) && ((blksize & (blksize - 1)) == 0))
	{
		power = convert_from_bytes_to_power_of_two(blksize);	    	   
//		SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN, 1, blksize);		//·¢ËÍCMD16+ÉèÖÃÊý¾Ý³¤¶ÈÎªblksize,¶ÌÏìÓ¦ 	   
		SDIO_CmdInitStructure.SDIO_Argument = blksize;
		SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_SET_BLOCKLEN;
		SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
		SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
		SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
		SDIO_SendCommand(&SDIO_CmdInitStructure);
		errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);	//Wait for an R1 response   
		if(errorstatus != SD_OK)
			return errorstatus;   							//Response error	 
	}
	else
	{
		return SD_INVALID_PARAMETER;	
	}
	
//	SDIO_Send_Data_Cfg(SD_DATATIMEOUT, blksize, power, 1);	//blksize,¿¨µ½¿ØÖÆÆ÷	  
	SDIO_DataInitStructure.SDIO_DataTimeOut = SD_DATATIMEOUT;
	SDIO_DataInitStructure.SDIO_DataLength = blksize;
	SDIO_DataInitStructure.SDIO_DataBlockSize = (u32) power << 4;
	SDIO_DataInitStructure.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	SDIO_DataInitStructure.SDIO_TransferMode = SDIO_TransferMode_Block;
	SDIO_DataInitStructure.SDIO_DPSM = SDIO_DPSM_Enable;
	SDIO_DataConfig(&SDIO_DataInitStructure);

//	SDIO_Send_Cmd(SD_CMD_READ_SINGLE_BLOCK, 1, addr);		//·¢ËÍCMD17+´ÓaddrµØÖ·³ö¶ÁÈ¡Êý¾Ý,¶ÌÏìÓ¦ 	   
	/*!< Send CMD17 READ_SINGLE_BLOCK */
	SDIO_CmdInitStructure.SDIO_Argument = (u32)addr;
	SDIO_CmdInitStructure.SDIO_CmdIndex = SD_CMD_READ_SINGLE_BLOCK;
	SDIO_CmdInitStructure.SDIO_Response = SDIO_Response_Short;
	SDIO_CmdInitStructure.SDIO_Wait = SDIO_Wait_No;
	SDIO_CmdInitStructure.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&SDIO_CmdInitStructure);
	errorstatus = CmdResp1Error(SD_CMD_READ_SINGLE_BLOCK);	//Wait for an R1 response   
	if(errorstatus != SD_OK)
		return errorstatus;   								//Response error
	
	if(DeviceMode == SD_POLLING_MODE)						//²éÑ¯Ä£Ê½,ÂÖÑ¯Êý¾Ý	 
	{
//		while(!(SDIO->STA & ((1 << 5) | (1 << 1) | (1 << 3) | (1 << 10) | (1 << 9))))	//ÎÞÉÏÒç/CRC/³¬Ê±/Íê³É(±êÖ¾)/ÆðÊ¼Î»´íÎó
//		{
//			if(SDIO->STA & (1 << 15))						//½ÓÊÕÇø°ëÂú,±íÊ¾ÖÁÉÙ´æÁË8¸ö×Ö
//			{
//				for(count = 0; count < 8; count++)			//Ñ­»·¶ÁÈ¡Êý¾Ý
//				{
//					*(tempbuff + count) = SDIO->FIFO;	 
//				}
//				tempbuff += 8;
//			}
//		}
//	
//		if(SDIO->STA & (1 << 3))		//Êý¾Ý³¬Ê±´íÎó
//		{										   
//	 		SDIO->ICR |= 1 << 3; 		//Çå´íÎó±êÖ¾
//			return SD_DATA_TIMEOUT;
//	 	}
//		else if(SDIO->STA & (1 << 1))	//Êý¾Ý¿éCRC´íÎó
//		{
//	 		SDIO->ICR |= 1 << 1; 		//Çå´íÎó±êÖ¾
//			return SD_DATA_CRC_FAIL;		   
//		}
//		else if(SDIO->STA & (1 << 5)) 	//½ÓÊÕfifoÉÏÒç´íÎó
//		{
//	 		SDIO->ICR |= 1 << 5; 		//Çå´íÎó±êÖ¾
//			return SD_RX_OVERRUN;		 
//		}
//		else if(SDIO->STA & (1 << 9)) 	//½ÓÊÕÆðÊ¼Î»´íÎó
//		{
//	 		SDIO->ICR |= 1 << 9; 		//Çå´íÎó±êÖ¾
//			return SD_START_BIT_ERR;		 
//		}
//		
//		while(SDIO->STA & (1 << 21))	//FIFOÀïÃæ,»¹´æÔÚ¿ÉÓÃÊý¾Ý
//		{
//			*tempbuff = SDIO->FIFO;		//Ñ­»·¶ÁÈ¡Êý¾Ý
//			tempbuff++;
//		}
//		SDIO->ICR = 0X5FF;	 			//Çå³ýËùÓÐ±ê¼Ç
/*!< In case of single block transfer, no need of stop transfer at all.*/
  /*!< Polling mode */
		while (!(SDIO->STA &(SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND | SDIO_FLAG_STBITERR)))
		{
			if (SDIO_GetFlagStatus(SDIO_FLAG_RXFIFOHF) != RESET)
			{
				for (count = 0; count < 8; count++)
				{
					*(tempbuff + count) = SDIO_ReadData();
				}
				tempbuff += 8;
			}
		}

		if (SDIO_GetFlagStatus(SDIO_FLAG_DTIMEOUT) != RESET)
		{
			SDIO_ClearFlag(SDIO_FLAG_DTIMEOUT);
			return SD_DATA_TIMEOUT;
		}
		else if (SDIO_GetFlagStatus(SDIO_FLAG_DCRCFAIL) != RESET)
		{
			SDIO_ClearFlag(SDIO_FLAG_DCRCFAIL);
			return SD_DATA_CRC_FAIL;
		}
		else if (SDIO_GetFlagStatus(SDIO_FLAG_RXOVERR) != RESET)
		{
			SDIO_ClearFlag(SDIO_FLAG_RXOVERR);
			return SD_RX_OVERRUN;
		}
		else if (SDIO_GetFlagStatus(SDIO_FLAG_STBITERR) != RESET)
		{
			SDIO_ClearFlag(SDIO_FLAG_STBITERR);
			return SD_START_BIT_ERR;
		}

		while (SDIO_GetFlagStatus(SDIO_FLAG_RXDAVL) != RESET)
		{
			*tempbuff = SDIO_ReadData();
			tempbuff++;
		}

		/*!< Clear all the static flags */
		SDIO_ClearFlag(SDIO_STATIC_FLAGS);
	}
	else if(DeviceMode == SD_DMA_MODE)
	{
 		TransferError = SD_OK;
		StopCondition = 0;				//µ¥¿é¶Á,²»ÐèÒª·¢ËÍÍ£Ö¹´«ÊäÖ¸Áî
		TransferEnd = 0;				//Transfer-complete flag, set to 1 in the interrupt handler
		SDIO->MASK |= (1 << 1) | (1 << 3) | (1 << 8) | (1 << 5) | (1 << 9);	//Enable the interrupts that are needed 
	 	SDIO->DCTRL |= 1 << 3;		 	//Enable SDIO DMA 
 	    SD_DMA_Config((u32*)buf, blksize, 0);
		timeout = SDIO_DATATIMEOUT;
 		while(((DMA2->ISR & 0X2000) == RESET) && (TransferEnd == 0) && (TransferError == SD_OK) && timeout)
		{
			timeout--;					//Wait for the transfer to finish 
		}
		if(timeout == 0)
			return SD_DATA_TIMEOUT;		//Timeout
		
		if(TransferError != SD_OK)
			errorstatus = TransferError;  
    }   
 	return errorstatus; 
}

//SD¿¨¶ÁÈ¡¶à¸ö¿é 
//buf: read data buffer
//addr: address to read
//blksize: block size
//nblks:Òª¶ÁÈ¡µÄ¿éÊý
//Return: error status
SD_Error SD_ReadMultiBlocks(u8 *buf, u32 addr, u16 blksize, u32 nblks)
{
  	SD_Error errorstatus = SD_OK;
	u8 power;
   	u32 count = 0, *tempbuff = (u32*)buf;			//Cast to a u32 pointer
	u32 timeout = 0;  
    SDIO->DCTRL = 0x0;								//Clear the data control register (DMA off)
	
	if(CardType == SDIO_HIGH_CAPACITY_SD_CARD)		//High capacity card
	{
		blksize = 512;
		addr >>= 9;
	}
	
   	SDIO_Send_Data_Cfg(SD_DATATIMEOUT, 0, 0, 0);	//Clear the DPSM state machine configuration
	if(SDIO->RESP1 & SD_CARD_LOCKED)
		return SD_LOCK_UNLOCK_FAILED;				//The card is locked
	
	if((blksize > 0) && (blksize <= 2048) && ((blksize & (blksize - 1)) == 0))
	{
		power = convert_from_bytes_to_power_of_two(blksize);	    
		SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN, 1, blksize);		//Send CMD16 and set the data length to blksize, short response 	   
		errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);	//Wait for an R1 response   
		if(errorstatus != SD_OK)
			return errorstatus;   							//Response error	 
	}
	else
	{
		return SD_INVALID_PARAMETER;	  
	}
	
	if(nblks > 1)											//¶à¿é¶Á  
	{									    
 	  	if(nblks * blksize > SD_MAX_DATA_LENGTH)
			return SD_INVALID_PARAMETER;					//ÅÐ¶ÏÊÇ·ñ³¬¹ý×î´ó½ÓÊÕ³¤¶È
		
		SDIO_Send_Data_Cfg(SD_DATATIMEOUT, nblks * blksize, power, 1);	//nblks*blksize,512¿é´óÐ¡,¿¨µ½¿ØÖÆÆ÷	  
	  	SDIO_Send_Cmd(SD_CMD_READ_MULT_BLOCK, 1, addr);		//·¢ËÍCMD18+´ÓaddrµØÖ·³ö¶ÁÈ¡Êý¾Ý,¶ÌÏìÓ¦ 	   
		errorstatus = CmdResp1Error(SD_CMD_READ_MULT_BLOCK);//Wait for an R1 response   
		if(errorstatus != SD_OK)
			return errorstatus;   							//Response error	  
		
		if(DeviceMode == SD_POLLING_MODE)
		{
			while(!(SDIO->STA & ((1 << 5) | (1 << 1) | (1 << 3) | (1 << 8) | (1 << 9))))//ÎÞÉÏÒç/CRC/³¬Ê±/Íê³É(±êÖ¾)/ÆðÊ¼Î»´íÎó
			{
				if(SDIO->STA & (1 << 15))					//½ÓÊÕÇø°ëÂú,±íÊ¾ÖÁÉÙ´æÁË8¸ö×Ö
				{
					for(count = 0; count < 8; count++)		//Read the data in a loop
					{
						*(tempbuff + count) = SDIO->FIFO;	 
					}
					tempbuff += 8;
				}
			}
			
			if(SDIO->STA & (1 << 3))		//Data timeout error
			{										   
		 		SDIO->ICR |= 1 << 3; 		//Clear the error flags
				return SD_DATA_TIMEOUT;
		 	}
			else if(SDIO->STA & (1 << 1))	//Data block CRC error
			{
		 		SDIO->ICR |= 1 << 1; 		//Clear the error flags
				return SD_DATA_CRC_FAIL;		   
			}
			else if(SDIO->STA & (1 << 5)) 	//½ÓÊÕfifoÉÏÒç´íÎó
			{
		 		SDIO->ICR |= 1 << 5; 		//Clear the error flags
				return SD_RX_OVERRUN;		 
			}
			else if(SDIO->STA & (1 << 9)) 	//Receive start-bit error
			{
		 		SDIO->ICR |= 1 << 9; 		//Clear the error flags
				return SD_START_BIT_ERR;		 
			}
			
			while(SDIO->STA & (1 << 21))	//FIFOÀïÃæ,»¹´æÔÚ¿ÉÓÃÊý¾Ý
			{
				*tempbuff = SDIO->FIFO;		//Read the data in a loop
				tempbuff++;
			}
			
	 		if(SDIO->STA & (1 << 8))		//½ÓÊÕ½áÊø
			{
				if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
				{
					SDIO_Send_Cmd(SD_CMD_STOP_TRANSMISSION, 1, 0);			//Send CMD12 to end the transfer 	   
					errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);	//Wait for an R1 response   
					if(errorstatus != SD_OK)
						return errorstatus;	 
				}
 			}
	 		SDIO->ICR = 0X5FF;	 											//Clear all flags 
 		}
		else if(DeviceMode == SD_DMA_MODE)
		{
	   		TransferError = SD_OK;
			StopCondition = 1;									//¶à¿é¶Á,ÐèÒª·¢ËÍÍ£Ö¹´«ÊäÖ¸Áî 
			TransferEnd = 0;									//Transfer-complete flag, set to 1 in the interrupt handler
			SDIO->MASK |= (1 << 1) | (1 << 3) | (1 << 8) | (1 << 5) | (1 << 9);	//Enable the interrupts that are needed 
		 	SDIO->DCTRL |= 1 << 3;		 						//Enable SDIO DMA 
	 	    SD_DMA_Config((u32*)buf, nblks * blksize, 0);
			timeout = SDIO_DATATIMEOUT;
	 		while(((DMA2->ISR & 0X2000) == RESET) && timeout)
			{
				timeout--;										//Wait for the transfer to finish 
			}
			
			if(timeout == 0)
				return SD_DATA_TIMEOUT;							//Timeout
			
			while((TransferEnd == 0) && (TransferError == SD_OK));
			
			if(TransferError != SD_OK)
				errorstatus = TransferError;  	 
		}		 
  	}
	return errorstatus;
}

//¼ì²é¿¨ÊÇ·ñÕýÔÚÖ´ÐÐÐ´²Ù×÷
//pstatus:µ±Ç°×´Ì¬.
//Return: error code
SD_Error IsCardProgramming(u8 *pstatus)
{
 	vu32 respR1 = 0, status = 0; 
	
  	SDIO_Send_Cmd(SD_CMD_SEND_STATUS, 1, (u32)RCA << 16);		//·¢ËÍCMD13 	   
  	status = SDIO->STA;
	while(!(status & ((1 << 0) | (1 << 6) | (1 << 2))))
		status = SDIO->STA;										//Wait for the operation to finish
	
   	if(status & (1 << 0))			//CRC¼ì²âÊ§°Ü
	{
		SDIO->ICR |= 1 << 0;		//Clear the error flags
		return SD_CMD_CRC_FAIL;
	}
	
   	if(status & (1 << 2))			//ÃüÁî³¬Ê± 
	{
		SDIO->ICR |= 1 << 2;		//Clear the error flags
		return SD_CMD_RSP_TIMEOUT;
	}
	
 	if(SDIO->RESPCMD != SD_CMD_SEND_STATUS)
		return SD_ILLEGAL_CMD;
	
	SDIO->ICR = 0X5FF;	 			//Clear all flags
	
	respR1 = SDIO->RESP1;
	*pstatus = (u8)((respR1 >> 9) & 0x0000000F);
	
	return SD_OK;
}

//SD¿¨Ð´1¸ö¿é 
//buf: data buffer
//addr: address to write
//blksize: block size	  
//Return: error status
SD_Error SD_WriteBlock(u8 *buf, u32 addr, u16 blksize)
{
	SD_Error errorstatus = SD_OK;
	u8 power = 0, cardstate = 0;
	u32 timeout = 0, bytestransferred = 0;
	u32 cardstatus = 0, count = 0, restwords = 0;
	u32	tlen = blksize;							//Total length in bytes
	u32 *tempbuff = (u32*)buf;								 
	
 	if(buf == NULL)
		return SD_INVALID_PARAMETER;			//Parameter error   
	
  	SDIO->DCTRL = 0x0;							//Clear the data control register (DMA off)   
  	SDIO_Send_Data_Cfg(SD_DATATIMEOUT, 0, 0, 0);//Clear the DPSM state machine configuration
	if(SDIO->RESP1 & SD_CARD_LOCKED)
		return SD_LOCK_UNLOCK_FAILED;			//The card is locked
	
 	if(CardType == SDIO_HIGH_CAPACITY_SD_CARD)	//High capacity card
	{
		blksize = 512;
		addr >>= 9;
	}
	
	if((blksize > 0) && (blksize <= 2048) && ((blksize & (blksize - 1)) == 0))
	{
		power = convert_from_bytes_to_power_of_two(blksize);	    
		SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN, 1, blksize);		//Send CMD16 and set the data length to blksize, short response 	   
		errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);	//Wait for an R1 response   
		if(errorstatus != SD_OK)
			return errorstatus;   							//Response error	 
	}
	else
	{
		return SD_INVALID_PARAMETER;	 
	}
	
   	SDIO_Send_Cmd(SD_CMD_SEND_STATUS, 1, (u32)RCA << 16);	//Send CMD13 to query the card status, short response 	   
	errorstatus = CmdResp1Error(SD_CMD_SEND_STATUS);		//Wait for an R1 response   		   
	if(errorstatus != SD_OK)
		return errorstatus;
	
	cardstatus = SDIO->RESP1;													  
	timeout = SD_DATATIMEOUT;
   	while(((cardstatus & 0x00000100) == 0) && (timeout > 0))//¼ì²éREADY_FOR_DATAÎ»ÊÇ·ñÖÃÎ»
	{
		timeout--;
	   	SDIO_Send_Cmd(SD_CMD_SEND_STATUS, 1, (u32)RCA << 16);//Send CMD13 to query the card status, short response 	   
		errorstatus = CmdResp1Error(SD_CMD_SEND_STATUS);	//Wait for an R1 response   		   
		if(errorstatus != SD_OK)
			return errorstatus;				    
		
		cardstatus = SDIO->RESP1;													  
	}
	if(timeout == 0)
		return SD_ERROR;
	
   	SDIO_Send_Cmd(SD_CMD_WRITE_SINGLE_BLOCK, 1, addr);		//·¢ËÍCMD24,Ð´µ¥¿éÖ¸Áî,¶ÌÏìÓ¦ 	   
	errorstatus = CmdResp1Error(SD_CMD_WRITE_SINGLE_BLOCK);	//Wait for an R1 response   		   
	if(errorstatus != SD_OK)
		return errorstatus;   	  
	
	StopCondition = 0;										//Single block write, so no stop-transfer command is needed 
 	SDIO_Send_Data_Cfg(SD_DATATIMEOUT, blksize, power, 0);	//blksize, controller to card	  
	if (DeviceMode == SD_POLLING_MODE)
	{
		while(!(SDIO->STA & ((1 << 10) | (1 << 4) | (1 << 1) | (1 << 3) | (1 << 9))))//Êý¾Ý¿é·¢ËÍ³É¹¦/ÏÂÒç/CRC/³¬Ê±/ÆðÊ¼Î»´íÎó
		{
			if(SDIO->STA & (1 << 14))							//·¢ËÍÇø°ë¿Õ,±íÊ¾ÖÁÉÙ´æÁË8¸ö×Ö
			{
				if((tlen - bytestransferred) < SD_HALFFIFOBYTES)//Fewer than 32 bytes left
				{
					restwords = ((tlen - bytestransferred) % 4 == 0) ? ((tlen - bytestransferred) / 4) : ((tlen - bytestransferred) / 4 + 1);
					
					for(count = 0; count < restwords; count++, tempbuff++, bytestransferred += 4)
					{
						SDIO->FIFO = *tempbuff;
					}
				}
				else
				{
					for(count = 0; count < 8; count++)
					{
						SDIO->FIFO = *(tempbuff + count);
					}
					tempbuff += 8;
					bytestransferred += 32;
				}
			}
		}
		
		if(SDIO->STA & (1 << 3))		//Data timeout error
		{										   
	 		SDIO->ICR |= 1 << 3; 		//Clear the error flags
			return SD_DATA_TIMEOUT;
	 	}
		else if(SDIO->STA & (1 << 1))	//Data block CRC error
		{
	 		SDIO->ICR |= 1 << 1; 		//Clear the error flags
			return SD_DATA_CRC_FAIL;		   
		}
		else if(SDIO->STA & (1 << 4)) 	//Receive FIFO underrun error
		{
	 		SDIO->ICR |= 1 << 4; 		//Clear the error flags
			return SD_TX_UNDERRUN;		 
		}
		else if(SDIO->STA & (1 << 9)) 	//Receive start-bit error
		{
	 		SDIO->ICR |= 1 << 9; 		//Clear the error flags
			return SD_START_BIT_ERR;		 
		}   
		SDIO->ICR = 0X5FF;	 			//Clear all flags	  
	}
	else if(DeviceMode == SD_DMA_MODE)
	{
   		TransferError = SD_OK;
		StopCondition = 0;				//Single block write, so no stop-transfer command is needed 
		TransferEnd = 0;				//Transfer-complete flag, set to 1 in the interrupt handler
		SDIO->MASK |= (1 << 1) | (1 << 3) | (1 << 8) | (1 << 4) | (1 << 9);	//Enable the data-received interrupt
		SD_DMA_Config((u32*)buf, blksize, 1);	//SDIO DMA configuration
 	 	SDIO->DCTRL |= 1 << 3;					//Enable SDIO DMA. 
		timeout = SDIO_DATATIMEOUT;
 		while(((DMA2->ISR & 0X2000) == RESET) && timeout)
		{
			timeout--;							//Wait for the transfer to finish 
		}
		if(timeout==0)
		{
  			SD_Init();	 						//Re-initialising the SD card clears the hang seen on writes
			return SD_DATA_TIMEOUT;				//Timeout	 
 		}
		
		timeout = SDIO_DATATIMEOUT;
		while((TransferEnd == 0) && (TransferError == SD_OK) && timeout)
		{
			timeout--;
		}
 		if(timeout == 0)
			return SD_DATA_TIMEOUT;				//Timeout
		
  		if(TransferError != SD_OK)
			return TransferError;
 	}
	
 	SDIO->ICR = 0X5FF;	 						//Clear all flags
 	errorstatus = IsCardProgramming(&cardstate);
 	while((errorstatus == SD_OK) && ((cardstate == SD_CARD_PROGRAMMING) || (cardstate == SD_CARD_RECEIVING)))
	{
		errorstatus = IsCardProgramming(&cardstate);
	}
	
	return errorstatus;
}
//SD¿¨Ð´¶à¸ö¿é 
//buf: data buffer
//addr: address to write
//blksize: block size
//nblks:ÒªÐ´ÈëµÄ¿éÊý
//Return: error status												   
SD_Error SD_WriteMultiBlocks(u8 *buf,u32 addr,u16 blksize,u32 nblks)
{
	SD_Error errorstatus = SD_OK;
	u8 power = 0, cardstate = 0;
	u32 timeout = 0, bytestransferred = 0;
	u32 count = 0, restwords = 0;
	u32 tlen = nblks * blksize;				//Total length in bytes
	u32 *tempbuff = (u32*)buf;  
  	if(buf == NULL)
		return SD_INVALID_PARAMETER;		//Parameter error  
	
  	SDIO->DCTRL = 0x0;								//Clear the data control register (DMA off)   
  	SDIO_Send_Data_Cfg(SD_DATATIMEOUT, 0, 0, 0);	//Clear the DPSM state machine configuration
	if(SDIO->RESP1 & SD_CARD_LOCKED)
		return SD_LOCK_UNLOCK_FAILED;				//The card is locked
	
 	if(CardType == SDIO_HIGH_CAPACITY_SD_CARD)		//High capacity card
	{
		blksize = 512;
		addr >>= 9;
	}
	
	if((blksize > 0) && (blksize <= 2048) && ((blksize & (blksize - 1)) == 0))
	{
		power = convert_from_bytes_to_power_of_two(blksize);	    
		SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN, 1, blksize);		//Send CMD16 and set the data length to blksize, short response 	   
		errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);	//Wait for an R1 response   
		if(errorstatus != SD_OK)
			return errorstatus;   							//Response error	 
		
	}
	else
	{
		return SD_INVALID_PARAMETER;	 
	}
	
	if(nblks > 1)
	{					  
		if(nblks * blksize > SD_MAX_DATA_LENGTH)
			return SD_INVALID_PARAMETER;   
		
     	if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
    	{
			//Ìá¸ßÐÔÄÜ
	 	   	SDIO_Send_Cmd(SD_CMD_APP_CMD, 1, (u32)RCA << 16);	//·¢ËÍACMD55,¶ÌÏìÓ¦ 	   
			errorstatus = CmdResp1Error(SD_CMD_APP_CMD);		//Wait for an R1 response   		   
			if(errorstatus != SD_OK)
				return errorstatus;				    
			
	 	   	SDIO_Send_Cmd(SD_CMD_SET_BLOCK_COUNT, 1, nblks);	//·¢ËÍCMD23,ÉèÖÃ¿éÊýÁ¿,¶ÌÏìÓ¦ 	   
			errorstatus = CmdResp1Error(SD_CMD_SET_BLOCK_COUNT);//Wait for an R1 response   		   
			if(errorstatus != SD_OK)
				return errorstatus;				    
		}
		
		SDIO_Send_Cmd(SD_CMD_WRITE_MULT_BLOCK, 1, addr);		//·¢ËÍCMD25,¶à¿éÐ´Ö¸Áî,¶ÌÏìÓ¦ 	   
		errorstatus = CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK);	//Wait for an R1 response   		   
		if(errorstatus != SD_OK)
			return errorstatus;
		
 	 	SDIO_Send_Data_Cfg(SD_DATATIMEOUT, nblks * blksize, power, 0);	//blksize, controller to card	
	    if(DeviceMode == SD_POLLING_MODE)
	    {
			while(!(SDIO->STA & ((1 << 4) | (1 << 1) | (1 << 8) | (1 << 3) | (1 << 9))))//ÏÂÒç/CRC/Êý¾Ý½áÊø/³¬Ê±/ÆðÊ¼Î»´íÎó
			{
				if(SDIO->STA & (1 << 14))							//·¢ËÍÇø°ë¿Õ,±íÊ¾ÖÁÉÙ´æÁË8×Ö(32×Ö½Ú)
				{	  
					if((tlen - bytestransferred) < SD_HALFFIFOBYTES)//Fewer than 32 bytes left
					{
						restwords = ((tlen - bytestransferred) % 4 == 0) ? ((tlen - bytestransferred) / 4) : ((tlen - bytestransferred) / 4 + 1);
						for(count = 0; count < restwords; count++, tempbuff++, bytestransferred += 4)
						{
							SDIO->FIFO = *tempbuff;
						}
					}
					else 										//·¢ËÍÇø°ë¿Õ,¿ÉÒÔ·¢ËÍÖÁÉÙ8×Ö(32×Ö½Ú)Êý¾Ý
					{
						for(count = 0; count < SD_HALFFIFO; count++)
						{
							SDIO->FIFO = *(tempbuff + count);
						}
						
						tempbuff += SD_HALFFIFO;
						bytestransferred += SD_HALFFIFOBYTES;
					} 
				}
			}
			
			if(SDIO->STA & (1 << 3))		//Data timeout error
			{										   
		 		SDIO->ICR |= 1 << 3; 		//Clear the error flags
				return SD_DATA_TIMEOUT;
		 	}
			else if(SDIO->STA & (1 << 1))	//Data block CRC error
			{
		 		SDIO->ICR |= 1 << 1; 		//Clear the error flags
				return SD_DATA_CRC_FAIL;		   
			}
			else if(SDIO->STA & (1 << 4)) 	//Receive FIFO underrun error
			{
		 		SDIO->ICR |= 1 << 4; 		//Clear the error flags
				return SD_TX_UNDERRUN;		 
			}
			else if(SDIO->STA & (1 << 9)) 	//Receive start-bit error
			{
		 		SDIO->ICR |= 1 << 9; 		//Clear the error flags
				return SD_START_BIT_ERR;		 
			}
			
			if(SDIO->STA & (1 << 8))		//·¢ËÍ½áÊø
			{															 
				if((SDIO_STD_CAPACITY_SD_CARD_V1_1 == CardType) || (SDIO_STD_CAPACITY_SD_CARD_V2_0 == CardType) || (SDIO_HIGH_CAPACITY_SD_CARD == CardType))
				{
					SDIO_Send_Cmd(SD_CMD_STOP_TRANSMISSION, 1, 0);			//Send CMD12 to end the transfer 	   
					errorstatus = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);	//Wait for an R1 response   
					if(errorstatus != SD_OK)
						return errorstatus;	 
				}
			}
	 		SDIO->ICR = 0X5FF;	 			//Clear all flags 
	    }
		else if(DeviceMode == SD_DMA_MODE)
		{
	   		TransferError = SD_OK;
			StopCondition = 1;				//¶à¿éÐ´,ÐèÒª·¢ËÍÍ£Ö¹´«ÊäÖ¸Áî 
			TransferEnd = 0;				//Transfer-complete flag, set to 1 in the interrupt handler
			SDIO->MASK |= (1 << 1) | (1 << 3) | (1 << 8) | (1 << 4) | (1 << 9);	//Enable the data-received interrupt
			SD_DMA_Config((u32*)buf, nblks * blksize, 1);		//SDIO DMA configuration
	 	 	SDIO->DCTRL |= 1 << 3;								//Enable SDIO DMA. 
			timeout = SDIO_DATATIMEOUT;
	 		while(((DMA2->ISR & 0X2000) == RESET) && timeout)
			{
				timeout--;					//Wait for the transfer to finish 
			}
			if(timeout==0)	 				//Timeout
			{									  
  				SD_Init();	 				//Re-initialising the SD card clears the hang seen on writes
	 			return SD_DATA_TIMEOUT;		//Timeout	 
	 		}
			
			timeout = SDIO_DATATIMEOUT;
			while((TransferEnd == 0) && (TransferError == SD_OK) && timeout)
			{
				timeout--;
			}
	 		if(timeout == 0)
				return SD_DATA_TIMEOUT;		//Timeout	 
			
	 		if(TransferError != SD_OK)
				return TransferError;	 
		}
  	}
 	SDIO->ICR = 0X5FF;	 		//Clear all flags
 	errorstatus = IsCardProgramming(&cardstate);
 	while((errorstatus == SD_OK) && ((cardstate == SD_CARD_PROGRAMMING) || (cardstate == SD_CARD_RECEIVING)))
	{
		errorstatus = IsCardProgramming(&cardstate);
	}
	return errorstatus;	   
}
 																    
//SDIOÖÐ¶Ï´¦Àíº¯Êý
//´¦ÀíSDIO´«Êä¹ý³ÌÖÐµÄ¸÷ÖÖÖÐ¶ÏÊÂÎñ
//Return: error code
SD_Error SD_ProcessIRQSrc(void)
{
	if(SDIO->STA & (1 << 8))	//½ÓÊÕÍê³ÉÖÐ¶Ï
	{	 
		if(StopCondition == 1)
		{
			SDIO_Send_Cmd(SD_CMD_STOP_TRANSMISSION, 1, 0);			//·¢ËÍCMD12,½áÊø´«Êä 	   
			TransferError = CmdResp1Error(SD_CMD_STOP_TRANSMISSION);
		}
		else
		{
			TransferError = SD_OK;	
		}
		
 		SDIO->ICR |= 1 << 8;										//Çå³ýÍê³ÉÖÐ¶Ï±ê¼Ç
		SDIO->MASK &= ~((1 << 1) | (1 << 3) | (1 << 8) | (1 << 14) | (1 << 15) | (1 << 4) | (1 << 5) | (1 << 9));//Disable the related interrupts
 		TransferEnd = 1;
		return(TransferError);
	}
	
 	if(SDIO->STA & (1 << 1))	//Êý¾ÝCRC´íÎó
	{
		SDIO->ICR |= 1 << 1;	//Clear the interrupt flag
		SDIO->MASK &= ~((1 << 1) | (1 << 3) | (1 << 8) | (1 << 14) | (1 << 15) | (1 << 4) | (1 << 5) | (1 << 9));//Disable the related interrupts
	    TransferError = SD_DATA_CRC_FAIL;
	    return(SD_DATA_CRC_FAIL);
	}
 	if(SDIO->STA & (1 << 3))	//Data timeout error
	{
		SDIO->ICR |= 1 << 3;	//Clear the interrupt flag
		SDIO->MASK &= ~((1 << 1) | (1 << 3) | (1 << 8) | (1 << 14) | (1 << 15) | (1 << 4) | (1 << 5) | (1 << 9));//Disable the related interrupts
	    TransferError = SD_DATA_TIMEOUT;
	    return(SD_DATA_TIMEOUT);
	}
  	if(SDIO->STA & (1 << 5))	//FIFOÉÏÒç´íÎó
	{
		SDIO->ICR |= 1 << 5;	//Clear the interrupt flag
		SDIO->MASK &= ~((1 << 1) | (1 << 3) | (1 << 8) | (1 << 14) | (1 << 15) | (1 << 4) | (1 << 5) | (1 << 9));//Disable the related interrupts
	    TransferError = SD_RX_OVERRUN;
	    return(SD_RX_OVERRUN);
	}
   	if(SDIO->STA & (1 << 4))	//FIFOÏÂÒç´íÎó
	{
		SDIO->ICR |= 1 << 4;	//Clear the interrupt flag
		SDIO->MASK &= ~((1 << 1) | (1 << 3) | (1 << 8) | (1 << 14) | (1 << 15) | (1 << 4) | (1 << 5) | (1 << 9));//Disable the related interrupts
	    TransferError = SD_TX_UNDERRUN;
	    return(SD_TX_UNDERRUN);
	}
	if(SDIO->STA & (1 << 9))	//ÆðÊ¼Î»´íÎó
	{
		SDIO->ICR |= 1 << 9;	//Clear the interrupt flag
		SDIO->MASK &= ~((1 << 1) | (1 << 3) | (1 << 8) | (1 << 14) | (1 << 15) | (1 << 4) | (1 << 5) | (1 << 9));//Disable the related interrupts
	    TransferError = SD_START_BIT_ERR;
	    return(SD_START_BIT_ERR);
	}
	return(SD_OK);
}

//SDIOÖÐ¶Ï·þÎñº¯Êý		  
void SDIO_IRQHandler(void) 
{											
 	SD_ProcessIRQSrc();			//´¦ÀíËùÓÐSDIOÏà¹ØÖÐ¶Ï
}

//²éÕÒSD¿¨µÄSCR¼Ä´æÆ÷Öµ
//rca:¿¨Ïà¶ÔµØÖ·
//pscr:Êý¾Ý»º´æÇø(´æ´¢SCRÄÚÈÝ)
//Return: error status		   
SD_Error FindSCR(u16 rca, u32 *pscr)
{ 
	u32 index = 0;
	SD_Error errorstatus = SD_OK;
	u32 tempscr[2] = {0, 0};
	
 	SDIO_Send_Cmd(SD_CMD_SET_BLOCKLEN, 1, 8);				//·¢ËÍCMD16,¶ÌÏìÓ¦,ÉèÖÃBlock SizeÎª8×Ö½Ú											  
 	errorstatus = CmdResp1Error(SD_CMD_SET_BLOCKLEN);
 	if(errorstatus != SD_OK)
		return errorstatus;
	
  	SDIO_Send_Cmd(SD_CMD_APP_CMD, 1, (u32)rca << 16);		//Send CMD55, short response 									  
 	errorstatus = CmdResp1Error(SD_CMD_APP_CMD);
 	if(errorstatus != SD_OK)
		return errorstatus;
	
	SDIO_Send_Data_Cfg(SD_DATATIMEOUT, 8, 3, 1);			//8¸ö×Ö½Ú³¤¶È,blockÎª8×Ö½Ú,SD¿¨µ½SDIO.
   	SDIO_Send_Cmd(SD_CMD_SD_APP_SEND_SCR, 1, 0);			//·¢ËÍACMD51,¶ÌÏìÓ¦,²ÎÊýÎª0											  
 	errorstatus = CmdResp1Error(SD_CMD_SD_APP_SEND_SCR);
 	if(errorstatus != SD_OK)
		return errorstatus;
	
 	while(!(SDIO->STA & (SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND | SDIO_FLAG_STBITERR)))
	{
		if(SDIO->STA & (1 << 21))							//½ÓÊÕFIFOÊý¾Ý¿ÉÓÃ
		{
			*(tempscr + index) = SDIO->FIFO;				//¶ÁÈ¡FIFOÄÚÈÝ
			index++;
			if(index >= 2)
				break;
		}
	}
	
 	if(SDIO->STA & (1 << 3))								//½ÓÊÕÊý¾Ý³¬Ê±
	{										 
 		SDIO->ICR |= 1 << 3;								//Clear the flag
		return SD_DATA_TIMEOUT;
	}
	else if(SDIO->STA & (1 << 1))							//ÒÑ·¢ËÍ/½ÓÊÕµÄÊý¾Ý¿éCRCÐ£Ñé´íÎó
	{
 		SDIO->ICR |= 1 << 1;								//Clear the flag
		return SD_DATA_CRC_FAIL;   
	}
	else if(SDIO->STA & (1 << 5))							//½ÓÊÕFIFOÒç³ö
	{
 		SDIO->ICR |= 1 << 5;								//Clear the flag
		return SD_RX_OVERRUN;   	   
	}
	else if(SDIO->STA & (1 << 9))							//ÆðÊ¼Î»¼ì²â´íÎó
	{
 		SDIO->ICR |= 1 << 9;								//Clear the flag
		return SD_START_BIT_ERR;    
	}
   	SDIO->ICR = 0X5FF;								 		//Clear the flag	 
	
	//°ÑÊý¾ÝË³Ðò°´8Î»Îªµ¥Î»µ¹¹ýÀ´.   	
	*(pscr + 1) = ((tempscr[0] & SD_0TO7BITS) << 24) | ((tempscr[0] & SD_8TO15BITS) << 8) | ((tempscr[0] & SD_16TO23BITS) >> 8) | ((tempscr[0] & SD_24TO31BITS) >> 24);
	*(pscr) = ((tempscr[1] & SD_0TO7BITS) << 24) | ((tempscr[1] & SD_8TO15BITS) << 8) | ((tempscr[1] & SD_16TO23BITS) >> 8) | ((tempscr[1] & SD_24TO31BITS) >> 24);
 	return errorstatus;
}

//Read the SD card
//buf: read data buffer
//sector: sector address
//cnt: number of sectors	
//Return: error status; 0 = ok; other = error code;				  				 
u8 SD_ReadDisk(u8*buf, u32 sector, u8 cnt)
{
	u8 sta = SD_OK;
	u8 n;
	
	if(CardType != SDIO_STD_CAPACITY_SD_CARD_V1_1)
		sector <<= 9;
	
	if((u32)buf % 4 != 0)
	{
	 	for(n = 0; n < cnt; n++)
		{
		 	sta = SD_ReadBlock(SDIO_DATA_BUFFER, sector + 512 * n, 512);		//Single sector read
			memcpy(buf, SDIO_DATA_BUFFER, 512);
			buf += 512;
		} 
	}
	else
	{
		if(cnt == 1)
			sta = SD_ReadBlock(buf, sector, 512);  							  	//Single sector read
		else
			sta = SD_ReadMultiBlocks(buf, sector, 512, cnt);					//Multiple sectors  
	}
	return sta;
}

//Write the SD card
//buf:Ð´Êý¾Ý»º´æÇø
//sector: sector address
//cnt: number of sectors	
//Return: error status; 0 = ok; other = error code;	
u8 SD_WriteDisk(u8*buf, u32 sector, u8 cnt)
{
	u8 sta = SD_OK;
	u8 n;
	
	if(CardType != SDIO_STD_CAPACITY_SD_CARD_V1_1)
		sector <<= 9;
	
	if((u32)buf % 4 != 0)
	{
	 	for(n = 0; n < cnt; n++)
		{
			memcpy(SDIO_DATA_BUFFER, buf, 512);
		 	sta = SD_WriteBlock(SDIO_DATA_BUFFER, sector + 512 * n, 512);		//Single sector write
			buf += 512;
		} 
	}
	else
	{
		if(cnt == 1)
			sta = SD_WriteBlock(buf, sector, 512);						    	//Single sector write
		else
			sta = SD_WriteMultiBlocks(buf, sector, 512, cnt);					//Multiple sectors  
	}
	return sta;
}

SD_Error SD_Init(void)
{
	SD_Error errorstatus = SD_OK;
	
	Sdio_GPIO_Init();
	SDIO_DeInit();
	
	errorstatus = SD_PowerON();
	if(errorstatus == SD_OK)
		errorstatus = SD_InitializeCards();					//Initialise the SD card
	
  	if(errorstatus == SD_OK)
		errorstatus = SD_GetCardInfo(&SDCardInfo);			//»ñÈ¡¿¨ÐÅÏ¢
	
 	if(errorstatus == SD_OK)
		errorstatus = SD_SelectDeselect((u32)(SDCardInfo.RCA << 16));//Ñ¡ÖÐSD¿¨  
	
   	if(errorstatus == SD_OK)
		errorstatus = SD_EnableWideBusOperation(SDIO_BusWide_4b);	//4Î»¿í¶È,Èç¹ûÊÇMMC¿¨,Ôò²»ÄÜÓÃ4Î»Ä£Ê½ 
	
  	if((errorstatus == SD_OK) || (SDIO_MULTIMEDIA_CARD == CardType))
	{  		    
//		SDIO_Clock_Set(SDIO_TRANSFER_CLK_DIV);				//ÉèÖÃÊ±ÖÓÆµÂÊ,SDIOÊ±ÖÓ¼ÆËã¹«Ê½:SDIO_CKÊ±ÖÓ=SDIOCLK/[clkdiv+2];ÆäÖÐ,SDIOCLKÒ»°ãÎª72Mhz 
		errorstatus = SD_SetDeviceMode(SD_DMA_MODE);		//ÉèÖÃÎªDMAÄ£Ê½
		//errorstatus = SD_SetDeviceMode(SD_POLLING_MODE);	//ÉèÖÃÎª²éÑ¯Ä£Ê½
 	}
	
	return errorstatus;
}

// not use detection pin
//uint8_t SD_Detect(void)
//{
//	u8 status = SD_PRESENT;
//	
//	/*!< Check GPIO to detect SD */
//	if (GPIO_ReadInputDataBit(SD_DETECT_GPIO_PORT, SD_DETECT_PIN) != Bit_RESET)
//	{
//		status = SD_NOT_PRESENT;
//	}
//	return status;
//}

