/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ext_elf.c,v 1.8 2001/10/08 22:27:14 jimix Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    This module accesses a 32bit Elf File and determines the index
 *    of virtual functions in their virtual function table.
 *    In particular it requires a program to have the following layout
 *    class XClass {
 *         void virtual myfunc( ARGS );
 *    }
 *    void (XClass::*__Virtual__<somevarname>)(ARGS) = &XClass::myfunc;
 *
 *    We are explicitely searching for "__Virtual__" variable and
 *    examining the initialization data in the ".data" segment for this
 *    variable to determine the index.
 *
 *    NOTE: this module works closely with all the other programs and scripts
 *          in the stubgen directory. It is imperative that the assumptions
 *          and outputs are maintained consistent.
 * **************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <libelf.h>

#ifdef O_BINARY  /* in case the architecture cares */
#define OPEN_FLAGS (O_RDWR | O_BINARY)
#else
#define OPEN_FLAGS (O_RDWR)
#endif

/**************************************************************************
 * MEMBER FUNCTIONS POINTER:
 * Architectures and compilers define the layout of memberfunction pointer
 * in various ways. Since we are looking for static initializers of member
 * function pointer variables we must know the layout
 * In order to define for new ARCH/COMP combination restructure the layout
 * of the <Ptr_to_mem_func> and define MACROS
 *

 #define MEMFCT_IS_VIRTUAL(m) // is the memberfunction a valid virtual func
 #define MEMFCT_GET_INDEX(m)  // get the index.. sometimes byte index
 #define MEMFCT_GET_FADDR(m)  // get the address of the non-virt func
 #define THE_DATA_SECTION  ELF_DATA  // which data segment to look for
 #define SMALL_DATA_SECTION  ELF_SDATA  // name of small data segment

 *
 **************************************************************************/

/* this AIX XLC compiler which doesn't define __XLC__ as it should */
#if (defined(_POWER) || defined(_POWERPC)) && (! defined(__GNUC__))
typedef struct {
    void* faddr;
    int index;
    int delta;
    int v_off;
} Ptr_to_mem_func;

#define MEMFCT_IS_VIRTUAL(m) ((m)->faddr != NULL)
#define MEMFCT_GET_INDEX(m)  ((m)->index/4)
#define MEMFCT_GET_FADDR(m)  ((m)->faddr)
#define THE_DATA_SECTION  ELF_DATA
#define SMALL_DATA_SECTION ELF_SDATA
#endif

#if defined(__GNUC__)
typedef struct {
    short delta;
    short index;
    union {
	void* faddr;
	short v_off;
    } u;
} Ptr_to_mem_func;
/* no multiple inheritance is disallowed */
#define MEMFCT_IS_VIRTUAL(m) (((m)->index >= 0))
#define MEMFCT_GET_INDEX(m)  ((m)->index)
#define MEMFCT_GET_FADDR(m)  ((m)->u.faddr)
#define THE_DATA_SECTION  ELF_DATA
#define SMALL_DATA_SECTION ELF_SDATA
#endif

/**************************************************************************/

size_t      nsyms;
Elf32_Sym  *psyms = NULL;
char       *pdata = NULL;
int        data_index = 0;
char       *psdata = NULL;
int        sdata_index = 0;
char       *pstr  = NULL;
char       program[128];

#define VMAGIC	"__Virtual__"
#define SMAGIC	"__Static__"


void extract_virtual_index()
{
    int i;
    for( i = 0; i < nsyms; i++, psyms++ ) {
	Ptr_to_mem_func	*p;
	char *name = &pstr[psyms->st_name];

	if(strncmp(name, VMAGIC, strlen(VMAGIC)) == 0) {

	    /* the symbol has the magic "__Virtual__" infront of it
	     * get its initialization value, because the value obtains
	     * the index the delta etc of the virtual function invocation
	     */

	    if (psyms->st_shndx == sdata_index)
		p = (Ptr_to_mem_func *) (psdata + psyms->st_value);
	    else if (psyms->st_shndx == data_index)
		p = (Ptr_to_mem_func *) (pdata + psyms->st_value);
	    else {
		fprintf(stderr, "%s: %s Bad data section index: %d \n",
			program, name, psyms->st_shndx);
		exit(1);
	    }

	    if (!(MEMFCT_IS_VIRTUAL(p))) {
		fprintf (stderr, "%s: %s Bad method ptr: %p %x %x \n",
			 program, name,
			 MEMFCT_GET_FADDR(p), p->index, p->delta );
		exit(1);
	    }

	    printf("%d %ld %s\n", MEMFCT_GET_INDEX(p), psyms->st_value, name );
	} else if( strncmp( name, SMAGIC, strlen(SMAGIC) ) == 0 ) {
	    printf("%d\n", -1 );
	}
    }
}

#define TMAGIC	"__TYPE_"

void extract_type_values()
{
    int i;
    for( i = 0; i < nsyms; i++, psyms++ ) {
	char   *name = &pstr[psyms->st_name];
	unsigned long *p;
	/* printf("%s: %d %x\n", name, psyms->st_size, psyms->st_value);  */
	if( strncmp( name, TMAGIC, strlen(TMAGIC) ) == 0 ) {
	    if (psyms->st_shndx == sdata_index)
		p = (unsigned long *) (psdata + psyms->st_value);
	    else if (psyms->st_shndx == data_index)
		p = (unsigned long *) (pdata + psyms->st_value);
	    else {
		fprintf(stderr, "%s: %s Bad data section index: %d \n",
			program, name, psyms->st_shndx);
		exit(1);
	    }
	    printf("%s %lx\n", name, *p );
	}
    }
}

#define VTMAGIC1 "_vt."

void extract_virtual_table_size_direct()
{
    int i;
    for( i = 0; i < nsyms; i++, psyms++ ) {
	char   *name = &pstr[psyms->st_name];
	if( strncmp( name, VTMAGIC1, strlen(VTMAGIC1) ) == 0 ) {
	    printf("%s: %lu -> %lu\n", name, psyms->st_size,
		   psyms->st_size / sizeof(Ptr_to_mem_func));
	}
    }
}

#define VTMAGIC2 "__VAR_SIZE_VIRTUAL__"

void extract_virtual_table_size_indirect()
{
    int i;
    for( i = 0; i < nsyms; i++, psyms++ ) {
	Ptr_to_mem_func	*p;
	char *name = &pstr[psyms->st_name];

	if( strncmp( name, VTMAGIC2, strlen(VTMAGIC2) ) == 0 ) {

	    /* the symbol has the magic "__Virtual__" infront of it
	     * get its initialization value, because the value obtains
	     * the index the delta etc of the virtual function invocation
	     */

	    if (psyms->st_shndx == sdata_index)
		p = (Ptr_to_mem_func *) (psdata + psyms->st_value);
	    else if (psyms->st_shndx == data_index)
		p = (Ptr_to_mem_func *) (pdata + psyms->st_value);
	    else {
		fprintf(stderr, "%s: %s Bad data section index: %d \n",
			program, name, psyms->st_shndx);
		exit(1);
	    }

	    if (!(MEMFCT_IS_VIRTUAL(p))) {
		fprintf(stderr, "%s: %s Bad method ptr: %p %x %x \n",
			program, name,
			MEMFCT_GET_FADDR(p), p->index, p->delta );
		exit(1);
	    }

	    printf("%s %d\n", name, MEMFCT_GET_INDEX(p) );
	}
    }
}

int
main( int argc, char **argv )
{
    int         fd;
    Elf_Scn    *section;
    int         scndx;
    Elf32_Shdr *sheader;
    Elf32_Ehdr *eheader;
    int         strings;
    int         len;
    int         what_to_do;
    Elf        *elf;
    char       *filep;

    if( argc != 3 ) {
	fprintf(stderr, "%s: <what> <filename>\n", argv[0]);
	return(1);
    }

    if (strcmp("vindex",argv[1]) == 0)
	what_to_do = 1;
    else if (strcmp("type",argv[1]) == 0)
	what_to_do = 2;
    else if (strcmp("vtable",argv[1]) == 0)
	what_to_do = 3;
    else {
	fprintf(stderr, "%s: unknown command> %s\n", argv[0], argv[1] );
	return(1);
    }

    sprintf(program,"%s<%s>",argv[0],argv[1]);
    elf_version(EV_CURRENT);
    fd = open(argv[2], OPEN_FLAGS);
    if( fd < 0 ) {
	fprintf(stderr, "%s: open of file %s failed\n", argv[0], argv[2] );
	return(1);
    }

    len = lseek(fd, 0, SEEK_END);

#if MAPFILE
    /* printf("Mapping file %s [len %d]\n", argv[1], len); */
    filep = (char *) mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,fd,0);
    if (filep == (char *)-1) {
	perror("mmap");
	return(1);
    }
#else
    filep = (char*)malloc(len);
    lseek(fd,0,SEEK_SET);
    if (len != read(fd,filep,len)) {
	perror("mmap");
	return(1);
    }
#endif
    elf = elf_begin(fd, ELF_C_READ, (Elf *)NULL);

    if ((eheader = elf32_getehdr(elf)) == NULL) {
	fprintf(stderr, "%s: get header failed\n", argv[0] );
	return(1);
    }

    strings = eheader->e_shstrndx;

    section = NULL;
    scndx = 0;
    while ((section = elf_nextscn(elf, section)) != NULL) {
	char    *name = 0;

	++scndx;
	if ((sheader = elf32_getshdr(section)) != NULL)
	    name = elf_strptr(elf, strings, sheader->sh_name);
	if( name != NULL ) {
//	    printf("SECTION[%d]: <%s>\n",(int)section,name);
	    if( strcmp( name, THE_DATA_SECTION ) == 0 ) {
		pdata = (char *) (filep + sheader->sh_offset);
		data_index = scndx;
	    } else if( strcmp( name, SMALL_DATA_SECTION ) == 0 ) {
		psdata = (char *) (filep + sheader->sh_offset);
		sdata_index = scndx;
	    } else if( strcmp( name, ELF_SYMTAB ) == 0 ) {
		psyms = (Elf32_Sym *) (filep + sheader->sh_offset);
		nsyms = sheader->sh_size / sizeof(Elf32_Sym);
	    } else if( strcmp( name, ELF_STRTAB ) == 0 ) {
		pstr  = (char *) (filep + sheader->sh_offset);
	    }
	}
    }

    if( pdata == NULL && psdata == NULL ) {
	fprintf(stderr, "%s: no data segment found\n", argv[0] );
	return(1);
    }
    if( psyms == NULL ) {
	fprintf(stderr, "%s: no symbol table found\n", argv[0] );
	return(1);
    }
    if( pstr == NULL ) {
	fprintf(stderr, "%s: no string table found\n", argv[0] );
	return(1);
    }

    switch (what_to_do) {
    case 1:
	extract_virtual_index();
	break;

    case 2:
	extract_type_values();
	break;

    case 3:
	extract_virtual_table_size_indirect();
	break;
    }
    elf_end(elf);

    return(0);
}
