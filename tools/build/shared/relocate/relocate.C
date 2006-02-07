/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: relocate.C,v 1.10 2005/06/10 15:19:50 awaterl Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: pre-relocator for ld.so
 * **************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#ifdef Linux
#define _GNU_SOURCE
#include <getopt.h>
#endif
#include "elf.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
/* A PowerPC64 function descriptor.  The .plt (procedure linkage
   table) and .opd (official procedure descriptor) sections are
   arrays of these.  */
typedef struct
{
    Elf64_Addr fd_func;
    Elf64_Addr fd_toc;
    Elf64_Addr fd_aux;
} Elf64_FuncDesc;


extern int fstat(int filedes, struct stat *buf);
typedef unsigned long long uval;
char* strings;
char* base;
int debug = 0;

#define PRINTF(xxx...) ({if (debug) { printf( xxx ); }})

struct dyn_data{
    uval pltrelsz;
    uval rela;
    uval relasz;
    uval relaent;
    uval relacount;
    uval symtab;
    uval strtab;
};

dyn_data dd;

Elf64_Phdr *phdr;
Elf64_Ehdr *ehdr;

Elf64_Shdr *bssHdr;
Elf64_Shdr *shdrs = NULL;
uval l_addr = 0;


#if !defined(__BIG_ENDIAN__) && !defined(PLATFORM_AIX)
//Little endian system
# include <byteswap.h>
// EC - generic endian converter
# define EC(x)								\
({									\
	extern void bad_byte_conversion();				\
	typeof(x) y;							\
	switch (sizeof(x)) {						\
		case 1:	y = x; break;					\
		case 2: y = bswap_16(x); break;				\
		case 4: y = bswap_32(x); break;				\
		case 8: y = bswap_64(x); break;				\
		default:						\
		printf("Bad size: %zd\n",sizeof(x));			\
	}								\
	y;								\
})
#else
# define EC(x) (x)
# define bswap_16(x) (x)
# define bswap_32(x) (x)
# define bswap_64(x) (x)
#endif

#define adjustVal(x) ({ ((x)) = bswap_64(l_addr + EC(x)); })


//Take a pointer from vaddr space and present pointer in file image
Elf64_Addr* virtToFile(Elf64_Addr offset) {
    int i = 0;
    if (offset > EC(bssHdr->sh_addr) + EC(bssHdr->sh_size)) {
	return NULL;
    }
    while ( i<ehdr->e_shnum ) {
	Elf64_Shdr *hdr = &shdrs[i];
	if (EC(hdr->sh_addr) <= offset &&
	    EC(hdr->sh_addr) + EC(hdr->sh_size) > offset) {
	    return (Elf64_Addr*) (offset - EC(hdr->sh_addr) +
				  EC(hdr->sh_offset) + base);
	}
	++i;
    }
    return NULL;
}


Elf64_Addr adjustPtr(Elf64_Addr in) {
    if (in > EC(bssHdr->sh_addr) + EC(bssHdr->sh_size)) {
	return EC(in);
    }
    return EC(in + l_addr);
}


int examine_dynamic(Elf64_Phdr *ph, uval baseAddr) {
    Elf64_Dyn* start = (Elf64_Dyn*)(EC(ph->p_offset) + base);
    Elf64_Dyn* end = (Elf64_Dyn*)(base + EC(ph->p_offset) + EC(ph->p_filesz));
    uval offset = baseAddr;
    while (start<end) {
	char* name;
	switch (EC(start->d_tag)) {
	default:
	    ++start;
	    continue;
	    break;
	case DT_PLTRELSZ:
	    name = "pltrelsz";
	    dd.pltrelsz = EC(start->d_un.d_val);
	    break;
	case DT_RELA:
	    name = "rela";
	    dd.rela = EC(start->d_un.d_val) - offset;
	    adjustVal(start->d_un.d_val);
	    break;
	case DT_RELASZ:
	    name = "relasz";
	    dd.relasz = EC(start->d_un.d_val);
	    break;
	case DT_RELAENT:
	    name = "relaent";
	    dd.relaent = EC(start->d_un.d_val);
	    break;
	case DT_RELACOUNT:
	    name = "relacount";
	    dd.relacount = EC(start->d_un.d_val);
	    break;
	case DT_SYMTAB:
	    name = "symtab";
	    dd.symtab = EC(start->d_un.d_val) - offset;
	    adjustVal(start->d_un.d_val);
	    break;
	case DT_HASH:
	    //This is a pointer that needs relocation
	    name = "hash";
	    adjustVal(start->d_un.d_val);
	    break;
	case DT_PLTGOT:
	    //This is a pointer that needs relocation
	    name = "pltgot";
	    adjustVal(start->d_un.d_val);
	    break;
	case DT_STRTAB:
	    //This is a pointer that needs relocation
	    dd.strtab = EC(start->d_un.d_val);
	    name = "strtab";
	    adjustVal(start->d_un.d_val);
	    break;
	case DT_RELENT:
	    //This is a pointer that needs relocation
	    name = "relent";
	    adjustVal(start->d_un.d_val);
	    break;
	case DT_REL:
	    //This is a pointer that needs relocation
	    name = "rel";
	    adjustVal(start->d_un.d_val);
	    break;
	case DT_JMPREL:
	    //This is a pointer that needs relocation
	    name = "jmprel";
	    adjustVal(start->d_un.d_val);
	    break;
	case DT_VERSYM:
	    //This is a pointer that needs relocation
	    name = "versym";
	    adjustVal(start->d_un.d_val);
	    break;

	}
	PRINTF("%08" PRIx64 " %016" PRIx64 " %s\n",
	       EC(start->d_tag), EC(start->d_un.d_val), name);
	++start;
    }
    return 0;
}

void usage() {
    PRINTF("relocate -i <input> -o <output> -l <offset>\n");
    exit(0);
}

int main(int argc, char** argv) {
    int c;
    char* file;
    char* output;
    int makeref = 0;
    uval location;
    while ((c = getopt(argc, argv, "i:o:l:dr")) != EOF) {
	switch (c) {
	case 'i':
	    file = optarg;
	    break;
	case 'o':
	    output = optarg;
	    break;
	case 'l':
	    location = strtoll(optarg, NULL, 16);
	    break;
	case 'd':
	    debug = 1;
	    break;
	case 'r':
	    makeref = 1;
	    break;
	default:
	    usage();
	}
    }

    int fd = open(file, O_RDWR);
    struct stat info;
    int newFD = open(output, O_RDWR|O_CREAT, 0755);
    fstat(fd,&info);
    uval inData;
    base = (char*)malloc(info.st_size);
    inData = read(fd, base, info.st_size);

    ehdr = (Elf64_Ehdr*)base;
    phdr = (Elf64_Phdr*)(base + EC(ehdr->e_phoff));

    shdrs = (Elf64_Shdr*)(base + EC(ehdr->e_shoff));

    Elf64_Shdr *nameSec = &shdrs[EC(ehdr->e_shstrndx)];

    Elf64_Shdr *gotHdr = NULL;
    strings = (char*)base + EC(nameSec->sh_offset);

    PRINTF("Idx type %8.8s %8.8s %8.8s\n","offset","size","addr" );
    for (int i = 1; i<EC(ehdr->e_shnum); ++i) {
	Elf64_Shdr *shdr64 = &shdrs[i];
	PRINTF("%02d     %x %08" PRIx64 " %08" PRIx64 " %08" PRIx64 " %s\n",
	       i,
	       EC(shdr64->sh_type),
	       EC(shdr64->sh_offset),
	       EC(shdr64->sh_size),
	       EC(shdr64->sh_addr),
	       strings + EC(shdr64->sh_name));
	if (memcmp(strings+EC(shdr64->sh_name),".got",4)==0) {
	    gotHdr = shdr64;
	}
	if (memcmp(strings+EC(shdr64->sh_name),".bss",4)==0) {
	    bssHdr = shdr64;
	}
    }

    uval origLocation = EC(phdr[0].p_vaddr);
    //Figure out how much image needs to be relocated
    l_addr = location - EC(phdr[0].p_vaddr);

    if (makeref){
	Elf64_Phdr *p = phdr;

	// This will tell ld.so where to find the Phdr --
	// in the target library's mappings

	//Note the gross casts and endian conversions.  These are so
	//nasty because we have to do a cast from one integer size to
	//another, with the values potentially being stored in the
	//wrong endian format.
	p->p_type = EC(PT_PHDR);
	p->p_flags = EC(PF_R);
	p->p_offset = bswap_64((Elf64_Off(bswap_64(ehdr->e_phoff))));
	p->p_vaddr = bswap_64(
			(Elf64_Addr(origLocation + bswap_64(p->p_offset))));
	p->p_paddr = bswap_64(
			(Elf64_Addr(origLocation + bswap_64(p->p_offset))));
	p->p_filesz = bswap_64((Elf64_Xword)bswap_16(ehdr->e_phentsize));
	p->p_memsz = bswap_64((Elf64_Xword)bswap_16(ehdr->e_phentsize));
	p->p_align = 0;

	++p;

	// One LOAD segment --- ld.so barfs if there isn't one.
	// Just put anything in here -- it will be unused anyways
	p->p_type = EC(PT_LOAD);
	p->p_vaddr= EC((Elf64_Addr)location);
	p->p_paddr= EC((Elf64_Addr)location);
	p->p_align= EC((typeof(p->p_align))4096);
	p->p_filesz=EC((typeof(p->p_filesz))4096);
	p->p_memsz= EC((typeof(p->p_memsz))4096);
	p->p_offset=0;
	p->p_flags = PF_R;

	// Leave the DYNAMIC segment alone -- it points into the
	// target library already
	++p;

	// Write it all out.  We leave everything else unchanged;
	// we've changed everything ld.so will look at, but the rest
	// is still needed by the compile time linker.

	off_t newSize=  write(newFD,base, info.st_size);
	int ret =0;
	PRINTF("done %ld %d %d\n", newSize, ret, errno);
	return 0;
    }


    examine_dynamic(&phdr[2],EC(phdr[0].p_vaddr));

    //Fixup the got contents
    *virtToFile(EC(gotHdr->sh_addr)) = EC(gotHdr->sh_addr) + 0x8000 + l_addr;

    strings = (char*)virtToFile(dd.strtab);

    Elf64_Rela *re = (Elf64_Rela*)(base + dd.rela);
    Elf64_Rela *end_reloc =
	(Elf64_Rela*)(base + dd.rela + dd.relasz + dd.pltrelsz);

    PRINTF("Relocation entries begin %p/%llx [%llu]\n",re,dd.rela,dd.relacount);
    PRINTF("Relocation entries end   %p/%llx [%llu]\n",
	   end_reloc, dd.rela + dd.relasz+dd.pltrelsz,
	   (dd.relasz + dd.pltrelsz) / 24);

    for (uval i=0; i< dd.relacount; ++i) {
	Elf64_Addr* addr = virtToFile(EC(re->r_offset));
	Elf64_Addr contents = adjustPtr(EC(re->r_addend));
	if (*addr == contents) {
	    PRINTF("%08" PRIx64 " <- %" PRIx64 "  Already set\n",
		    EC(re->r_offset), EC(*addr));
	} else {
	    PRINTF("%08" PRIx64 " <- %" PRIx64 " {%" PRIx64 "}\n",
		    EC(re->r_offset), EC(contents), EC(*addr));
	    *addr = contents;
	}
	++re;
    }

    Elf64_Sym *symtab = (Elf64_Sym*)(base + dd.symtab);
    for (;re < end_reloc;++re) {
	Elf64_Addr* addr = virtToFile(EC(re->r_offset));
	int r_type = ELF64_R_TYPE(EC(re->r_info));
	if (r_type == R_PPC64_RELATIVE) {
	    Elf64_Addr contents = adjustPtr(EC(re->r_addend));
	    if (*addr == contents) {
		PRINTF("%08" PRIx64 " <- %" PRIx64 "  Already set\n",
			EC(re->r_offset), EC(*addr));
	    } else {
		PRINTF("%08" PRIx64 " <- %" PRIx64 " {%" PRIx64 "}\n",
			EC(re->r_offset), EC(contents), EC(*addr));
		*addr = contents;
	    }
	    continue;
	}
	if (r_type == R_PPC64_NONE) {
	    continue;
	}
	Elf64_Sym* sym = &symtab[ELF64_R_SYM(EC(re->r_info))];
	Elf64_Addr val = EC(sym->st_value) + EC(re->r_addend);

	switch (r_type) {
	case R_PPC64_ADDR64:
	case R_PPC64_GLOB_DAT:
	{

	    Elf64_Addr contents = adjustPtr(val);;
	    if (*addr == contents) {
		PRINTF("%08" PRIx64 " <- %" PRIx64 "  Already set %s\n",
		       EC(re->r_offset), EC(*addr),strings+sym->st_name);
	    } else {
		PRINTF("%08" PRIx64 " <- %" PRIx64 " {%" PRIx64 "} %s\n",
			EC(re->r_offset), EC(contents), EC(*addr),
			strings+sym->st_name);
		*addr = contents;
	    }
	}
	break;
	case R_PPC64_JMP_SLOT:
	{
	    Elf64_FuncDesc *plt = (Elf64_FuncDesc*)addr;
	    if (val) {
		Elf64_FuncDesc *rel = (Elf64_FuncDesc*)virtToFile(val);
		PRINTF("%08" PRIx64 " <- FD:%" PRIx64 " %" PRIx64
			" from: %" PRIx64 " %s\n",
		       EC(re->r_offset), EC(rel->fd_func), EC(rel->fd_toc),
		       val, strings+sym->st_name);
		plt->fd_aux = rel->fd_aux;
		plt->fd_toc = rel->fd_toc;
		plt->fd_func = rel->fd_func;
	    } else {
		plt->fd_aux = 0;
		plt->fd_func = 0;
		plt->fd_toc = 0;
	    }
	    break;
	}
	default:
	    PRINTF("Unknown entry\n");
	    break;
	}
    }


    off_t newSize=  write(newFD,base, info.st_size);
    int ret =0;
    PRINTF("done %ld %d %d\n",newSize, ret, errno);
    return 0;
}
