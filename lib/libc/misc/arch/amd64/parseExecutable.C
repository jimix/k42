/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: parseExecutable.C,v 1.7 2002/11/15 15:43:23 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: has functions that takes the address of the beginning
 *                     of an elf file and returns pointers to its data, text,
 *                     entry, bss, and other critical components.  Code modified
 *                     from code in elf.c taken from Toronto
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <misc/execute.H>


SysStatus
PutAuxVector(uval memStart, uval &offset, BinInfo &info)
{
    return 0;
}


sval
parseExecutable(uval vaddr, BinInfo *info)
{
    Elf64_Ehdr *eh64;
    Elf64_Phdr *phdr;
    uval phdr_num;
    uval i;
    uval valid;				/* flag indicating validity of file */
    uval seenText, seenData;

    eh64 = (Elf64_Ehdr *)vaddr;

    /* consistency checks */
    valid = 1;
    valid = valid & eh64->e_ident[EI_MAG0]==0x7f;
    valid = valid & eh64->e_ident[EI_MAG1]=='E';
    valid = valid & eh64->e_ident[EI_MAG2]=='L';
    valid = valid & eh64->e_ident[EI_MAG3]=='F';
    tassertMsg(valid, "loader: not an ELF format file.\n");

    valid = valid & eh64->e_ident[EI_CLASS]==ELFCLASS64;
    tassertMsg(valid, "loader only support 64-bit ELF executables.\n");

    valid = valid & eh64->e_ident[EI_DATA]==ELFDATA2LSB;
    tassertMsg(valid, "loader only support little-endian executables.\n");

    valid = valid & eh64->e_ident[EI_VERSION]==EV_CURRENT;
    tassertMsg(valid, "loader only supports current version ELF files.\n");

    tassertMsg(eh64->e_phoff!=0, "loader found null program header.\n");

    phdr = (Elf64_Phdr *)(vaddr+eh64->e_phoff);
    phdr_num = eh64->e_phnum;

    /* we assume sections are all adjacent to each other */
    /* we assume only one text and one data/bss section */
    seenText = 0;
    seenData = 0;
    for (i=0; i<phdr_num; i++,phdr++) {
	if (phdr->p_type==PT_LOAD) {
	    if (phdr->p_flags == ((PHF_R) | (PHF_W))) {
		if (seenData) {
		    err_printf("error: loader found more than one data segment\n");
		    return -1;
		}
		tassert((phdr->p_memsz >= phdr->p_filesz),
			err_printf("seomthing's wrong with data segment\n"));

		seenData = 1;

		info->sec[BinInfo::DATA].offset = phdr->p_offset;
		info->sec[BinInfo::DATA].start = phdr->p_vaddr;
		info->sec[BinInfo::DATA].size = phdr->p_filesz;
		info->sec[BinInfo::BSS].start = phdr->p_vaddr+ phdr->p_filesz;
		info->sec[BinInfo::BSS].size = phdr->p_memsz - phdr->p_filesz;
	    } else if (phdr->p_flags == ((PHF_R) | (PHF_X)) ||
		     phdr->p_flags == ((PHF_R) | (PHF_W) | PHF_X)) {

		if (seenText) {
		    err_printf("error: loader found more than one test segment\n");
		    return -1;
		}
		seenText = 1;
		info->sec[BinInfo::TEXT].offset = phdr->p_offset;
		info->sec[BinInfo::TEXT].start = phdr->p_vaddr;

		//*textSize = ALIGN_UP(phdr->p_filesz, phdr->p_align);
		// we will round only to page at next higher level
		// want to have enough info to map only the text and not
		// what's after it
		info->sec[BinInfo::TEXT].size = phdr->p_filesz;
	    }
	}
    }

    if (!seenText) {
	err_printf("error: loader did not find text segment\n");
	return -1;
    }
    if (!seenData) {
	err_printf("error: loader did not find data segment\n");
	return -1;
    }

    if (info) {
	info->entryPointDesc.rip = codeAddress(eh64->e_entry);
	err_printf(" program starts at 0x%lx\n",
		   (long int)(info->entryPointDesc.rip));
    }

    return 0;
}
