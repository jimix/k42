/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: elf64.h,v 1.4 2001/10/05 21:51:40 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: elf stuff
 *
 * **************************************************************************/

#ifndef __ELF64_H_
#define __ELF64_H_

/* define a simple name conversion for 64 bit ELF code to reflect back
 * to 32 bit Elf data structures.
 * This is just to get existing code running
 * H. Franke
 */

#define ELF64_EQUAL_ELF32 1

#if ELF64_EQUAL_ELF32

#define Elf64_Half Elf32_Half
#define Elf64_Word Elf32_Word
#define Elf64_Addr Elf32_Addr
#define Elf64_Off  Elf32_Off

#define Elf64_Ehdr Elf32_Ehdr
#define Elf64_Shdr Elf32_Shdr
#define Elf64_Phdr Elf32_Phdr
#define Elf64_Sym  Elf32_Sym
#define Elf64_Rel  Elf32_Rel
#define Elf64_Rela Elf32_Rela

#else /* #if ELF64_EQUAL_ELF32 */
/* this is not yet provided */
#endif /* #if ELF64_EQUAL_ELF32 */

#endif /* #ifndef __ELF64_H_ */
