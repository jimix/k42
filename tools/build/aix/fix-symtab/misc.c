/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: misc.c,v 1.1 2000/08/02 20:07:53 jimix Exp $
 ****************************************************************************/
/******************************************************************************
 * Miscellaneous functions
 ****************************************************************************/

#include <ctype.h>
#include <memory.h>

#include "fix-symtab.h"


/*
 * format_addr -- Format a 64-bit address string as hex or 0
 */
char * format_addr (unsigned long long addr)
{
    static char str[32];

    if (addr) {
	sprintf (str, "0x%.16llX", addr);
	return str;
    }
    else
	return "0";
}

/*
 * format_long_long
 */
char * format_long_long (unsigned long long val)
{
    static char str[32];

    if (val < 10)
	sprintf (str, "%lld", val);
    else
	sprintf (str, "0x%llX", val);
    return str;
}

/*
 * format_long -- Format a long as hex or 0
 */
char * format_long (unsigned long val)
{
    static char str[32];

    if (val < 10)
	sprintf (str, "%ld", val);
    else
	sprintf (str, "0x%lX", val);
    return str;

}

/*
 * format_int -- Format a 4-byte integer as hex or 0
 */
char * format_int (unsigned int val)
{
    static char str[32];

    if (val < 10) 
	sprintf (str, "%d", val);
    else
	sprintf (str, "0x%X", val);
    return str;
}

/*
 * store_hex () -- Store the two hex bytes representing a character
 */

static char ToHex[] = {"0123456789ABCDEF"};

__inline__ void store_hex (char *where, int value)
{
    where[0] = ToHex[(value >> 4) & 0xF];
    where[1] = ToHex[value & 0xF];
}

/*
 * print_rld () -- Print a line showing all the info for an RLD, either
 *                 an RLD for a regular section (.text, .data) or
 *                 an RLD from the .loader section
 */

void print_rld_heading (int for_loader) 
{
    printf ("                    %s                        symtab  symbol\n"
	    "    virtual addr    %sflags        len  type   index  name\n",
	    for_loader ? " in   " : "",
	    for_loader ? "sect  " : "");
}

static char *RLDFlagNames [] = {
    "",
    "fixup",
    "signed",
    "signed,fixup"};

static char *RLDTypeNames [] = {
    "POS", "NEG", "REL", "TOC", "TRL", "TRLA", "GL", 
    "TCL", "RL", "RLA", "REF", "BA", "RBA", "BR", "RBR"};

static int RLDTypeValues [] = {
    0, 1, 2, 3, 4, 19, 5, 6, 12, 13, 15, 8, 24, 10, 26};

#define NumRLDTypes (sizeof (RLDTypeValues) / sizeof (int))

void print_rld (int for_loader,		/* TRUE if to print section number */
		unsigned long long vaddr, /* virtual address */
		int section,		/* section number */
		int symndx,		/* index in symbol table (file or .loader symtab) */
		char *symname,		/* name of symbol, from appropriate symbol table */
		int rsize,		/* reloc size and info */
		int rtype)		/* reloc type */
{
    int i;

    printf ("  %.16llX  ", vaddr);
    if (for_loader)
	printf ("%3d   ", section);
    printf ("%-12s %3d  ", RLDFlagNames[(rsize >> 6) & 3], (rsize & 0x3F) + 1);
    for (i=0; i<NumRLDTypes; i++) 
	if (rtype == RLDTypeValues[i])
	    break;
    if (i < NumRLDTypes)
	printf ("%-6s", RLDTypeNames[i]);
    else
	printf ("0x%.2X  ", rtype);
    printf ("%6d  %s\n", symndx, symname);
}

/*
 * print_raw_data ()
 *
 * Print the data in hex, with one or more offsets/addresses for each line
 *
 */
void print_raw_data (void *DataPointer,		/* pointer to the actual data to print */
		     long Length,		/* amount of data to print */
		     long long VirtualAddress, 	/* display this virtual address, if not -1 */
		     long long FilePointer, 	/* display this file pointer, if not -1 */
		     long long DataOffset) 	/* display this data offset, if not -1 */
{
    int VAWanted = 1;
    int FPWanted = 1;
    int DOWanted = 1;
    char HexArea[36];
    char ASCIIArea[17];
    int Cursor;
    char *DP = (char *) DataPointer;
    char C;
    
    if (VirtualAddress == -1)
	VAWanted = 0;
    if (FilePointer == -1)
	FPWanted = 0;
    if (DataOffset == -1)
	DOWanted = 0;
    
/*
    virtual addr    file ptr    offset
  0000000000000000  00000000  00000000  00000000 00000000 00000000 00000000  |................|
*/

    if (VAWanted | FPWanted)		/* don't bother with header if it would just be "offset" */
	printf ("  %s%s%s\n",
		VAWanted ? "  virtual addr    " : "",
		FPWanted ? "file ptr  "         : "",
		DOWanted ? "  offset"           : "");
    
    while (Length > 0) {
	memset (HexArea, ' ', 35);
	HexArea[35] = '\0';
	memset (ASCIIArea, ' ', 16);
	ASCIIArea[16] = '\0';
	
	if (VAWanted) 
	    Cursor = VirtualAddress & 0xF;
	else if (FPWanted) 
	    Cursor = FilePointer & 0xF;
	else if (DOWanted) 
	    Cursor = DataOffset & 0xF;
	else
	    Cursor = 0;

	if (VAWanted) {
	    printf ("  %.16llX", VirtualAddress);
	    VirtualAddress += (16 - Cursor);
	}
	if (FPWanted) {
	    printf ("  %8llX", FilePointer);
	    FilePointer += (16 - Cursor);
	}
	if (DOWanted) {
	    printf ("  %8llX", DataOffset);
	    DataOffset += (16 - Cursor);
	}

	while (Cursor < 16 && Length > 0) {
	    C = *DP++;
	    store_hex (HexArea + (Cursor << 1) + (Cursor >> 2), C);
	    ASCIIArea[Cursor] = isprint(C) ? C : ' ';
	    Cursor++;
	    Length--;
	}
	printf ("  %s  |%s|\n", HexArea, ASCIIArea);
    }
}
