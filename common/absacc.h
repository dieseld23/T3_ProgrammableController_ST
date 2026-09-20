#ifndef __ABSACC_H__
#define __ABSACC_H__

/*
 * Absolute memory access macros.
 *
 * MDK-ARM v4 shipped ABSACC.H in C:\Keil\ARM\INC. MDK5 dropped that folder,
 * so <absacc.h> (included unconditionally by common/main.h) no longer resolves
 * and every translation unit that pulls in main.h fails to compile. This is a
 * local replacement so the ARM build finds the header; the asix/ C51 projects
 * keep using Keil's own copy from the C51 toolchain.
 *
 * Nothing in this tree currently uses these macros - they are provided only to
 * match the original header. DWORD is deliberately NOT defined here: FatFS
 * declares it as a typedef in SD/FATFS/src/integer.h, and a macro of the same
 * name would break every FatFS prototype that uses it.
 */

#define CBYTE ((unsigned char volatile *)0)
#define DBYTE ((unsigned char volatile *)0)
#define PBYTE ((unsigned char volatile *)0)
#define XBYTE ((unsigned char volatile *)0)

#define CWORD ((unsigned short volatile *)0)
#define PWORD ((unsigned short volatile *)0)
#define XWORD ((unsigned short volatile *)0)

#endif
