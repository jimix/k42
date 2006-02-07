/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ext_xcoff.c,v 1.14 2001/11/27 17:56:06 peterson Exp $
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define __XCOFF64__
#include <a.out.h>
#ifdef FREAD
#undef FREAD
#undef FWRITE
#endif /* #ifdef FREAD */
#include <ldfcn.h>

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
    uint64_t faddr;
    int index;
    int delta;
    int v_off;
} Ptr_to_mem_func;

#define MEMFCT_IS_VIRTUAL(m) ((m)->faddr != NULL)
#define MEMFCT_GET_INDEX(m)  (int64_t)((m)->index/4)
#define MEMFCT_GET_FADDR(m)  ((m)->faddr)
#define MEMFCT_GET_DELTA(m)  (int64_t)((m)->delta)
#endif /* #if (defined(_POWER) || ... */

#if defined(__GNUC__)
#if GCC_CLASS_VERSION >= 3
/*
 * We are only interested in 64bit targets thought this program is
 * likely 32bit it may be 64bits as well.
 */
typedef struct {
  union {
      int64_t index;		/* was long */
      uint64_t faddr;		/* was void * */
  } u;
  int64_t delta;		/* was long */
} Ptr_to_mem_func;
/* no multiple inheritance is allowed */
#define MEMFCT_IS_VIRTUAL(m) (((m)->u.index % sizeof((m)->u)) != 0)
#define MEMFCT_GET_INDEX(m)  (((m)->u.index - 1) / sizeof((m)->u))
#define MEMFCT_GET_FADDR(m)  ((m)->u.faddr)
#define MEMFCT_GET_DELTA(m)  ((m)->delta)

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
#endif /* #if defined(__GNUC__) */

/**************************************************************************/

LDFILE	*ldptr;
size_t	nsyms;
SCNHDR  *pdhdr;
void	*pdata;

char	program[128];
#ifndef _AIX43
extern char *ldgetname(LDFILE *, SYMENT *);
#endif /* #ifndef _AIX43 */

#define VMAGIC	"__Virtual__"
#define SMAGIC	"__Static__"

void extract_virtual_index()
{
    int sym_index=0;
    int index;

    while (sym_index < nsyms) {
	SYMENT symbol;
	Ptr_to_mem_func *p;

	if (ldtbread (ldptr, sym_index, &symbol) <= 0)
	    break;
	sym_index += symbol.n_numaux + 1;
	if ((symbol.n_sclass == C_EXT) &&
	    symbol.n_scnum > N_UNDEF) {
	    char *name;

	    if ((name = ldgetname (ldptr, &symbol)) == NULL)
		continue;
	    if (*name == '.')
		++name;

	    if (strncmp (name, VMAGIC, strlen(VMAGIC)) == 0) {
	    /* The symbol has the magic "__Virtual__" in front of
	     * it. Get its initialization value, because the value
	     * obtains the index, the delta, etc of the virtual
	     * function invocation */
		if (symbol.n_scnum == 2) {
		    p = (Ptr_to_mem_func *)
			(pdata + symbol.n_value - pdhdr->s_paddr);
		} else {
		    fprintf(stderr, "%s: Bad section index %d for %s\n",
			    program, symbol.n_scnum, name);
		    exit(1);
		}
		if (!(MEMFCT_IS_VIRTUAL(p))) {
		    fprintf(stderr, "%s: %s Bad method ptr: %llx %llx %llx \n",
			    program, name, MEMFCT_GET_FADDR(p),
			    MEMFCT_GET_INDEX(p), MEMFCT_GET_DELTA(p));
		    exit(1);
		}
		index = MEMFCT_GET_INDEX(p);
		/* for powerpc and mips, under the old GCC, we
		   need to decrease the index by one */
		index -= 1;
		printf("%d\n", index );
	    } else if (strncmp(name, SMAGIC, strlen(SMAGIC)) == 0 ) {
		puts("-1");
	    }
	}
    }
}

#define TMAGIC	"__TYPE_"

void extract_type_values()
{
    int sym_index=0;

    while (sym_index < nsyms) {
	SYMENT symbol;

	if (ldtbread (ldptr, sym_index, &symbol) <= 0)
	    break;
	sym_index += symbol.n_numaux + 1;
	if (symbol.n_sclass == C_EXT &&
	    symbol.n_scnum > N_UNDEF) {
	    char *name;
	    char *newname;

	    if ((name = ldgetname (ldptr, &symbol)) == NULL)
		continue;
	    if (*name == '.')
		++name;
	    newname = strstr(name, TMAGIC);
	    if (newname != NULL) {
		unsigned long *p;
		if (symbol.n_scnum == 2) {
		    p = pdata + symbol.n_value - pdhdr->s_paddr;
		} else {
		    fprintf(stderr, "%s: Bad section index %d for %s\n",
			    program, symbol.n_scnum, name);
		    exit(1);
		}
		printf("%s %lx\n", newname, *p );
	    }
	}
    }
}

#if __GNUC__ >= 3
#define VTMAGIC1 "_ZTV"
#else /* #if __GNUC__ >= 3 */
#define VTMAGIC1 "_vt."
#endif /* #if __GNUC__ >= 3 */

void extract_virtual_table_size_direct()
{
    int sym_index=0;

    while (sym_index < nsyms) {
	SYMENT symbol;

	if (ldtbread (ldptr, sym_index, &symbol) <= 0)
	    break;
	sym_index += symbol.n_numaux + 1;
	if (symbol.n_sclass == C_EXT &&
	    symbol.n_scnum > N_UNDEF) {
	    char *name;

	    if ((name = ldgetname (ldptr, &symbol)) == NULL)
		continue;
	    if (*name == '.')
		++name;

	    if (strncmp (name, VTMAGIC1, strlen(VTMAGIC1)) == 0 ) {
	    }
	}
    }
}

#define VTMAGIC2 "__VAR_SIZE_VIRTUAL__"

void extract_virtual_table_size_indirect()
{
    int sym_index=0;

    while (sym_index < nsyms) {
	SYMENT symbol;
	Ptr_to_mem_func	*p;

	if (ldtbread (ldptr, sym_index, &symbol) <= 0)
	    break;
	sym_index += symbol.n_numaux + 1;
	if (symbol.n_sclass == C_EXT &&
	    symbol.n_scnum > N_UNDEF) {
	    char *name;

	    if ((name = ldgetname (ldptr, &symbol)) == NULL)
		continue;
	    if (*name == '.')
		++name;

	    if (strncmp( name, VTMAGIC2, strlen(VTMAGIC2)) == 0 ) {
	    /* the symbol has the magic "__VAR_SIZE_VIRTUAL__" infront of it
	     * get its initialization value, because the value obtains
	     * the index the delta etc of the virtual function invocation
	     */
		if (symbol.n_scnum == 2) {
		    p = (Ptr_to_mem_func *)
			(pdata + symbol.n_value - pdhdr->s_paddr);
		} else {
		    fprintf(stderr, "%s: Bad section index %d for %s\n",
			    program, symbol.n_scnum, name);
		    exit(1);
		}
		if (!(MEMFCT_IS_VIRTUAL(p))) {
		    fprintf(stderr, "%s: %s Bad method ptr: %llx %llx %llx \n",
			    program, name, MEMFCT_GET_FADDR(p),
			    MEMFCT_GET_INDEX(p), MEMFCT_GET_DELTA(p));
		    exit(1);
		}
		printf("%s %lld\n", name, MEMFCT_GET_INDEX(p));
	    }
	}
    }
}

int
main( int argc, char **argv )
{
    int         what_to_do;

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
    if ((ldptr = ldopen(argv[2], ldptr)) == NULL) {
	fprintf(stderr, "%s: cannot open as COFF file: %s", argv[0], argv[2]);
	return(1);
    }
    if ((HEADER (ldptr).f_magic != U802TOCMAGIC)
	&& (HEADER (ldptr).f_magic != 0757)) {
	fprintf(stderr, "%s: not a COFF file: %s", argv[0], argv[2]);
	return(1);
    }

    nsyms = HEADER(ldptr).f_nsyms;

    pdhdr = malloc(SCNHSZ);

    if (ldshread (ldptr, 2, pdhdr) <= 0) {
	fprintf(stderr, "%s: cannot access data section header", argv[0]);
	return(1);
    }

    pdata = malloc(pdhdr->s_size);
    FSEEK(ldptr, pdhdr->s_scnptr, 0);
    FREAD(pdata, pdhdr->s_size, 1, ldptr);

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

    (void) ldclose(ldptr);

    return(0);
}
