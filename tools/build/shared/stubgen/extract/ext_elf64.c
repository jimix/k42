/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ext_elf64.c,v 1.21 2005/06/10 02:42:32 apw Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    This module accesses a 64bit Elf File and determines the index
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
 *
 *
 *
 *
 *
 *  The program takes two commands:
 *
 *   (a) a command
 *   (b) an elf file name
 *
 * **************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <libelf64.h>

#ifndef DPRINTF
#ifdef TRACE_DEBUG
#define DPRINTF(x) fprintf(stderr,"%s,line %d: ",__FILE__,__LINE__); fprintf x
#else /* #ifdef TRACE_DEBUG */
#define DPRINTF(x)
#endif /* #ifdef TRACE_DEBUG */
#endif /* #ifndef DPRINTF */

#ifdef O_BINARY  /* in case the architecture cares */
#define OPEN_FLAGS (O_RDWR | O_BINARY)
#else /* #ifdef O_BINARY */
#define OPEN_FLAGS (O_RDWR)
#endif /* #ifdef O_BINARY */


enum what
{
  GET_NOTHING,
  GET_VINDEX,
  GET_VTABLE,
  GET_TYPE,
};

/**************************************************************************
 * MEMBER FUNCTIONS POINTER:
 * Architectures and compilers define the layout of member function
 * pointers in various ways. Since we are looking for static
 * initializers of member function pointer variables we must know the
 * layout. In order to define for new ARCH/COMP combination restructure
 * the layout of the <Ptr_to_mem_func> and define MACROS
 *

 #define MEMFCT_IS_VIRTUAL(m) // is the memberfunction a valid virtual func
 #define MEMFCT_GET_INDEX(m)  // get the index.. sometimes byte index
 #define MEMFCT_GET_FADDR(m)  // get the address of the non-virt func
 #define THE_DATA_SECTION  ELF_DATA  // which data segment to look for
 #define SMALL_DATA_SECTION  ELF_SDATA  // name of small data segment
 #define THE_BSS_SECTION  ELF_BSS    // name of bss segment
 *
 **************************************************************************/

/* remember that we need to know the data structure layouts for
   the code with is compiled -- so we need the information for
   the cross compiler that will be used by stubgen, and its
   target platform -- NOT (necessarily) the same as we are
   currently using to build this tool.  Hence, we pass
   GCC_CLASS_VERSION in, we do not use __GCC__ (which is the
   compiler for this tool).
*/

#if GCC_CLASS_VERSION >= 3
typedef struct {
  union {
    int64_t index;
    uint64_t faddr;
  } u;
  int64_t delta;
} Ptr_to_mem_func;
/* no multiple inheritance is allowed */
#define MEMFCT_IS_VIRTUAL(m) ((E_OFF((m)->u.index) % sizeof((m)->u)) != 0)
#define MEMFCT_GET_INDEX(m)  ((E_OFF((m)->u.index) - 1) / sizeof((m)->u))
#define MEMFCT_GET_FADDR(m)  (E_OFF((m)->u.faddr))
#define MEMFCT_GET_DELTA(m)  (E_OFF((m)->delta))

#else /* #if GCC_CLASS_VERSION >= 3 */

typedef struct {
    short delta;
    short index;
    union {
	uint64_t faddr;
	short v_off;
    } u;
} Ptr_to_mem_func;
/* no multiple inheritance is allowed */
#define MEMFCT_IS_VIRTUAL(m) (((m)->index >= 0))
#define MEMFCT_GET_INDEX(m)  (int64_t)((m)->index)
#define MEMFCT_GET_FADDR(m)  ((m)->u.faddr)
#define MEMFCT_GET_DELTA(m)  (int64_t)((m)->delta)
#endif /* #if GCC_CLASS_VERSION >= 3 */

#define THE_DATA_SECTION     ELF_DATA
#define SMALL_DATA_SECTION   ELF_SDATA
#define THE_BSS_SECTION      ELF_BSS


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


size_t      nsyms;
Elf64_Sym  *psyms = NULL;

char       *pdata = NULL;
int        data_index = 0;
char       *psdata = NULL;
int        sdata_index = 0;
char       *pbss = NULL;
int	   bss_index = 0;

char       *pstr  = NULL;
char       program[128];



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

#define VMAGIC	"__Virtual__"
#define SMAGIC	"__Static__"


void extract_virtual_index()
{
    int i;

    DPRINTF((stderr,"extract_virtual_index() with GCC_CLASS_VERSION = %d\n", GCC_CLASS_VERSION));

    for ( i = 0; i < nsyms; i++, psyms++ ) {
	Ptr_to_mem_func	*p;
	char *name = &pstr[E_WORD(psyms->st_name)];
	uint64_t index;

	DPRINTF((stderr,"next symbol: %s section %d, value %d, size %d info 0x%X\n", name, psyms->st_shndx, psyms->st_value, psyms->st_size, psyms->st_info));
	if ( strncmp( name, VMAGIC, strlen(VMAGIC) ) == 0 ) {

	    /* the symbol has the magic "__Virtual__" in front of it
	     * get its initialization value, because the value obtains
	     * the index the delta etc of the virtual function invocation
	     */

	  DPRINTF((stderr,"found %s, section %d\n", name, psyms->st_shndx));
	    if (E_HALF(psyms->st_shndx) == sdata_index)
		p = (Ptr_to_mem_func *) (psdata + E_OFF(psyms->st_value));
	    else if (E_HALF(psyms->st_shndx) == data_index)
		p = (Ptr_to_mem_func *) (pdata + E_OFF(psyms->st_value));
	    else if (E_HALF(psyms->st_shndx) == bss_index)
		p = (Ptr_to_mem_func *) (pbss + E_OFF(psyms->st_value));
	    else {
		fprintf(stderr, "%s: %s Bad data section index: %d \n",
			program, name, E_HALF(psyms->st_shndx));
		continue;
	    }

	    if (!(MEMFCT_IS_VIRTUAL(p))) {
		fprintf(stderr, "%s: %s Bad method ptr: %llx %llx %llx \n",
			program, name, 
			(unsigned long long)MEMFCT_GET_FADDR(p),
			(unsigned long long)MEMFCT_GET_INDEX(p), 
			(unsigned long long)MEMFCT_GET_DELTA(p) );
		continue;
	    }

	    index = MEMFCT_GET_INDEX(p);
#if GCC_CLASS_VERSION < 3
	    /* for powerpc and mips, under the old GCC, we
	       need to decrease the index by one */
	    index -= 1;
#endif /* #if GCC_CLASS_VERSION < 3 */
	    fprintf(stdout,"%lld %lld ", (long long)index, 
			    (long long)E_OFF(psyms->st_value));
	    fprintf(stdout,"%s\n", name );
	} else if ( strncmp( name, SMAGIC, strlen(SMAGIC) ) == 0 ) {
	    fprintf(stdout,"%d\n", -1 );
	}
    }
}



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

#define VTMAGIC2 "__VAR_SIZE_VIRTUAL__"

void extract_virtual_table_size_indirect()
{
    int i;

    DPRINTF((stderr,"extract_virtual_table_size_indirect()\n"));

    for ( i = 0; i < nsyms; i++, psyms++ ) {
	Ptr_to_mem_func	*p;
	char *name = &pstr[E_WORD(psyms->st_name)];

	DPRINTF((stderr,"check name %d: %s\n", i, name));
	if ( strncmp( name, VTMAGIC2, strlen(VTMAGIC2) ) == 0 ) {

	    /* the symbol has the magic "__Virtual__" in front of it
	     * get its initialization value, because the value obtains
	     * the index the delta etc of the virtual function invocation
	     */

	    if (E_HALF(psyms->st_shndx) == sdata_index)
		p = (Ptr_to_mem_func *) (psdata + E_OFF(psyms->st_value));
	    else if (E_HALF(psyms->st_shndx) == data_index)
		p = (Ptr_to_mem_func *) (pdata + E_OFF(psyms->st_value));
	    else if (E_HALF(psyms->st_shndx) == bss_index)
		p = (Ptr_to_mem_func *) (pbss + E_OFF(psyms->st_value));
	    else {
		fprintf(stderr, "*** %s: %s Bad data section index: %d \n",
			program, name, E_HALF(psyms->st_shndx));
		exit(1);
	    }

	    if (!(MEMFCT_IS_VIRTUAL(p))) {
	      fprintf(stderr, "*** %s: %s Bad method ptr: %llx %llx %llx \n",
		      program, name, 
		      (unsigned long long)MEMFCT_GET_FADDR(p),
		      (unsigned long long)MEMFCT_GET_INDEX(p), 
		      (unsigned long long)MEMFCT_GET_DELTA(p));
	      exit(1);
	    }

	    fprintf(stdout,"%s %lld\n", name, (long long)MEMFCT_GET_INDEX(p));
	}
    }
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

#define TMAGIC	"__TYPE_"

void extract_type_values()
{
    int i;
    DPRINTF((stderr,"extract_type_values()\n"));

    for ( i = 0; i < nsyms; i++, psyms++ ) {
        char *name;
        char *newname;
	char *base;
	uint64_t *p;

	// get the name for this symbol table entry
	name = &pstr[E_WORD(psyms->st_name)];
	DPRINTF((stderr,"%s: %d %x\n", name, E_WORD(psyms->st_size),
		 E_WORD(psyms->st_value)));

	// check if the name has TMAGIC as a substring
	newname = strstr(name, TMAGIC);
	if (newname != NULL)
	  {
	    // determine the base for the section for this symbol
	    if (E_HALF(psyms->st_shndx) == sdata_index)
	      base = psdata;
	    else if (E_HALF(psyms->st_shndx) == data_index)
	      base = pdata;
	    else if (E_HALF(psyms->st_shndx) == bss_index)
	      base = pbss;
	    else {
	      fprintf(stderr, "*** %s: %s Bad data section index: %d \n",
		      program, name, psyms->st_shndx);
	      exit(1);
	    }

	    // skip over the __TYPE_ part of the name
	    // compute the address of the symbol value
	    newname += sizeof(TMAGIC)-1;
	    p = (uint64_t *) (base + E_WORD(psyms->st_value));

	    // print the (truncated) name and its value
	    fprintf(stdout,"%s %lld\n", newname, (long long)*p);
	  }
    }
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

int
main( int argc, char **argv )
{
    enum what   what_to_do;
    char       *filename;

    int         fd;
    Elf_Scn    *section;
    int         scndx;
    Elf64_Shdr *sheader;
    Elf64_Ehdr *eheader;
    int         strings;
    int         len;
    Elf        *elf;
    char       *filep;

    if ( argc != 3 ) {
	fprintf(stderr, "%s: <what> <filename>\n", argv[0]);
	return (1);
    }

    if (strcmp("vindex",argv[1]) == 0)
        what_to_do = GET_VINDEX;
    else if (strcmp("vtable",argv[1]) == 0)
        what_to_do = GET_VTABLE;
    else if (strcmp("type",argv[1]) == 0)
        what_to_do = GET_TYPE;
    else {
	fprintf(stderr, "%s: unknown command> %s\n", argv[0], argv[1] );
	return (1);
    }
    sprintf(program,"%s<%s>",argv[0],argv[1]);

    filename = argv[2];

    /* ******************************************************** */
    /*								*/
    /*								*/
    /* ******************************************************** */

#ifdef PLATFORM_IRIX64
    /* The Irix version, running on mips, uses a different elf library
       than the Linux version.  Use "man elf_begin" to get an explanation
       of how it is supposed to work.  */
    elf_version(EV_CURRENT);
#endif /* #ifdef PLATFORM_IRIX64 */

    fd = open(filename, OPEN_FLAGS);
    if ( fd < 0 ) {
	fprintf(stderr, "%s: open of file %s failed\n", program, filename );
	return (1);
    }

    len = lseek(fd, 0, SEEK_END);

#if MAPFILE
    DPRINTF((stderr,"m-Mapping file %s [len %d]\n", filename, len));
    filep = (char *) mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,fd,0);
    if (filep == (char *)-1) {
	perror("mmap");
	return (1);
    }
#else /* #if MAPFILE */
    filep = (char*)malloc(len);
    lseek(fd,0,SEEK_SET);
    if (len != read(fd,filep,len)) {
	perror("malloc/read");
	return (1);
    }
#endif /* #if MAPFILE */

    elf = elf_begin(fd, ELF_C_READ, (Elf *)NULL);
    if (elf == NULL)
      {
	fprintf(stderr, "*** %s: %s: read of elf file failed\n", program, filename);
	return (1);
      }

    eheader = elf64_getehdr(elf);
    if (eheader == NULL) {
	fprintf(stderr, "*** %s: %s: get header failed\n", program, filename);
	return (1);
    }

    strings = E_HALF(eheader->e_shstrndx);

    section = NULL;
    scndx = 0;
    while ((section = elf_nextscn(elf, section)) != NULL) {
	char    *name = 0;

	++scndx;
	if ((sheader = elf64_getshdr(section)) != NULL)
	    name = elf_strptr(elf, strings, E_WORD(sheader->sh_name));
	if ( name != NULL ) {
	  DPRINTF((stderr,"SECTION[%d]: <%s>\n",scndx,name));
	    if ( strcmp( name, THE_DATA_SECTION ) == 0 ) {
		pdata = (char *) (filep + E_OFF(sheader->sh_offset));
		data_index = scndx;
		DPRINTF((stderr,"found data section %d\n", data_index));
	    } else if ( strcmp( name, SMALL_DATA_SECTION ) == 0 ) {
		psdata = (char *) (filep + E_OFF(sheader->sh_offset));
		sdata_index = scndx;
		DPRINTF((stderr,"found small data section %d\n", sdata_index));
	    } else if ( strcmp( name, THE_BSS_SECTION ) == 0 ) {
		pbss = (char *) (filep + E_OFF(sheader->sh_offset));
		bss_index = scndx;
		DPRINTF((stderr,"found BSS section %d\n", bss_index));
	    } else if ( strcmp( name, ELF_SYMTAB ) == 0 ) {
		psyms = (Elf64_Sym *) (filep + E_OFF(sheader->sh_offset));
		nsyms = E_OFF(sheader->sh_size) / sizeof(Elf64_Sym);
		DPRINTF((stderr,"section %d: symbol table:  %d symbols\n", scndx, nsyms));
	    } else if ( strcmp( name, ELF_STRTAB ) == 0 ) {
		pstr  = (char *) (filep + E_OFF(sheader->sh_offset));
		DPRINTF((stderr,"section %d: string table\n", scndx));
	    }
	}
    }

    /* ******************************************************** */
    /*								*/
    /*								*/
    /* ******************************************************** */

    if ( pdata == NULL && psdata == NULL ) {
	fprintf(stderr, "*** %s: %s: no data segment found\n", program, filename);
	what_to_do = GET_NOTHING;
    }
    if ( psyms == NULL ) {
	fprintf(stderr, "*** %s: %s: no symbol table found\n", program, filename);
	what_to_do = GET_NOTHING;
    }
    if ( pstr == NULL ) {
	fprintf(stderr, "*** %s: %s: no string table found\n", program, filename);
	what_to_do = GET_NOTHING;
    }


    /* ******************************************************** */
    /*								*/
    /*								*/
    /* ******************************************************** */

    switch (what_to_do) {
    case GET_NOTHING:
	break;

    case GET_VINDEX:
	extract_virtual_index();
	break;

    case GET_VTABLE:
	extract_virtual_table_size_indirect();
	break;

    case GET_TYPE:
	extract_type_values();
	break;
    }
    elf_end(elf);

    return (0);
}
