#include "dma.h"																	   	  
#include "delay.h"																	   	  

u16 DMA1_MEM_LEN;//Store the length of each DMA transfer 		    
//Configuration for each DMA1 channel
//The transfer setup here is fixed; it has to be changed to suit other cases
//Memory -> peripheral mode / 8-bit data width / memory increment mode
//DMA_CHx: DMA channel CHx
//cpar: peripheral address
//cmar: memory address
//cndtr: transfer count  
void MYDMA_Config(DMA_Channel_TypeDef *DMA_CHx, u32 cpar, u32 cmar, u16 cndtr)
{
	RCC->AHBENR|=1<<0;				//Enable the DMA1 clock
	delay_ms(5);					//Wait for the DMA clock to settle
	DMA_CHx->CPAR = cpar; 	 		//DMA1 peripheral address 
	DMA_CHx->CMAR = (u32)cmar; 		//DMA1, memory address
	DMA1_MEM_LEN = cndtr;      		//Store the DMA transfer count
	DMA_CHx->CNDTR = cndtr;    		//DMA1, transfer count
	DMA_CHx->CCR = 0X00000000;		//Reset
	DMA_CHx->CCR |= 1 << 4;  		//Read from memory
	DMA_CHx->CCR |= 0 << 5;  		//Normal mode
	DMA_CHx->CCR |= 0 << 6; 		//Peripheral address, no increment
	DMA_CHx->CCR |= 1 << 7; 	 	//Memory increment mode
	DMA_CHx->CCR |= 0 << 8; 	 	//Peripheral data width 8 bits
	DMA_CHx->CCR |= 0 << 10; 		//Memory data width 8 bits
	DMA_CHx->CCR |= 1 << 12; 		//Medium priority
	DMA_CHx->CCR |= 0 << 14; 		//Not memory-to-memory mode		  	
}
 
//Start a single DMA transfer
void MYDMA_Enable(DMA_Channel_TypeDef* DMA_CHx)
{
	DMA_CHx->CCR &= ~(1 << 0);			//Disable the DMA transfer 
	DMA_CHx->CNDTR = DMA1_MEM_LEN;		//DMA1, transfer count 
	DMA_CHx->CCR |= 1 << 0;				//Enable the DMA transfer
}	  
