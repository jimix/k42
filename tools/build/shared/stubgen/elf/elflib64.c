#ifndef ELFLIB64_DEFC
#define ELFLIB64_DEFC

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: elflib64.c,v 1.11 2004/08/20 17:30:49 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Generic 64-bit ELF library interface implementation
 * **************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf64.h>
#define NYI(fct) /* fprintf(stderr,"%s: not yet implemented\n",#fct) */

/**************************************************************************/
/* code to support both Big-endian and Little-endian */

/* Every reference to the ELF data has to be run thru a macro
   for its data type.  These are defined by the ELF data types:
   E_HALF, E_OFF, ... */

/* first decide if we are big-endian or little-endian */
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
# ifdef __BYTE_ORDER
#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define __LITTLE_ENDIAN__
#  else /* #if __BYTE_ORDER == __LITTLE_ENDIAN */
#    define __BIG_ENDIAN__
#  endif /* #if __BYTE_ORDER == __LITTLE_ENDIAN */
# else /* #ifdef __BYTE_ORDER */
#  ifdef PLATFORM_OS_AIX
#   define __BIG_ENDIAN__
#  else
#   error "Can't determine __BIG_ENDIAN__ or __LITTLE_ENDIAN__"
#  endif
# endif /* #ifdef __BYTE_ORDER */
#endif /* #if !defined(__BIG_ENDIAN__) && ... */

#if defined(__BIG_ENDIAN__)
#define DEFAULT_ENDIAN ELFDATA2MSB
#elif defined(__LITTLE_ENDIAN__)
#define DEFAULT_ENDIAN ELFDATA2LSB
#endif

/* There are two endians involved: the input ELF data file is
   in one endian; the machine that we are running on (the host)
   is in another endian.  If these two are the same, then no
   work need be done; if they are different, we must reverse
   each data item. */

int endian_elf = DEFAULT_ENDIAN;

#ifdef __LITTLE_ENDIAN__
int endian_host = ELFDATA2LSB;
#else
int endian_host = ELFDATA2MSB;
#endif /*#if __BYTE_ORDER == __LITTLE_ENDIAN*/


/**************************************************************************/

struct Elf_Scn {
    struct Elf* elf;
    int idx;
    Elf64_Shdr shdr;
};

struct Elf {
    int        fd;
    int        allocated;
    /* fileoffsets of typical accessed items */
    Elf64_Ehdr   ehdr;
    Elf_Scn     *scns;  /* array of pointers */
    Elf64_Shdr  *strtab; /* pointer to stringtab section */
};

/**************************************************************************/

#define elf_error(why) fprintf(stderr,"%s:%d elf error: %s\n",__FILE__,__LINE__, why)


static Elf *theElf;

void elf_read(Elf *elf, unsigned ofs, void *buf, size_t size)
{
    lseek(elf->fd,ofs,SEEK_SET);
    read(elf->fd,buf,size);
}

void* elf_alloc_read(Elf *elf, unsigned int ofs, size_t size)
{
    void *buf = malloc(size);
    if (buf == NULL)
      {
	elf_error("No room for elf_alloc_read");
	return (NULL);
      }
    elf_read(elf,ofs,buf,size);
    return (buf);
}


unsigned elf_version(unsigned dummy)
{
    return (EV_CURRENT);
}

char* elf_getident(Elf *elf, size_t *size)
{
    if (size != NULL)
      {
	*size = EI_NIDENT;
      }
    return elf->ehdr.e_ident;
}

Elf*
elf_begin(int fd, Elf_Cmd cmd, Elf* usrelf)
{
    Elf* relf;
    Elf64_Ehdr* ehdrp;

    /* at the current time we are only dealing with the reading of ELF
       files */
    if (cmd != ELF_C_READ) {
	elf_error("we can only read ELF files");
	return NULL;
    }

    if (usrelf) {
	relf = usrelf;
    } else {
	relf = (Elf*)malloc(sizeof(Elf));
	if (relf == NULL)
	  {
	    elf_error("No memory for Elf header structure");
	    return NULL;
	  }
    }

    /* initialization of the structure */
    memset(relf,0,sizeof(Elf));
    relf->fd = fd;
    relf->allocated = !usrelf;
    /* expect the file to be open, reset the file pointer */
    lseek(fd,0,SEEK_SET);

    ehdrp = &relf->ehdr;
    elf_read(relf,0,ehdrp,sizeof(Elf64_Ehdr));

    /* verify some information on the hdr */
    if (! (IS_ELF(*ehdrp)) )
    {
	elf_error("file is not ELF");
	return NULL;
    }

    if (ehdrp->e_ident[EI_CLASS] != ELFCLASS64) {
	elf_error("Elf file is not 64 bit");
	return NULL;
    }
    if (ehdrp->e_ident[EI_VERSION] != EV_CURRENT) {
	elf_error("Elf file is not current version");
	return NULL;
    }


    /* determine if little-endian or big-endian -- must know
       before we try to access multi-byte fields */
    if (ehdrp->e_ident[EI_DATA] == ELFDATA2LSB) {
        endian_elf = ELFDATA2LSB;
	DPRINTF((stderr,"Elf is little endian\n"));
    }
    else if (ehdrp->e_ident[EI_DATA] == ELFDATA2MSB) {
        endian_elf = ELFDATA2MSB;
	DPRINTF((stderr,"Elf is big endian\n"));
    }
    else {
	elf_error("Elf file is neither big nor little endian?");
	return NULL;
    }


    {
	/* preparing the section header structures */
	int recs = E_HALF(relf->ehdr.e_shnum);
	int size = recs * sizeof(Elf_Scn);
	int i;

	relf->scns = (Elf_Scn*)malloc(size);
	if (relf->scns == NULL)
	  {
	    elf_error("No memory for Elf section headers");
	    return NULL;
	  }
	memset(relf->scns,0,size);
	DPRINTF((stderr,"Elf has %d sections\n", recs));
	for (i=0;i<recs;i++) {
	  long ofs;
	  long n;

	  relf->scns[i].elf = relf;
	  relf->scns[i].idx = i;
	  ofs = E_OFF(relf->ehdr.e_shoff)
	    + i*E_HALF(relf->ehdr.e_shentsize);
	  n = sizeof(Elf64_Shdr);
	  DPRINTF((stderr,"  section %d is %d bytes at offset %ld\n", i, n, ofs));
	  elf_read(relf,ofs, &relf->scns[i].shdr,n);
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

Elf64_Ehdr*
elf64_getehdr(Elf* elf)
{
    return (&elf->ehdr);
}

static Elf64_Shdr*
elf64_getshdr_by_ndx(Elf *elf, int idx)
{
    if ((idx < 0) || (idx >= E_HALF(elf->ehdr.e_shnum))) {
	elf_error("Elf section header out of range");
	return NULL;
    }
    return (&elf->scns[idx].shdr);
}

Elf_Scn*
elf_nextscn(Elf *elf, Elf_Scn *scn)
{
    int scnidx = (scn == NULL) ? 1 : (scn->idx + 1);
    if ((scnidx < 0) || (scnidx >= E_HALF(elf->ehdr.e_shnum)))
	return NULL;
    else
	return ( & elf->scns[scnidx] );
}

Elf64_Shdr*
elf64_getshdr(Elf_Scn* section)
{
    if (section == NULL) return NULL;
    return &section->shdr;
}

#define ELF_INTERN_STR (128)
#define ELF_STR_READS  (8)

char string_table[ELF_STR_READS*ELF_INTERN_STR];

char*
elf_strptr(Elf *elf, size_t shstrndx, size_t str_idx)
{
    int ofs,sofs,i;

    if (elf->strtab == NULL) {
	elf->strtab = elf64_getshdr_by_ndx(elf,shstrndx);
	DPRINTF((stderr,"StrTab(%d) at ofs=%d\n",shstrndx,
		 E_OFF(elf->strtab->sh_offset)));
    }

    DPRINTF((stderr,"elf_strptr with shstrndx = %d, str_idx = %d\n", shstrndx, str_idx));

    ofs = E_OFF(elf->strtab->sh_offset) + str_idx;
    sofs = 0;

    elf->strtab = NULL;

    do {
	elf_read(elf,ofs,&string_table[sofs],ELF_INTERN_STR);
	for ( i=0;i<ELF_INTERN_STR;i++ ) {
	    if ( string_table[sofs+i] == '\0')
		return (string_table);
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


#endif /* #ifndef ELFLIB64_DEFC */
