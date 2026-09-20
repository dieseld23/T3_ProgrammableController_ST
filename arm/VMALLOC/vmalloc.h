#ifndef __VMALLOC_H
#define __VMALLOC_H

#include "stm32f10x.h"

#ifndef NULL
#define NULL					0
#endif

#define SRAMIN					0	//Internal pool
#define SRAMEX					1	//External pool


//mem1 parameters; mem1 lives entirely in internal SRAM
#define MEM1_BLOCK_SIZE			32  	  						//The memory block size is 32 bytes
#define MEM1_MAX_SIZE			1*1024  						//Manages at most 20K
#define MEM1_ALLOC_TABLE_SIZE	MEM1_MAX_SIZE/MEM1_BLOCK_SIZE 	//Memory table size

//mem2 parameters; the mem2 pool lives in external SRAM and the rest in internal SRAM
#define MEM2_BLOCK_SIZE			32  	  						//The memory block size is 32 bytes
#define MEM2_MAX_SIZE			1*1024  						//Manages at most 200K
#define MEM2_ALLOC_TABLE_SIZE	MEM2_MAX_SIZE/MEM2_BLOCK_SIZE 	//Memory table size
		 
		 
//Memory management controller
struct _m_mallco_dev
{
	void (*init)(u8);						//Initialise
	u8 (*perused)(u8);		  	    		//Memory usage
	u8 *membase[2];							//The pool manages two regions of memory
	u16 *memmap[2]; 						//Memory management state table
	u8 memrdy[2]; 							//Whether memory management is ready
};
extern struct _m_mallco_dev mallco_dev;	 	//Defined in malloc.c

void mymemset(void *s, u8 c, u32 count);	 //Set memory
void mymemcpy(void *des, void *src, u32 n);	//Copy memory     
void mem_init(u8 memx);					 	//Initialise memory management (internal or external)
u32 mem_malloc(u8 memx, u32 size);		 	//Allocate memory (internal)
u8 mem_free(u8 memx, u32 offset);		 	//Free memory (internal)
u8 mem_perused(u8 memx);				 	//Get the memory usage (internal or external) 
////////////////////////////////////////////////////////////////////////////////
//Functions the user calls
void myfree(u8 memx, void *ptr);  				//Free memory (external)
void *mymalloc(u8 memx, u32 size);				//Allocate memory (external)
void *myrealloc(u8 memx, void *ptr, u32 size);	//Reallocate memory (external)

#endif
