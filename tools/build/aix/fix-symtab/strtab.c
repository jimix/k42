/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: strtab.c,v 1.1 2000/08/02 20:07:53 jimix Exp $
 ****************************************************************************/
/******************************************************************************
 * Print string table of 64-bit XCOFF module
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <xcoff.h>

#include "fix-symtab.h"

char * string_table_entry (void *ObjFileMapAddr, long offset)
{
    FILHDR *p = (FILHDR *) ObjFileMapAddr;
    int *StrTabLengthPtr = (int *) (ObjFileMapAddr + p->f_symptr + (p->f_nsyms * SYMESZ));
    int len = *StrTabLengthPtr;
    char *s = ((char *) (StrTabLengthPtr)) + offset;
    
    return s;
}
    
