#ifndef __LIBELF64_H_
#define __LIBELF64_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: libelf64.h,v 1.7 2002/07/09 15:09:25 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     Basic <incomplete> definition of libelf interface as used by the
 *     stubcompiler this is not a complete implementation but only those
 *     functions and definitions we currently need. For completion and
 *     extensions according to proper usage please extended in accordance
 *     to some standard manual.
 * **************************************************************************/

#include <sys/types.h>
#include <gelf.h>

#ifdef __STDC__
	typedef void		Elf_Void;
#else /* #ifdef __STDC__ */
	typedef char		Elf_Void;
#endif /* #ifdef __STDC__ */


/*	commands
 */

typedef enum {
	ELF_C_NULL = 0,	/* must be first, 0 */
	ELF_C_READ,
	ELF_C_WRITE,
	ELF_C_RDWR
} Elf_Cmd;


typedef struct Elf	Elf;
typedef struct Elf_Scn	Elf_Scn;

typedef enum {
        ELF_T_BYTE = 0, /* must be first, 0 */
        ELF_T_ADDR,
        ELF_T_DYN,
        ELF_T_EHDR,
        ELF_T_HALF,
        ELF_T_OFF,
        ELF_T_PHDR,
        ELF_T_RELA,
        ELF_T_REL,
        ELF_T_SHDR,
        ELF_T_SWORD,
        ELF_T_SXWORD,
        ELF_T_SYM,
        ELF_T_WORD,
        ELF_T_XWORD,
        ELF_T_NUM       /* must be last */
} Elf_Type;

/*	data descriptor
 */

typedef struct {
	Elf_Void	*d_buf;
	Elf_Type	d_type;
	size_t		d_size;
	off_t		d_off;		/* offset into section */
	size_t		d_align;	/* alignment in section */
	unsigned	d_version;	/* elf version */
} Elf_Data;


/*	function declarations
 */

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */


/* For endian-safe acceses of elf data structure fields */
extern int endian_elf;
extern int endian_host;

static inline u_int16_t swap_16(u_int16_t x) {
    return ((0xff00 & x)>>8) | ((0xff & x)<<8);
}

static inline u_int32_t swap_32(u_int32_t x) {
    return swap_16((0xffff0000 & x)>>16) | (swap_16(0xffff & x)<<16);
}

static inline u_int64_t swap_64(u_int64_t x) {
    return swap_32((0xffffffff00000000ULL & x)>>32ULL) |
	(((u_int64_t)swap_32(0xffffffffULL & x))<<32ULL);
}

#define E_HALF(x) (endian_elf == endian_host ? (x) : swap_16(x))
#define E_WORD(x) (endian_elf == endian_host ? (x) : swap_32(x))
#define E_OFF(x)  (endian_elf == endian_host ? (x) : swap_64(x))

/* And we add our own stuff to it */

Elf		*elf_begin	(int, Elf_Cmd, Elf *);
int		 elf_end	(Elf *);
void             elf_read       (Elf *elf, unsigned int ofs, void *buf, size_t size);
void*            elf_alloc_read (Elf *elf, unsigned int ofs, size_t size);
unsigned	 elf_version	(unsigned);
char		*elf_getident	(Elf *, size_t *);
Elf_Scn		*elf_nextscn	(Elf *, Elf_Scn *);
char		*elf_strptr	(Elf *, size_t, size_t);
Elf_Scn		*elf_nextscn	(Elf *, Elf_Scn *);
Elf_Data	*elf_getdata	(Elf_Scn *, Elf_Data *);

Elf64_Ehdr	*elf64_getehdr	(Elf *);
Elf64_Shdr	*elf64_getshdr	(Elf_Scn *);


Elf64_Shdr	*elf64_getshdr	(Elf_Scn *);
Elf64_Ehdr	*elf64_getehdr	(Elf *);


#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#ifdef TRACE_DEBUG
#define DPRINTF(x) fprintf(stderr,"%s,line %d: ",__FILE__,__LINE__); fprintf x
#else /* #ifdef TRACE_DEBUG */
#define DPRINTF(x)
#endif /* #ifdef TRACE_DEBUG */

#endif /* #ifndef __LIBELF64_H_ */
