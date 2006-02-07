/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: symbols.c,v 1.1 2000/08/02 20:07:53 jimix Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Routines dealing with the symbol table.
 ****************************************************************************/

/*#define PRINT_RAW_SYMTAB*/ /* debugging flag */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <xcoff.h>
#include <dbxstclass.h>
#include <string.h>

#include "fix-symtab.h"

static char *StgClassNames[] = {
    "EFCN", "NULL", "AUTO", "EXT", "STAT", "REG", "EXTDEF", "LABEL", "ULABEL", "MOS", "ARG",
    "STRTAG", "MOU", "UNTAG", "TPDEF", "USTATIC", "ENTAG", "MOE", "REGPARM", "FIELD", "BLOCK",
    "FCN", "EOS", "FILE", "LINE", "ALIAS", "HIDDEN", "HIDEXT", "BINCL", "EINCL", "INFO",
    "GSYM", "LSYM", "PSYM", "RSYM", "RPSYM", "STSYM", "TCSYM", "BCOMM", "ECOML", 
    "ECOMM", "DECL", "ENTRY", "FUN", "BSTAT", "ESTAT"};

static int StgClassValues[] = {
    255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 100,
    101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 
    0x89, 0x8C, 0x8D, 0x8E, 0x8F, 0x90};

#define NumStgClasses (sizeof (StgClassValues) / sizeof (int))

#define DECLStgClassValue 0x8C

static char *StgMappingClassNames [] = {
    "PR", "RO", "DB", "GL", "XO", "TI", "TB",
    "RW", "TC0", "TC", "TD", "DS",
    "SV", "SV64", "SV3264", "UA", "BS", "UC"};

static int StgMappingClassValues[] = {
    0, 1, 2, 6, 7, 12, 13,
    5, 15, 3, 16, 10, 
    8, 17, 18, 4, 9, 11};

#define NumStgMappingClasses (sizeof (StgMappingClassValues) / sizeof (int))

static char *SymbolTypeNames[] = {
    "ER", "SD", "LD", "CM", 0, 0, 0, 0};

/*
 * get_symbol_name () -- Return pointer to the character string name of a symbol
 */

char *get_symbol_name (void *ObjFileMapAddr, int index)
{
    FILHDR *f = (FILHDR *) (ObjFileMapAddr);
    SYMENT *s = (SYMENT *) (ObjFileMapAddr + f->f_symptr + index * SYMESZ);
    SCNHDR *h;

    int StringOffset = s->n_offset;
    int SectionNumber = s->n_scnum;
    int StorageClass = s->n_sclass;
    char *Name;
    int i;

    switch (StorageClass) {
    case C_BCOMM:
    case C_DECL:
    case C_ECOML:
    case C_ENTRY:
    case C_FUN:
    case C_GSYM:
    case C_LSYM:
    case C_PSYM:
    case C_RPSYM:
    case C_RSYM:
    case C_STSYM:
    case C_TCSYM:
	Name = "** No debug section **";
	for (i=0; i<f->f_nscns; i++) {
	    h = (SCNHDR *) (ObjFileMapAddr + FILHSZ + f->f_opthdr + i * SCNHSZ);
	    if ((h->s_flags & 0xFFFF) == STYP_DEBUG) {
		Name = (char *) (ObjFileMapAddr + h->s_scnptr + StringOffset);
		break;
	    }
	}
	break;
    case C_BINCL:
    case C_EXT:
    case C_FCN:
    case C_FILE:
    case C_HIDEXT:
    case C_INFO:
    case C_STAT:
	Name = (char *) (ObjFileMapAddr + f->f_symptr + f->f_nsyms * SYMESZ + StringOffset);
	break;
    default:
	Name = "";
    }

    return Name;
}


/*
 * get_file_aux_name () -- Get file name (or other string) from a FILE aux entry
 */

char *get_file_aux_name (void *ObjFileMapAddr, int index)
{
    FILHDR *f = (FILHDR *) (ObjFileMapAddr);
    AUXENT *x = (AUXENT *) (ObjFileMapAddr + f->f_symptr + index * SYMESZ);
    static char Name[FILNMLEN + 2];

    if (x->x_file._x.x_zeroes == 0) {
	/*
	 * The entry contains an offset into the string table
	 */
	return string_table_entry (ObjFileMapAddr, x->x_file._x.x_offset);
    }
    else {
	/*
	 * The string is short enough to be in the auxent itself
	 */
	strncpy (Name, x->x_file.x_fname, FILNMLEN);
	Name[FILNMLEN] = '\0';		/* ensure that it ends in 0 */
	return Name;
    }
}

/*
 * print_raw_symtab () -- Print a symtab entry in hex for debugging
 */
void print_raw_symtab (int num, void *ent)
{
#ifdef PRINT_RAW_SYMTAB
    struct RawSymbol {
	int a;
	int b;
	int c;
	int d;
	short e;
    };
    struct RawSymbol *r;

    r = (struct RawSymbol *) ent;
    printf ("  %6d  %.8X %.8X %.8X %.8X %.4X\n",
	    num, r->a, r->b, r->c, r->d, r->e & 0xFFFF);

#endif /* PRINT_RAW_SYMTAB */
}

/*
 * symtab_print_symbol_string () -- Print a symbol or file name, 
 *                                  breaking long names across lines if necessary.
 */
static void symtab_print_symbol_string (char *Name)
{
    int Length = strlen (Name);
    int MaxFirstLineLength = 77;
    int MaxSubsequentLineLength = 75;
    int MaxLineLength = MaxFirstLineLength;

    char *Padding = "";

    while (Length >= 0) {
	if (Length <= MaxLineLength) {
	    printf ("%s%s\n", Padding, Name);
	    return;
	}
	else {
	    char Buffer[80];
	    strncpy (Buffer, Name, MaxLineLength);
	    Buffer[MaxLineLength] = '\\';
	    Buffer[MaxLineLength + 1] = '\0';
	    printf ("%s%s\n", Padding, Buffer);
	    Name += MaxLineLength;
	    Padding = "                                                      ";
	    Length -= MaxLineLength;
	    MaxLineLength = MaxSubsequentLineLength;
	}
    }
}    

/**************************************************************************
 *
 * print_aux_symtab_entry () -- Print an auxiliary entry
 *
 **************************************************************************/

void print_aux_symtab_entry (void *ObjFileMapAddr, int index)
{
    FILHDR *f = (FILHDR *) (ObjFileMapAddr);
    AUXENT *x = (AUXENT *) (ObjFileMapAddr + f->f_symptr + index * SYMESZ);
    unsigned long long v;
    int t, i;

    switch (x->x_sym.x_auxtype) {
    case _AUX_CSECT:
	printf ("            CSECT                                     ");
	v = (((unsigned long long) x->x_csect.x_scnlen_hi) << 32) | 
	     ((unsigned long long) x->x_csect.x_scnlen_lo);
	printf ("scnlen %s  ", format_long_long (v));
	printf ("parmhash %s  ", format_int (x->x_csect.x_parmhash));
	printf ("snhash %d  ", x->x_csect.x_snhash);
	t = x->x_csect.x_smtyp & 0x7;
	if (SymbolTypeNames[t] != NULL)
	    printf ("type %s  ", SymbolTypeNames[t]);
	else
	    printf ("smtyp 0x%.2X  ", x->x_csect.x_smtyp);
	if (XTY_SD == t || XTY_CM == t) {
	    printf ("align %d  ", 1 << ((x->x_csect.x_smtyp >> 3) & 0x1F));
	    for (i=0; i<NumStgMappingClasses; i++) 
		if (StgMappingClassValues[i] == x->x_csect.x_smclas)
		    break;
	    if (i < NumStgMappingClasses)
		printf ("smclas %s", StgMappingClassNames[i]);
	    else
		printf ("smclas 0x%.2X", x->x_csect.x_smclas);
	}
	printf ("\n");
	break;
    case _AUX_EXCEPT:
	printf ("            EXCEPT                                    ");
	printf ("exptr %s  ", format_long_long (x->x_except.x_exptr));
	printf ("fsize %s  ", format_int (x->x_except.x_fsize));
	printf ("endndx %d\n", x->x_except.x_endndx);
	printf ("\n");
	break;
    case _AUX_FCN:
	printf ("            FCN                                       ");
	printf ("lnnoptr %s  ", format_long_long (x->x_fcn.x_lnnoptr));
	printf ("fsize %s  ", format_int (x->x_fcn.x_fsize));
	printf ("endndx %d\n", x->x_fcn.x_endndx);
	break;
    case _AUX_FILE:
	printf ("            FILE (%s)                                 ",
		x->x_file._x.x_ftype == XFT_FN ? "FN" :
		x->x_file._x.x_ftype == XFT_CT ? "CT" :
		x->x_file._x.x_ftype == XFT_CV ? "CV" :
		x->x_file._x.x_ftype == XFT_CD ? "CD" :
		"??");
	symtab_print_symbol_string (get_file_aux_name (ObjFileMapAddr, index));
	break;
    case _AUX_SYM:
	printf ("            SYM                                       "
		"line %d\n",
		x->x_sym.x_misc.x_lnsz.x_lnno);
	break;
    default:
	printf ("***** Unexpected auxiliary symbol table entry type at index %d (value = 0x%.2X)\n",
		index, x->x_sym.x_auxtype);
    }
}




/**************************************************************************
 *
 * process_symbol_table ()
 *
 **************************************************************************/

void process_symbol_table (void *ObjFileMapAddr, char *TargetNames[], int verbose)
{
    FILHDR *f = (FILHDR *) (ObjFileMapAddr);

    int i, j, k;
    char *p;
    enum {STATE_LOOKING, STATE_FOUND_TARGET} state = STATE_LOOKING;

/*                          stg           sec  num
   index       value        class   func  num  aux  name
  000000  0000000000000000  USTATIC func   0    0   a_symbol_with_a_really_long_name
*/                                           

    if (verbose) {
	printf ("  Symbol table:\n\n"
		"  symtab                    stg           sec  num\n"
		"   index       value        class   func  num  aux  name\n");
    }

    for (i=0; i<f->f_nsyms; i++) {
	SYMENT *s = (SYMENT *) (ObjFileMapAddr + f->f_symptr + i * SYMESZ);
	char *Class = NULL;
	static char HexClass[8];
	int j;

	/* Process entry according to its type, and what state we're in at the moment */
	switch (s->n_sclass) {
	case C_FILE:
	    switch (state) {
	    case STATE_LOOKING:
		/*
		 * FILE entry - see if it's one of our targets
		 */
		p = get_symbol_name (ObjFileMapAddr, i);
		for (k=0; TargetNames[k]; k++) {
		    if (p && *p && strstr (p, TargetNames[k])) {
			state = STATE_FOUND_TARGET;
			break;
		    }
		}
		break;
	    case STATE_FOUND_TARGET:
		state = STATE_LOOKING;
		break;
	    }
	    break;

	case C_HIDEXT:
	case C_EXT:
	case C_FCN:
	    switch (state) {
	    case STATE_LOOKING:
		break;
	    case STATE_FOUND_TARGET:
		/*
		 * Force the "value" (address) in the symbol table down into the real address range
		 */
		s->n_value &= 0x000000000FFFFFFFULL;

		/*
		 * Fix line number table entries, if symtab entry is for a functoin
		 */
		if (s->n_sclass != C_EXT && s->n_sclass != C_HIDEXT) break;
		if (s->n_type != 0x0020) break;
		{
		    SCNHDR *sc = (SCNHDR *) (ObjFileMapAddr + FILHSZ + f->f_opthdr + (s->n_scnum - 1) * SCNHSZ);
		    LINENO *l;
		    int ii;

		    /*
		     * Search for start of line number table for this function
		     */
		    for (ii=0; ii<sc->s_nlnno; ii++) {
			l = (LINENO *) (ObjFileMapAddr + sc->s_lnnoptr + ii * LINESZ);
			if (l->l_lnno == 0 && 		/* start of line table for a function */
			    l->l_addr.l_symndx == i) {  /* function index matches the one we want */
			    for (ii++; ; ii++) {
				l = (LINENO *) (ObjFileMapAddr + sc->s_lnnoptr + ii * LINESZ);
				if (l->l_lnno == 0)     /* start of line table for next function */
				    goto done_lines;
				l->l_addr.l_paddr &= 0x000000000FFFFFFFULL;  /* force address into real range */
			    }
			}
		    }
		done_lines:
		}
		
		break;
	    }
	    break;
	    
	case C_BCOMM:
	case C_BINCL:
	case C_DECL:
	case C_ECOML:
	case C_ENTRY:
	case C_FUN:
	case C_GSYM:
	case C_INFO:
	case C_LSYM:
	case C_PSYM:
	case C_RPSYM:
	case C_RSYM:
	case C_STAT:
	case C_STSYM:
	case C_TCSYM:
	default:
	    break;
	}

	/* Print out entries if in "found" range */
	if (verbose) {
	    switch (state) {
	    case STATE_LOOKING:
		break;
	    case STATE_FOUND_TARGET:
		print_raw_symtab (i, s);
		for (j=0; j<NumStgClasses; j++) {
		    if (StgClassValues[j] == s->n_sclass) {
			Class = StgClassNames[j];
			break;
		    }
		}
		if (Class == NULL) {
		    sprintf (HexClass, "0x%.4X", s->n_sclass);
		    Class = HexClass;
		}
		
		printf ("  %6d  %.16llX  %-7s %s %3d  %3d   ",
			i,
			s->n_value,
			Class,
			(C_EXT == s->n_sclass || C_HIDEXT == s->n_sclass) ?
			(0x0020 == s->n_type ? "func" : "    ") : "    ",
			s->n_scnum,
			s->n_numaux);
		
		symtab_print_symbol_string (get_symbol_name (ObjFileMapAddr, i));
		
		for (j=1; j<=s->n_numaux; j++) 
		    print_aux_symtab_entry (ObjFileMapAddr, i + j);
		break;
	    }
	}
	
	i += s->n_numaux;		/* skip over aux entries, if any */
    }
}






