#ifndef __ELFTYPES_H_
#define __ELFTYPES_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: elftypes.h,v 1.8 2003/12/03 18:52:54 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/


/* FIXME
 * this should all be cleaned up but hey it should also use libbfd
 */
#ifdef _AIX
#include <sys/ppcdefs.h>
#else /* #ifdef _AIX */
#include <stdint.h>
#endif /* #ifdef _AIX */

/* 32 bit data types */
#if (_PPC_SZLONG == 32)
typedef unsigned long	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef unsigned long	Elf32_Off;
typedef long		Elf32_Sword;
typedef unsigned long	Elf32_Word;
#else /* #if (_PPC_SZLONG == 32) */
typedef uint32_t	Elf32_Addr;
typedef uint16_t	Elf32_Half;
typedef uint32_t	Elf32_Off;
typedef int32_t		Elf32_Sword;
typedef uint32_t	Elf32_Word;
#endif /* #if (_PPC_SZLONG == 32) */

typedef unsigned char	Elf32_Byte;
typedef unsigned short	Elf32_Section;

/* 64 bit data types */
typedef uint64_t	Elf64_Addr;
typedef unsigned short	Elf64_Half;
typedef uint64_t	Elf64_Off;
typedef int32_t		Elf64_Sword;
typedef int64_t		Elf64_Sxword;
typedef uint32_t	Elf64_Word;
typedef uint64_t	Elf64_Xword;
typedef unsigned char	Elf64_Byte;
typedef unsigned short	Elf64_Section;

#endif /* #ifndef __ELFTYPES_H_ */
