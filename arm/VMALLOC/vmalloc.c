#include "vmalloc.h"	    


//Memory pool (4-byte aligned)
__align(4) u8 mem1base[MEM1_MAX_SIZE];													//Internal SRAM memory pool
__align(4) u8 mem2base[MEM2_MAX_SIZE] __attribute__((at(0X68000000)));					//External SRAM memory pool
//Memory management table
u16 mem1mapbase[MEM1_ALLOC_TABLE_SIZE];													//Internal SRAM pool map
u16 mem2mapbase[MEM2_ALLOC_TABLE_SIZE] __attribute__((at(0X68000000 + MEM2_MAX_SIZE)));	//External SRAM pool map
//Memory management parameters	   
const u32 memtblsize[2] = {MEM1_ALLOC_TABLE_SIZE,MEM2_ALLOC_TABLE_SIZE};				//Memory table size
const u32 memblksize[2] = {MEM1_BLOCK_SIZE,MEM2_BLOCK_SIZE};							//Memory block size
const u32 memsize[2] = {MEM1_MAX_SIZE,MEM2_MAX_SIZE};									//Total pool size


//Memory management controller
struct _m_mallco_dev mallco_dev =
{
	mem_init,				//Memory initialisation
	mem_perused,			//Memory usage
	mem1base, mem2base,		//Memory pool
	mem1mapbase, mem2mapbase,//Memory management state table
	0,0,  					//Memory management is not ready
};

//Copy memory
//*des: the destination address
//*src: the source address
//n: the number of bytes to copy
void mymemcpy(void *des, void *src, u32 n)  
{  
    u8 *xdes = des;
	u8 *xsrc = src; 
    while(n--)
		*xdes++ = *xsrc++;  
}

//Set memory
//*s: the start address
//c : the value to set
//count: the number of bytes to set
void mymemset(void *s, u8 c, u32 count)  
{  
    u8 *xs = s;  
    while(count--)
		*xs++=c;  
}

//Initialise memory management  
//memx: which memory block
void mem_init(u8 memx)  
{  
    mymemset(mallco_dev.memmap[memx], 0, memtblsize[memx] * 2);	//Clear the memory state table  
	mymemset(mallco_dev.membase[memx], 0, memsize[memx]);		//Clear the whole memory pool  
	mallco_dev.memrdy[memx] = 1;								//Memory management initialised  
}

//Get the memory usage
//memx: which memory block
//Return: the usage, 0~100
u8 mem_perused(u8 memx)  
{  
    u32 used = 0;  
    u32 i;  
    for(i = 0; i < memtblsize[memx]; i++)  
    {  
        if(mallco_dev.memmap[memx][i])
			used++; 
    }
	
    return (used * 100) / (memtblsize[memx]);  
}

//Allocate memory (internal)
//memx: which memory block
//size: number of bytes to allocate
//Return: 0xFFFFFFFF on error; otherwise the offset into the pool 
u32 mem_malloc(u8 memx, u32 size)  
{  
    signed long offset = 0;  
    u16 nmemb;												//The number of blocks needed  
	u16 cmemb = 0;											//Count of consecutive free blocks
    u32 i;  
    if(!mallco_dev.memrdy[memx])
		mallco_dev.init(memx);								//Not initialised yet, so initialise first 
	
    if(size == 0)
		return 0XFFFFFFFF;									//Nothing to allocate
	
    nmemb = size / memblksize[memx];  						//Work out how many consecutive blocks are needed
    if(size % memblksize[memx])
		nmemb++;
	
    for(offset = memtblsize[memx] - 1; offset >= 0; offset--)//Search the whole control area  
    {     
		if(!mallco_dev.memmap[memx][offset])
			cmemb++;										//One more consecutive free block
		else
			cmemb = 0;										//Reset the run of free blocks
		
		if(cmemb == nmemb)									//Found nmemb consecutive free blocks
		{
            for(i = 0; i < nmemb; i++)  					//Mark the blocks as used 
            {  
                mallco_dev.memmap[memx][offset+i] = nmemb;  
            }
			
            return (offset * memblksize[memx]);				//Return the offset  
		}
    }
	
    return 0XFFFFFFFF;										//No suitable run of blocks was found  
}

//Free memory (internal) 
//memx: which memory block
//offset: the offset into the pool
//Return: 0 = freed; 1 = failed;  
u8 mem_free(u8 memx, u32 offset)  
{  
    int i;  
    if(!mallco_dev.memrdy[memx])					//Not initialised yet, so initialise first
	{
		mallco_dev.init(memx);    
        return 1;									//Not initialised  
    }
	
    if(offset<memsize[memx])						//The offset lies inside the pool. 
    {  
        int index = offset / memblksize[memx];		//The block number the offset falls in  
        int nmemb = mallco_dev.memmap[memx][index];	//Number of blocks
        for(i = 0; i < nmemb; i++)  				//Clear the blocks
        {  
            mallco_dev.memmap[memx][index+i] = 0;  
        }
		
        return 0;  
    }
	else
		return 2;									//The offset is out of range.  
}

//Free memory (external) 
//memx: which memory block
//ptr: the start address 
void myfree(u8 memx, void *ptr)  
{  
	u32 offset;  
    if(ptr == NULL)
		return;					//The address is 0.  
	
 	offset = (u32)ptr - (u32)mallco_dev.membase[memx];  
    mem_free(memx, offset);		//Free memory     
}

//Allocate memory (external)
//memx: which memory block
//size: the size in bytes
//Return: the start of the memory allocated.
void *mymalloc(u8 memx, u32 size)  
{  
    u32 offset;  									      
	offset = mem_malloc(memx, size);  	   				   
    if(offset == 0XFFFFFFFF)
		return NULL;  
    else
		return (void*)((u32)mallco_dev.membase[memx] + offset);  
}

//Reallocate memory (external)
//memx: which memory block
//*ptr: the old start address
//size: number of bytes to allocate
//Return: the start of the newly allocated memory.
void *myrealloc(u8 memx, void *ptr, u32 size)  
{  
    u32 offset;
    offset = mem_malloc(memx, size);  
    if(offset == 0XFFFFFFFF)
	{
		return NULL;     
	}
    else  
    {  									   
	    mymemcpy((void*)((u32)mallco_dev.membase[memx] + offset), ptr, size);	//Copy the old contents into the new memory   
        myfree(memx, ptr);  											  		//Free the old memory
        return (void*)((u32)mallco_dev.membase[memx] + offset);  				//Return the new start address
    }  
}
