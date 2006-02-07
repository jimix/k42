#ifndef __AMD64_ELF_H_
#define __AMD64_ELF_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: elf.H,v 1.3 2001/10/05 21:47:55 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: requisite elf header info for amd64 parseExecutable.C
 * **************************************************************************/

// derived from the mips64 version instead of powerpc, seems that Elf64_Addr
// is more appropriate than Elf32_Addr and other 32 bits aspects of powerpc  VVV
// XXX

#define EI_NIDENT	16

typedef unsigned long	Elf64_Addr;
typedef unsigned short	Elf64_Half;
typedef unsigned long	Elf64_Off;
typedef int		Elf64_Sword;
typedef unsigned int	Elf64_Word;
typedef unsigned long	Elf64_Xword;


#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_PAD		7

#define ELFCLASSNONE	0
#define ELFCLASS32	1
#define ELFCLASS64	2
#define ELFCLASSNUM	3

#define ELFDATANONE	0
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2

#define EV_NONE		0
#define EV_CURRENT	1

#define PT_NULL		0
#define PT_LOAD		1
#define PT_DYNAMIC	2
#define PT_INTERP	3
#define PT_NOTE		4
#define PT_SHLIB	5
#define PT_PHDR		6

#define PHF_R		4
#define PHF_W		2
#define PHF_X		1

typedef struct {
	unsigned char	e_ident[EI_NIDENT];	/* ident bytes */
	Elf64_Half	e_type;			/* file type */
	Elf64_Half	e_machine;		/* target machine */
	Elf64_Word	e_version;		/* file version */
	Elf64_Addr	e_entry;		/* start address */
	Elf64_Off	e_phoff;		/* phdr file offset */
	Elf64_Off	e_shoff;		/* shdr file offset */
	Elf64_Word	e_flags;		/* file flags */
	Elf64_Half	e_ehsize;		/* sizeof ehdr */
	Elf64_Half	e_phentsize;		/* sizeof phdr */
	Elf64_Half	e_phnum;		/* number phdrs */
	Elf64_Half	e_shentsize;		/* sizeof shdr */
	Elf64_Half	e_shnum;		/* number shdrs */
	Elf64_Half	e_shstrndx;		/* shdr string index */
} Elf64_Ehdr;

typedef struct {
	Elf64_Word	p_type;		/* entry type */
	Elf64_Word	p_flags;	/* entry flags */
	Elf64_Off	p_offset;	/* file offset */
	Elf64_Addr	p_vaddr;	/* virtual address */
	Elf64_Addr	p_paddr;	/* physical address */
	Elf64_Xword	p_filesz;	/* file size */
	Elf64_Xword	p_memsz;	/* memory size */
	Elf64_Xword	p_align;	/* memory/file alignment */
} Elf64_Phdr;

typedef struct {
	Elf64_Word	sh_name;	/* section name */
	Elf64_Word	sh_type;	/* SHT_... */
	Elf64_Xword	sh_flags;	/* SHF_... */
	Elf64_Addr	sh_addr;	/* virtual address */
	Elf64_Off	sh_offset;	/* file offset */
	Elf64_Xword	sh_size;	/* section size */
	Elf64_Word	sh_link;	/* misc info */
	Elf64_Word	sh_info;	/* misc info */
	Elf64_Xword	sh_addralign;	/* memory alignment */
	Elf64_Xword	sh_entsize;	/* entry size if table */
} Elf64_Shdr;

typedef struct
{
        Elf64_Word      ri_gprmask;	/* mask of general regs used */
        Elf64_Word      ri_pad;		/* for alignment */
        Elf64_Word      ri_cprmask[4];	/* mask of cop regs used */
        Elf64_Addr      ri_gp_value;	/* initial value of gp */
} Elf64_RegInfo;

#endif /* #ifndef __AMD64_ELF_H_ */
