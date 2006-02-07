/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: genmap.c,v 1.1 2005/06/07 03:46:38 jk Exp $
 *****************************************************************************/
/**
 * @file genmap.c
 * Tool to generate the symbol table for module relocation
 */
#include <stdio.h>
#include <elf.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define Elf_Ehdr	Elf64_Ehdr
#define Elf_Shdr	Elf64_Shdr
#define Elf_Rela	Elf64_Rela
#define Elf_Sym		Elf64_Sym
#define Elf_Phdr	Elf64_Phdr
#define Elf_Dyn		Elf64_Dyn
#define ELF_R_SYM	ELF64_R_SYM
#define ELF_R_TYPE	ELF64_R_TYPE
#define ELF_R_INFO	ELF64_R_INFO
#define ELF_ST_TYPE	ELF64_ST_TYPE
#define ELF_ST_BIND	ELF64_ST_BIND

union object {
    char	*buf;
    Elf_Ehdr	*ehdr;
};

#define section(obj,n) (((Elf_Shdr *)((obj).buf + bswap((obj).ehdr->e_shoff)))\
		+ (n))
#define string(obj,s,n) ((char *)section_addr((obj),(s)) + (n))

#define u64fmt "%016" PRIx64

#if !defined(__BIG_ENDIAN__) && !defined(PLATFORM_AIX)
#include <byteswap.h>
#define bswap(x)							\
({									\
	typeof(x) y;							\
	switch (sizeof(x)) {						\
		/*case 1:	y = x; break;*/				\
		case 2: y = bswap_16(x); break;				\
		case 4: y = bswap_32(x); break;				\
		case 8: y = bswap_64(x); break;				\
		default:						\
		assert(0);						\
	}								\
	y;								\
})

#else
#define bswap(x) (x)
#endif
/*
#define section_name(obj,n) (section((obj),(obj).ehdr->sh_strndx) + \
                             section((obj),n)->sh_name)
*/

enum stype {
    STYPE_DISCARD,
    STYPE_ADDR,
    STYPE_OPD
};


enum stype symbol_type(enum stype *section_types, unsigned int index)
{
    if (index == SHN_ABS)
	return STYPE_ADDR;

    if (index == SHN_UNDEF)
	return STYPE_DISCARD;

    if (index >= SHN_LORESERVE)
	return STYPE_DISCARD;

    return section_types[index];
}

enum stype section_type(union object obj, Elf_Shdr *shdr)
{
    char *name, *shstrtab;

    shstrtab = obj.buf +
	    bswap(section(obj,bswap(obj.ehdr->e_shstrndx))->sh_offset);
    name = shstrtab + bswap(shdr->sh_name);
    
    if (!strcmp(shstrtab + bswap(shdr->sh_name), ".opd"))
	return STYPE_OPD;

    return STYPE_ADDR;
}

void get_opd(union object obj, unsigned int section, uint64_t addr,
	uint64_t *opd)
{
    Elf_Shdr *shdr = section(obj, section);

    addr -= bswap(shdr->sh_addr);
    addr += bswap(shdr->sh_offset);

    opd[0] = bswap(((uint64_t *)(obj.buf + addr))[0]);
    opd[1] = bswap(((uint64_t *)(obj.buf + addr))[1]);

}


int print_symtab(union object obj, enum stype *stypes, Elf_Shdr *shdr)
{
    Elf_Sym *syms;
    unsigned int i, nsyms;
    char *strtab, *shstrtab;

    syms = (Elf_Sym *)(obj.buf + bswap(shdr->sh_offset));
    strtab = obj.buf + bswap(section(obj, bswap(shdr->sh_link))->sh_offset);
    shstrtab = obj.buf +
	    bswap(section(obj,bswap(obj.ehdr->e_shstrndx))->sh_offset);
    nsyms = bswap(shdr->sh_size) / sizeof(Elf_Sym);

    for (i = 0; i < nsyms; i++) {
	unsigned short section = bswap(syms[i].st_shndx);
	enum stype sectype;
	unsigned char symtype;
	unsigned char symbind;
	char *name;

	sectype = symbol_type(stypes, section);
	symtype = ELF_ST_TYPE(syms[i].st_info);
	symbind = ELF_ST_BIND(syms[i].st_info);

	name = strtab + bswap(syms[i].st_name);

	if (symbind == STB_LOCAL)
	    continue;

	if (!name || !*name)
	    continue;
	
	if (sectype == STYPE_ADDR) {
	    printf("%s 0x" u64fmt "\n",
		name,
		bswap(syms[i].st_value));

	} else if (sectype == STYPE_OPD) {
	    uint64_t opd[2];

	    get_opd(obj, section, bswap(syms[i].st_value), opd);

	    printf("%s 0x" u64fmt " 0x" u64fmt " 0x" u64fmt "\n",
		name,
		bswap(syms[i].st_value),
		opd[0],
		opd[1]);
	}
    }

    return 0;
}

int print_symtabs(union object obj)
{
    Elf_Shdr *shdrs;
    unsigned int i;
    enum stype *stypes;

    if (bswap(obj.ehdr->e_shentsize) != sizeof(Elf_Shdr)) {
	fprintf(stderr, "Elf_Shdr size mismatch %d != %lu\n",
			bswap(obj.ehdr->e_shentsize),
			(unsigned long)sizeof(Elf_Shdr));
	return -ENOEXEC;
    }

    shdrs = (Elf_Shdr *)(obj.buf + bswap(obj.ehdr->e_shoff));

    if (!(stypes = malloc(bswap(obj.ehdr->e_shnum) * sizeof(enum stype)))) {
	perror("malloc");
	return -ENOMEM;
    }

    for (i = 0; i < bswap(obj.ehdr->e_shnum) ; i++) {
	stypes[i] = section_type(obj, shdrs + i);
    }

    for (i = 0; i < bswap(obj.ehdr->e_shnum) ; i++) {
	if (bswap(shdrs[i].sh_type) == SHT_SYMTAB) {
	    int rc;
	    if ((rc = print_symtab(obj, stypes, shdrs + i)))
		return rc;
	}
    }
    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    struct stat statbuf;
    union object obj;
    int rc = 0;

    if (argc != 2) {
	printf("Usage: %s <object>\n", argv[0]);
	return EXIT_FAILURE;
    }

    if (!(fd = open(argv[1], O_RDONLY))) {
	perror("open");
	return EXIT_FAILURE;
    }

    if (fstat(fd, &statbuf)) {
	perror("stat");
	return EXIT_FAILURE;
    }

    obj.buf = mmap(0, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (obj.buf == MAP_FAILED) {
	perror("mmap");
	return EXIT_FAILURE;
    }

    rc = print_symtabs(obj);

    if (munmap(obj.buf, statbuf.st_size)) {
	perror("munmap");
	return EXIT_FAILURE;
    }

    return rc;
}


