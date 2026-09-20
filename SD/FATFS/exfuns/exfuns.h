#ifndef __EXFUNS_H
#define __EXFUNS_H 			   

//#include "stm32f10x.h" 
#include "ff.h"
#include "types.h"
 
//extern FATFS far *fs;  
//extern FIL far *file;	 
extern FIL far *ftemp;	 
extern UINT br,bw;
extern FILINFO far fileinfo;
extern DIR far dir;
//extern u8 far *fatbuf;//SD card data buffer

extern FATFS far fs;
extern FIL far file; 
//extern FIL far dst_file;

//Type codes returned by f_typetell
//Taken from the FILE_TYPE_TBL table, which is defined in exfuns.c
#define T_BIN		0X00	//bin file
#define T_LRC		0X10	//lrc file
#define T_NES		0X20	//nes file
#define T_TEXT		0X30	//.txt file
#define T_C			0X31	//.c file
#define T_H			0X32    //.h file
#define T_FLAC		0X4C	//flac file
#define T_BMP		0X50	//bmp file
#define T_JPG		0X51	//jpg file
#define T_JPEG		0X52	//jpeg file		 
#define T_GIF		0X53	//gif file  

 
u8 exfuns_init(void);							//Allocate memory
u8 f_typetell(u8 *fname);						//Identify the file type
u8 exf_getfree(u8 *drv,u32 *total,u32 *free);	//Get the total and free capacity of the disk
u32 exf_fdsize(u8 *fdname);						//Get the size of a folder			 																		   
#endif


