#ifndef ELFLIB_DEFC
#define ELFLIB_DEFC

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: elflib.c,v 1.7 2004/01/30 21:59:01 aabauman Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Generic ELF library interface implementation
 * **************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>

#define NYI(fct) /* fprintf(stderr,"%s: not yet implemented\n",#fct) */

/**************************************************************************/

struct Elf_Scn {
    struct Elf* elf;
    int idx;
    Elf32_Shdr shdr;
};

struct Elf {
    int        fd;
    int        allocated;
    /* fileoffsets of typical accessed items */
    Elf32_Ehdr   ehdr;
    Elf_Scn     *scns;  /* array of pointers */
    Elf32_Shdr  *strtab; /* pointer to stringtab section */
};

/**************************************************************************/

static Elf *theElf;

void elf_read(Elf *elf, unsigned ofs, void *buf, size_t size)
{
    lseek(elf->fd,ofs,SEEK_SET);
    read(elf->fd,buf,size);
}

void* elf_alloc_read(Elf *elf, unsigned int ofs, size_t size)
{
    void *buf = malloc(size);
    elf_read(elf,ofs,buf,size);
    return (buf);
}

#define elf_error() fprintf(stderr,"%s:%d elf error\n",__FILE__,__LINE__)

unsigned elf_version(unsigned dummy)
{
    return (EV_CURRENT);
}

char* elf_getident(Elf *elf, size_t *size)
{
    if (size) *size = EI_NIDENT;
    return elf->ehdr.e_ident;
}

Elf*
elf_begin(int fd, Elf_Cmd cmd, Elf* usrelf)
{
    Elf* relf;
    Elf32_Ehdr* ehdrp;

    /* at the current time we are only dealing with the reading of ELF files */
    if (cmd != ELF_C_READ) {
	elf_error();
	return NULL;
    }

    if (usrelf) {
	relf = usrelf;
    } else {
	relf = (Elf*)malloc(sizeof(Elf));
    }
    /* initialization of the structure */
    memset(relf,0,sizeof(Elf));
    relf->fd = fd;
    relf->allocated = !usrelf;
    lseek(fd,0,SEEK_SET);
    /* expect the file to be open, reset the file pointer */
    ehdrp = &relf->ehdr;
    elf_read(relf,0,ehdrp,sizeof(Elf32_Ehdr));
    /* verify some information on the hdr */

    if (! (IS_ELF(*ehdrp)) )
    {
	elf_error();
	return NULL;
    }

    if (ehdrp->e_ident[EI_CLASS] != ELFCLASS32) {
	elf_error();
	return NULL;
    }
    if (ehdrp->e_ident[EI_DATA] != ELFDATA2MSB) {
	elf_error();
	return NULL;
    }
    if (ehdrp->e_ident[EI_VERSION] != EV_CURRENT) {
	elf_error();
	return NULL;
    }

    {
	/* preparing the section header structures */
	int recs = relf->ehdr.e_shnum;
	int size = recs * sizeof(Elf_Scn);
	int i;

	relf->scns = (Elf_Scn*)malloc(size);
	memset(relf->scns,0,size);
	for (i=0;i<recs;i++) {
	    relf->scns[i].elf = relf;
	    relf->scns[i].idx = i;
	    elf_read(relf,relf->ehdr.e_shoff + i*relf->ehdr.e_shentsize,
		     &relf->scns[i].shdr,sizeof(Elf32_Shdr));
	}
    }
    theElf = relf;
    return (relf);
}

int
elf_end(Elf *elf)
{
    if (elf->allocated) free(elf);
    return (0);
}

Elf32_Ehdr*
elf32_getehdr(Elf* elf)
{
    return (&elf->ehdr);
}

static Elf32_Shdr*
elf32_getshdr_by_ndx(Elf *elf, int idx)
{
    if ((idx < 0) || (idx >= elf->ehdr.e_shnum)) {
	elf_error();
	return NULL;
    }
    return (&elf->scns[idx].shdr);
}

Elf_Scn*
elf_nextscn(Elf *elf, Elf_Scn *scn)
{
    int scnidx = (scn == NULL) ? 1 : (scn->idx + 1);
    if ((scnidx < 0) || (scnidx >= elf->ehdr.e_shnum))
	return NULL;
    else
	return ( & elf->scns[scnidx] );
}

Elf32_Shdr*
elf32_getshdr(Elf_Scn* section)
{
    if (section == NULL) return NULL;
    return &section->shdr;
}

#define ELF_INTERN_STR (128)
#define ELF_STR_READS  (8)

char*
elf_strptr(Elf *elf, size_t shstrndx, size_t str_idx)
{
    static char str[ELF_STR_READS*ELF_INTERN_STR];

    int ofs,sofs,i;

    if (elf->strtab == NULL) {
	elf->strtab = elf32_getshdr_by_ndx(elf,shstrndx);
	/* printf("StrTab(%d) at ofs=%d\n",shstrndx,elf->strtab->sh_offset); */
    }

    ofs = elf->strtab->sh_offset + str_idx;
    sofs = 0;

    elf->strtab = NULL;

    do {
	elf_read(elf,ofs,&str[sofs],ELF_INTERN_STR);
	for ( i=0;i<ELF_INTERN_STR;i++ ) {
	    if ( str[sofs+i] == '\0')
		return (str);
	}
	ofs  += ELF_INTERN_STR;
	sofs += ELF_INTERN_STR;
    } while (sofs < ELF_STR_READS*ELF_INTERN_STR);
    return (NULL);
}

size_t
elf_ndxscn(Elf_Scn* section)
{
    if (section == NULL)
	return SHN_UNDEF;
    return (section->idx);
}

Elf_Data*
elf_getdata(Elf_Scn *scn, Elf_Data *data)
{
    static Elf_Data theData;
    if (scn == NULL) return NULL;
    if (data == NULL) {
	elf_read(scn->elf,scn->shdr.sh_offset,&theData,sizeof(Elf_Data));
    }
    return &theData;
}


#endif /* ELFLIB_DEFC */
