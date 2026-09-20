#include "product.h"

#if STORE_TO_SD

#include "fattester.h"	 
//#include "sdcard.h"
#include "mmc_sd.h"
//#include "usmart.h"
//#include "usart.h"
#include "exfuns.h"
//#include "vmalloc.h"
#include "ff.h"
#include "string.h"



    
//Register a work area for the disk	 
//drv: drive letter
//Return: the result
//u8 mf_mount(u8 drv)
//{		   
//	return f_mount(drv, fs[drv]); 
//}

//Open the file at the given path
//path: path and file name
//mode: open mode
//Return: the result
u8 mf_open(u8*path, u8 mode)
{
	u8 res;	 
	res = f_open(&file, (const TCHAR*)path, mode);	//Open a folder
	return res;
}

//Close the file
//Return: the result
u8 mf_close(void)
{
	f_close(&file);
	return 0;
}

//Read data
//len: number of bytes read
//Return: the result
u8 mf_read(u16 len)
{
	u16 i, t;
	u8 res = 0;
	u16 tlen = 0;
//	printf("\r\nRead file data is:\r\n");
	for(i = 0; i < len / 512; i++)
	{
		res = f_read(&file, fatbuf, 512, &br);
		if(res)
		{
//			printf("Read Error:%d\r\n", res);
			break;
		}
		else
		{
			tlen += br;
//			for(t = 0; t < br; t++)
//				printf("%c", fatbuf[t]); 
		}
	}
	
	if(len % 512)
	{
		res = f_read(&file, fatbuf, len % 512, &br);
		if(res)	//Error reading the data
		{
//			printf("\r\nRead Error:%d\r\n", res);   
		}
		else
		{
			tlen += br;
//			for(t = 0; t < br; t++)
//				printf("%c", fatbuf[t]); 
		}
	}
	
//	if(tlen)
//		printf("\r\nReaded data len:%d\r\n", tlen);//number of bytes read
	
//	printf("Read data over\r\n");	 
	return res;
}

//Write data
//dat: data buffer
//len: number of bytes to write
//Return: the result
u8 mf_write(u8*dat, u16 len)
{			    
	u8 res;	   					   

//	printf("\r\nBegin Write file...\r\n");
//	printf("Write data len:%d\r\n", len);	 
	res = f_write(&file,dat,len,&bw);
	if(res)
	{
//		printf("Write Error:%d\r\n", res);   
	}else
	{
	//	printf("Writed data len:%d\r\n", bw);
	}
	
	//printf("Write data over.\r\n");
	return res;
}

//Open a folder
 //path: the path
//Return: the result
u8 mf_opendir(u8* path)
{
	return f_opendir(&dir, (const TCHAR*)path);	
}

//Read a folder
//Return: the result
//u8 mf_readdir(void)
//{
//	u8 res;
//	char *fn;			 
//#if _USE_LFN
// 	fileinfo.lfsize = _MAX_LFN * 2 + 1;
//	fileinfo.lfname = mymalloc(SRAMIN,fileinfo.lfsize);
//#endif
//	
//	res = f_readdir(&dir, &fileinfo);//read the details of one file
//	if(res != FR_OK || fileinfo.fname[0] == 0)
//	{
//		myfree(SRAMIN, fileinfo.lfname);
//		return res;//finished reading.
//	}
//	
//#if _USE_LFN
//	fn = *fileinfo.lfname ? fileinfo.lfname : fileinfo.fname;
//#else
//	fn = fileinfo.fname;
//#endif
//	
////	printf("\r\n DIR info:\r\n");

////	printf("dir.id:%d\r\n", dir.id);
////	printf("dir.index:%d\r\n", dir.index);
////	printf("dir.sclust:%d\r\n", dir.sclust);
////	printf("dir.clust:%d\r\n", dir.clust);
////	printf("dir.sect:%d\r\n", dir.sect);	  

////	printf("\r\n");
////	printf("File Name is:%s\r\n", fn);
////	printf("File Size is:%d\r\n", fileinfo.fsize);
////	printf("File data is:%d\r\n", fileinfo.fdate);
////	printf("File time is:%d\r\n", fileinfo.ftime);
////	printf("File Attr is:%d\r\n", fileinfo.fattrib);
////	printf("\r\n");
//	myfree(SRAMIN, fileinfo.lfname);
//	return 0;
//}			 

 //Walk the files
 //path: the path
 //Return: the result
//u8 mf_scan_files(char *path)
//{
//	FRESULT res;	  
//    char *fn;   /* This function is assuming non-Unicode cfg. */
//#if _USE_LFN
// 	fileinfo.lfsize = _MAX_LFN * 2 + 1;
//	fileinfo.lfname = mymalloc(SRAMIN, fileinfo.lfsize);
//#endif		  

//    res = f_opendir(&dir, (const TCHAR*)path); //open a directory
//    if (res == FR_OK) 
//	{	
////		printf("\r\n"); 
//		while(1)
//		{
//	        res = f_readdir(&dir, &fileinfo);                   //read one file from the directory
//	        if (res != FR_OK || fileinfo.fname[0] == 0) break;  //error, or end reached, so stop
//	        //if (fileinfo.fname[0] == '.') continue;             //skip the parent directory
//			
//#if _USE_LFN
//        	fn = *fileinfo.lfname ? fileinfo.lfname : fileinfo.fname;
//#else							   
//        	fn = fileinfo.fname;
//#endif	                                              /* It is a file. */
//			
//			printf("%s/", path);//print the path	
//			printf("%s\r\n",  fn);//print the file name	  
//		} 
//    }
//	
//	myfree(SRAMIN, fileinfo.lfname);
//    return res;	  
//}

//Show the free capacity
//drv: drive letter
//Return: free capacity in bytes
u32 mf_showfree(u8 *drv)
{
	FATFS *fs1;
	u8 res;
    u32 fre_clust = 0, fre_sect = 0, tot_sect = 0;
    //Get the disk details and the number of free clusters
    res = f_getfree((const TCHAR*)drv, (void *)&fre_clust, &fs1);
    if(res == 0)
	{											   
	    tot_sect = (fs1->n_fatent - 2) * fs1->csize;//Get the total sector count
	    fre_sect = fre_clust * fs1->csize;			//Get the free sector count	   
		
#if _MAX_SS != 512
		tot_sect *= fs1->ssize / 512;
		fre_sect *= fs1->ssize / 512;
#endif
		
		if(tot_sect < 20480)						//Total capacity under 10M
		{
		    /* Print free space in unit of KB (assuming 512 bytes/sector) */
//		    printf("\r\nTotal disk capacity:%d KB\r\n"
//		           "Free space:%d KB\r\n",
//		           tot_sect >> 1, fre_sect >> 1);
		}else
		{
		    /* Print free space in unit of KB (assuming 512 bytes/sector) */
//		    printf("\r\nTotal disk capacity:%d MB\r\n"
//		           "Free space:%d MB\r\n",
//		           tot_sect >> 11, fre_sect >> 11);
		}
	}
	
	return fre_sect;
}

//Move the file read/write pointer
//offset: offset from the start of the file
//Return: the result.
u8 mf_lseek(u32 offset)
{
	return f_lseek(&file, offset);
}

//Read the current position of the file pointer.
//Return: the position
u32 mf_tell(void)
{
	return f_tell(&file);
}

//Get the file size
//Return: the file size
u32 mf_size(void)
{
	return f_size(&file);
}

//Create a directory
//pname: directory path and name
//Return: the result
u8 mf_mkdir(u8 *pname)
{
	return f_mkdir((const TCHAR *)pname);
}

//Format
//drv: drive letter
//mode: the mode
//au: cluster size
//Return: the result
u8 mf_fmkfs(u8 drv, u8 mode, u16 au)
{
	return f_mkfs(drv, mode, au);//Format; drv: drive letter; mode: the mode; au: cluster size
}

//Delete a file or directory
//pname: path and name of the file or directory
//Return: the result
u8 mf_unlink(u8 *pname)
{
	return  f_unlink((const TCHAR *)pname);
}

//Rename a file or directory (and if the directory differs, this moves it too)
//oldname: the previous name
//newname: the new name
//Return: the result
u8 mf_rename(u8 *oldname, u8* newname)
{
	return f_rename((const TCHAR *)oldname, (const TCHAR *)newname);
}

//Read a string from the file
//size: number of bytes to read
void mf_gets(u16 size)
{
 	TCHAR* rbuf;
	rbuf = f_gets((TCHAR*)fatbuf, size, &file);
	
	if(*rbuf == 0)
	{
		return  ;		//No data was read
	}
	else
	{
//		printf("\r\nThe String Readed Is:%s\r\n", rbuf);  	  
	}			    	
}

//Requires _USE_STRFUNC>=1
//Write one character to the file
//c: the character to write
//Return: the result
u8 mf_putc(u8 c)
{
	return f_putc((TCHAR)c, &file);
}

//Write a string to the file
//c: the string to write
//Return: the length of the string written
u8 mf_puts(u8*c)
{
	return f_puts((TCHAR*)c, &file);
}

#endif
