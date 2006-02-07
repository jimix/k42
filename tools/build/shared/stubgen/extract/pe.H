/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: pe.H,v 1.7 2001/05/30 13:40:11 peterson Exp $
 *****************************************************************************/

/* PE COFF header information */

#ifndef _PE_H
#define _PE_H

/* NT specific file attributes */
#define IMAGE_FILE_RELOCS_STRIPPED           0x0001
#define IMAGE_FILE_EXECUTABLE_IMAGE          0x0002
#define IMAGE_FILE_LINE_NUMS_STRIPPED        0x0004
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED       0x0008
#define IMAGE_FILE_BYTES_REVERSED_LO         0x0080
#define IMAGE_FILE_32BIT_MACHINE             0x0100
#define IMAGE_FILE_DEBUG_STRIPPED            0x0200
#define IMAGE_FILE_SYSTEM                    0x1000
#define IMAGE_FILE_DLL                       0x2000
#define IMAGE_FILE_BYTES_REVERSED_HI         0x8000

/* additional flags to be set for section headers to allow the NT loader to
   read and write to the section data (to replace the addresses of data in
   dlls for one thing); also to execute the section in .text's case */
#define IMAGE_SCN_MEM_DISCARDABLE 0x02000000
#define IMAGE_SCN_MEM_EXECUTE     0x20000000
#define IMAGE_SCN_MEM_READ        0x40000000
#define IMAGE_SCN_MEM_WRITE       0x80000000

/*
 * Section characteristics added for ppc-nt
 */

#define IMAGE_SCN_TYPE_NO_PAD                0x00000008  /* Reserved. */

#define IMAGE_SCN_CNT_CODE                   0x00000020  /* Section contains code. */
#define IMAGE_SCN_CNT_INITIALIZED_DATA       0x00000040  /* Section contains initialized data. */
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA     0x00000080  /* Section contains uninitialized data. */

#define IMAGE_SCN_LNK_OTHER                  0x00000100  /* Reserved. */
#define IMAGE_SCN_LNK_INFO                   0x00000200  /* Section contains comments or some other type of information. */
#define IMAGE_SCN_LNK_REMOVE                 0x00000800  /* Section contents will not become part of image. */
#define IMAGE_SCN_LNK_COMDAT                 0x00001000  /* Section contents comdat. */

#define IMAGE_SCN_MEM_FARDATA                0x00008000

#define IMAGE_SCN_MEM_PURGEABLE              0x00020000
#define IMAGE_SCN_MEM_16BIT                  0x00020000
#define IMAGE_SCN_MEM_LOCKED                 0x00040000
#define IMAGE_SCN_MEM_PRELOAD                0x00080000

#define IMAGE_SCN_ALIGN_1BYTES               0x00100000
#define IMAGE_SCN_ALIGN_2BYTES               0x00200000
#define IMAGE_SCN_ALIGN_4BYTES               0x00300000
#define IMAGE_SCN_ALIGN_8BYTES               0x00400000
#define IMAGE_SCN_ALIGN_16BYTES              0x00500000  /* Default alignment if no others are specified. */
#define IMAGE_SCN_ALIGN_32BYTES              0x00600000
#define IMAGE_SCN_ALIGN_64BYTES              0x00700000


#define IMAGE_SCN_LNK_NRELOC_OVFL            0x01000000  /* Section contains extended relocations. */
#define IMAGE_SCN_MEM_NOT_CACHED             0x04000000  /* Section is not cachable.               */
#define IMAGE_SCN_MEM_NOT_PAGED              0x08000000  /* Section is not pageable.               */
#define IMAGE_SCN_MEM_SHARED                 0x10000000  /* Section is shareable.                  */

/* COMDAT selection codes.  */

#define IMAGE_COMDAT_SELECT_NODUPLICATES     (1) /* Warn if duplicates.  */
#define IMAGE_COMDAT_SELECT_ANY		     (2) /* No warning.  */
#define IMAGE_COMDAT_SELECT_SAME_SIZE	     (3) /* Warn if different size.  */
#define IMAGE_COMDAT_SELECT_EXACT_MATCH	     (4) /* Warn if different.  */
#define IMAGE_COMDAT_SELECT_ASSOCIATIVE	     (5) /* Base on other section.  */

/* Magic values that are true for all dos/nt implementations */
#define DOSMAGIC       0x5a4d
#define NT_SIGNATURE   0x00004550

#define FILNMLEN	18	/* # characters in a file name		*/

typedef struct PE_hdr {
    /* DOS header fields */
    uval16 e_magic;		/* Magic number, 0x5a4d */
    uval16 e_cblp;		/* Bytes on last page of file, 0x90 */
    uval16 e_cp;			/* Pages in file, 0x3 */
    uval16 e_crlc;		/* Relocations, 0x0 */
    uval16 e_cparhdr;		/* Size of header in paragraphs, 0x4 */
    uval16 e_minalloc;		/* Minimum extra paragraphs needed, 0x0 */
    uval16 e_maxalloc;		/* Maximum extra paragraphs needed, 0xFFFF */
    uval16 e_ss;			/* Initial (relative) SS value, 0x0 */
    uval16 e_sp;			/* Initial SP value, 0xb8 */
    uval16 e_csum;		/* Checksum, 0x0 */
    uval16 e_ip;			/* Initial IP value, 0x0 */
    uval16 e_cs;			/* Initial (relative) CS value, 0x0 */
    uval16 e_lfarlc;		/* File address of relocation table, 0x40 */
    uval16 e_ovno;		/* Overlay number, 0x0 */
    uval32 e_res[2];		/* Reserved words, all 0x0 */
    uval16 e_oemid;		/* OEM identifier (for e_oeminfo), 0x0 */
    uval16 e_oeminfo;		/* OEM information; e_oemid specific, 0x0 */
    char e_res2[10][2];		/* Reserved words, all 0x0 */
    uval32 e_lfanew;		/* File address of new exe header, 0x80 */
    char dos_message[16][4];	/* other stuff, always follow DOS header */
    uval32 nt_signature;		/* required NT signature, 0x4550 */

    /* From standard header */

    uval16 f_magic;		/* magic number			*/
    uval16 f_nscns;		/* number of sections		*/
    uval32 f_timdat;		/* time & date stamp		*/
    uval32 f_symptr;		/* file pointer to symtab	*/
    uval32 f_nsyms;		/* number of symtab entries	*/
    uval16 f_opthdr;		/* sizeof(optional hdr)		*/
    uval16 f_flags;		/* flags			*/

} PE_hdr;

#define FILHSZ 152

/* standard file header size (e.g. object files ) */
typedef struct PE_stdfilehdr {
    uval16 f_magic;		/* magic number			*/
    uval16 f_nscns;		/* number of sections		*/
    uval32 f_timdat;		/* time & date stamp		*/
    uval32 f_symptr;		/* file pointer to symtab	*/
    uval32 f_nsyms;		/* number of symtab entries	*/
    uval16 f_opthdr;		/* sizeof(optional hdr)		*/
    uval16 f_flags;		/* flags			*/
} PE_stdfilehdr;

typedef struct PE_opt_hdr {
    /* aout header */
    uval16 	magic;		/* type of file			*/
    uval16	vstamp;		/* version stamp		*/
    uval32	tsize;		/* text size in bytes, padded to FW bdry*/
    uval32	dsize;		/* initialized data "  "	*/
    uval32	bsize;		/* uninitialized data "   "	*/
    uval32	entry;		/* entry pt.			*/
    uval32 	text_start;		/* base of text used for this file */
    uval32 	data_start;		/* base of data used for this file */

    /* NT extra fields; see internal.h for descriptions */
    uval32  ImageBase;
    uval32  SectionAlignment;
    uval32  FileAlignment;
    uval16  MajorOperatingSystemVersion;
    uval16  MinorOperatingSystemVersion;
    uval16  MajorImageVersion;
    uval16  MinorImageVersion;
    uval16  MajorSubsystemVersion;
    uval16  MinorSubsystemVersion;
    uval32  Reserved1;
    uval32  SizeOfImage;
    uval32  SizeOfHeaders;
    uval32  CheckSum;
    uval16  Subsystem;
    uval16  DllUval16acteristics;
    uval32  SizeOfStackReserve;
    uval32  SizeOfStackCommit;
    uval32  SizeOfHeapReserve;
    uval32  SizeOfHeapCommit;
    uval32  LoaderFlags;
    uval32  NumberOfRvaAndSizes;
    /* IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; */
    char  DataDirectory[16][2][4]; /* 16 entries, 2 elements/entry, 4 chars */

} PE_opt_hdr;


#define AOUTSZ (AOUTHDRSZ + 196)


/********************** SECTION HEADER **********************/


typedef struct PE_scnhdr {
    char		s_name[8];	/* section name			*/
    uval32		s_size;	        /* virtual size
					   (physical address, aliased s_nlib */
    uval32		s_vaddr;	/* virtual address		*/
    uval32		s_psize;        /* physical section size        */
    uval32		s_scnptr;	/* file ptr to raw data for section */
    uval32		s_relptr;	/* file ptr to relocation	*/
    uval32		s_lnnoptr;	/* file ptr to line numbers	*/
    uval16		s_nreloc;	/* number of relocation entries	*/
    uval16		s_nlnno;	/* number of line number entries*/
    uval32		s_flags;	/* flags			*/
} PE_scnhdr;


/*
 * names of "special" sections
 */
#define _TEXT	 ".text"
#define _DATA	 ".data"
#define _BSS	 ".bss"
#define _COMMENT ".comment"
#define _LIB     ".lib"
#define _ETEXT	 "etext"
#define _STAB    ".stab"
#define _STABSTR ".stabstr"


/********************** LINE NUMBERS **********************/

/* 1 line number entry for every "breakpointable" source line in a section.
 * Line numbers are grouped on a per function basis; first entry in a function
 * grouping will have l_lnno = 0 and in place of physical address will be the
 * symbol table index of the function name.
 */
struct external_lineno {
	union {
		uval32 l_symndx;	/* function name symbol index, iff l_lnno == 0*/
		uval32 l_paddr;	/* (physical) address of line number	*/
	} l_addr;
	uval16 l_lnno;	/* line number		*/
};


#define LINENO	struct external_lineno
#define LINESZ	6


/********************** SYMBOLS **********************/

#define E_SYMNMLEN	8	/* # characters in a symbol name	*/
#define E_FILNMLEN	14	/* # characters in a file name		*/
#define E_DIMNUM	4	/* # array dimensions in auxiliary entry */

struct external_syment {
  union {
    char e_name[E_SYMNMLEN];
    struct {
      uval32 e_zeroes;
      uval32 e_offset;
    } e;
  } e;
  uval32 e_value;
  uval16 e_scnum;
  uval16 e_type;
  uval8 e_sclass;
  uval8 e_numaux;
};

#define N_BTMASK	(0xf)
#define N_TMASK		(0x30)
#define N_BTSHFT	(4)
#define N_TSHIFT	(2)

union external_auxent {
	struct {
		uval32 x_tagndx;	/* str, un, or enum tag indx */
		union {
			struct {
			    uval16  x_lnno; /* declaration line number */
			    uval16  x_size; /* str/union/array size */
			} x_lnsz;
			uval32 x_fsize;	/* size of function */
		} x_misc;
		union {
			struct {		/* if ISFCN, tag, or .bb */
			    uval32 x_lnnoptr;	/* ptr to fcn line # */
			    uval32 x_endndx;	/* entry ndx past block end */
			} x_fcn;
			struct {		/* if ISARY, up to 4 dimen. */
			    uval16 x_dimen[E_DIMNUM];
			} x_ary;
		} x_fcnary;
		uval16 x_tvndx;		/* tv index */
	} x_sym;

	union {
		char x_fname[E_FILNMLEN];
		struct {
			uval32 x_zeroes;
			uval32 x_offset;
		} x_n;
	} x_file;

	struct {
		uval32 x_scnlen;	/* section length */
		uval16 x_nreloc;	/* # relocation entries */
		uval16 x_nlinno;	/* # line numbers */
		uval32 x_checksum;	/* section COMDAT checksum */
		uval16 x_associated;	/* COMDAT associated section index */
		uval8 x_comdat;	/* COMDAT selection number */
	} x_scn;

        struct {
		uval32 x_tvfill;	/* tv fill value */
		uval16 x_tvlen;	/* length of .tv */
		uval16 x_tvran[2];	/* tv range */
	} x_tv;		/* info about .tv section (in auxent of symbol .tv)) */


};

#define SYMENT	struct external_syment
#define SYMESZ	18
#define AUXENT	union external_auxent
#define AUXESZ	18




/********************** RELOCATION DIRECTIVES **********************/



struct external_reloc {
  uval32 r_vaddr;
  uval32 r_symndx;
  uval16 r_type;
};


#define RELOC struct external_reloc
#define RELSZ 10

/************************ String Table ***************************/
typedef struct PE_strtbl {
    uval32 strtbl_len;
} PE_strtbl;

#endif /* #ifndef _PE_H */
