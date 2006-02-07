/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: elf.c,v 1.4 2000/05/11 11:30:13 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: elf stuff
 *
 * **************************************************************************/

#include <elf.h>

#define ALIGN_UP( n, align )	( ((n) + (align) - 1) & ~((align)-1) )

int
elf_program_info( unsigned long paddr,
		  unsigned long *textStart, unsigned long *textSize,
		  unsigned long *dataStart,
		  unsigned long *dataSize, unsigned long *bssSize,
		  unsigned long *entry, unsigned long *gp )
{
    Elf64_Ehdr *eh64;
    Elf64_Phdr *phdr;
    int phdr_num;
    int i;
    int valid;				/* flag indicating validity of file */
    int seenText, seenData;

    paddr &= 0x00ffffffffffffffL;	/* strip phys region bits */
    /* paddr |= 0x9000000000000000L;*/	/* make uncached paddr, for startup */
/*#define CLUDGE_BAD_SHSEG*/
#ifndef CLUDGE_BAD_SHSEG /* to test out things when shared doesn't work */
    paddr |= 0xA800000000000000L;	/* make cached paddr, should be ok */
#else
    paddr |= 0xA000000000000000L;	/* make cached paddr, should be ok */
#endif
    eh64 = (Elf64_Ehdr *)paddr;

    /* consistency checks */
    valid = 1;
    valid = valid & eh64->e_ident[EI_MAG0]==0x7f;
    valid = valid & eh64->e_ident[EI_MAG1]=='E';
    valid = valid & eh64->e_ident[EI_MAG2]=='L';
    valid = valid & eh64->e_ident[EI_MAG3]=='F';

    if (!valid) {
	printf("loader: not an ELF format file.\n");
	return -1;
    }

    valid = valid & eh64->e_ident[EI_CLASS]==ELFCLASS64;
    if (!valid) {
	printf("loader: only support 64-bit ELF executables.\n");
	return -1;
    }

    valid = valid & eh64->e_ident[EI_DATA]==ELFDATA2MSB;
    if (!valid) {
	printf("loader: only support big-endian executables.\n");
	return -1;
    }

    valid = valid & eh64->e_ident[EI_VERSION]==EV_CURRENT;
    if (!valid) {
	printf("loader: only support current version ELF files.\n");
	return -1;
    }

    if (eh64->e_phoff==0) {
	printf("loader: null program header -> program not loadable.\n");
	return -1;
    }

    phdr = (Elf64_Phdr *)(paddr+eh64->e_phoff);
    phdr_num = eh64->e_phnum;

    /* we assume sections are all adjacent to each other */
    /* we assume only one text and one data/bss section */
    seenText = 0;
    seenData = 0;
    for (i=0; i<phdr_num; i++,phdr++) {
	if (phdr->p_type==0x70000002) {
	    /* I don't know why it's the number of above, or why we need to
	     * add 8 below, as it seems the headers and docs are a little
	     * out of touch with reality
	     */
	    Elf64_RegInfo *reginfo =
		(Elf64_RegInfo *)(paddr+phdr->p_offset + sizeof(Elf64_Addr));
	    *gp = reginfo->ri_gp_value;
	} else if (phdr->p_type==PT_LOAD) {

	    valid = 0;
	    if (phdr->p_flags == ((PF_R) | (PF_W))) {

		if( seenData ) {
		    printf("loader: more than one segment... ignoring\n");
		    continue;
		}
		seenData = 1;

		*dataStart = phdr->p_vaddr;
		*dataSize = ALIGN_UP(phdr->p_filesz, phdr->p_align);
		if( phdr->p_memsz > *dataSize ) {
		    *bssSize = ALIGN_UP( phdr->p_memsz - *dataSize,
					  phdr->p_align);
		} else if( phdr->p_memsz > phdr->p_filesz ) {
		    /* *bssSize = phdr->p_memsz - phdr->p_filesz; */
		    printf("loader: WARNING: ignoring bss partial segment\n");
		    *bssSize = 0;
		} else {
		    *bssSize = 0;
		}

	    } else if (phdr->p_flags == ((PF_R) | (PF_X)) ||
		     phdr->p_flags == ((PF_R) | (PF_W) | PF_X)) {

		if( seenText ) {
		    printf("loader: more than one segment... ignoring\n");
		    continue;
		}
		seenText = 1;
		*textStart = phdr->p_vaddr;
		*textSize = ALIGN_UP(phdr->p_filesz, phdr->p_align);
	    }
	}
    }

    if( !seenText ) {
	printf("Missing text segment\n");
    }
    if( !seenData ) {
	printf("Missing data segment\n");
    }

    *entry = eh64->e_entry;

    return 0;
}
