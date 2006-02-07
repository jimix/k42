#ifndef __FIX_SYMTAB_H_
#define __FIX_SYMTAB_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fix-symtab.h,v 1.2 2001/10/05 21:51:34 peterson Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Function prototypes
 ***************************************************************************/

void process_symbol_table   (void *ObjFileMapAddr, char *TargetNames[], int verbose);

void print_raw_data (void *DataPointer,		/* pointer to the actual data to print */
		     long Length,		/* amount of data to print */
		     long long VirtualAddress, 	/* display this virtual address, if not -1 */
		     long long FilePointer, 	/* display this file pointer, if not -1 */
		     long long DataOffset); 	/* display this data offset, if not -1 */

char *string_table_entry (void *ObjFileMapAddr, long offset);

char *format_addr      (unsigned long long addr);
char *format_long_long (unsigned long long val);
char *format_long      (unsigned long val);
char *format_int       (unsigned int val);

char *get_symbol_name   (void *ObjFileMapAddr, int index);
char *get_file_aux_name (void *ObjFileMapAddr, int index);

#endif /* #ifndef __FIX_SYMTAB_H_ */
