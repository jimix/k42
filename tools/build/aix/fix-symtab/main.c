/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.c,v 1.1 2000/08/02 20:07:53 jimix Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: fix-symtab main procedure
 *
 * This HACK DOG program is intended to make debugging K42's kernel a
 * bit simpler.
 *
 * It processes the symbol table entries and line number entries in
 * the copy of the K42 kernel that is read by the debugger, gdb.
 * (Typically, this file is boot_image.dbg.)
 *
 *  For those few routines that actually execute in real addressing
 *  mode, such as low-level interrupt handlers written in assembly
 *  langauge, it forces the addresses in the appropriate symbol
 *  table entries and line number entries to be the real address
 *  rather than the virtual address.
 *
 * This works because of the simple V<->R mapping used by K42 for
 * these programs.
 *
 * Conversion from virtual to real is done by just ANDing each address
 * with 0x000000000FFFFFFF, so that 0xC01234560 becomes just
 * 0x1234560.
 *
 * As a result, when execution enters one of these real-mode routines, the
 * debugger can display the appropriate source line as it steps through
 * the program.  Without such processing as done here, the debugger cannot
 * associate the real-mode execution with any known source file, because the
 * symbol table and line number entries contain virtual addresses that don't
 * match the real addresses that the debugger is seeing.
 *
 * The entries processed are specified on the command line.  The first
 * argument is the name of the kernel file (typically boot_image.dbg).
 * Subsequent arguments give a source file name (lolita.S; locore.S;
 * ...) whose functions are to be treated as described above.  The
 * program looks through the symbol table for a FILE entry mentioning
 * any of the specified file names, and then processes all the entries
 * for that file.
 *
 * Several assumptions are made: Conversion from virtual to real can
 * be done in the simple manner described above; only EXT, HIDEXT,
 * FCN, and FILE entries need be processed; and so on.
 *
 * Then again, it's not all hackery.  The program is idempotent.  Any
 * number of targets can be specified on the command line.  The kernel
 * file is mapped and modified in place.  The code here was modified
 * (hacked) from the xcdump program, which displays (in a general
 * manner) 64-bit XCOFF files.
 *
 * The idea is to invoke the program from a makefile, and forget about
 * it.
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>

#include <xcoff.h>

#include "fix-symtab.h"

int main (int argc, char *argv[]) {
    
    char *ObjFileName = NULL;
    int ObjFileFD;
    void *ObjFileMapAddr;
    FILHDR *FileHdrPtr;
    char *OptionFlag;
    char **TargetNames = 0;

    int rc;
    int i;
    int verbose = 0;

    /*
     * Parse command line
     */
    for (i=1; i<argc; i++) {
	if (argv[i][0] == '-') {
	    OptionFlag = argv[i] + 1;
	    while (*OptionFlag) {
		switch (*OptionFlag) {
		case 'q':
		    verbose = 0;
		    break;
		case 'v':
		    verbose = 1;
		    break;
		default:
		    goto show_syntax;	/* nothing here yet */
		}
		OptionFlag++;
	    }
	}
	else {
	    if (ObjFileName) {
		TargetNames = &argv[i];
		break;
	    }
	    ObjFileName = argv[i];
	}
    }
    if (!ObjFileName) goto show_syntax;
    if (!TargetNames) goto show_syntax;

    /*
     * Open input file for reading
     */
    ObjFileFD = open (ObjFileName, O_RDWR);
    if (ObjFileFD < 0) goto open_error;

    /*
     * Map the input file into virtual memory
     */
    ObjFileMapAddr = shmat (ObjFileFD, NULL, SHM_MAP);

    /*
     * Verify that we have a 64-bit executable (check the magic number)
     */
    FileHdrPtr = (FILHDR *) ObjFileMapAddr;
    if (U803XTOCMAGIC != FileHdrPtr->f_magic) goto not_64;

    /*
     * Process the object file
     */
    process_symbol_table (ObjFileMapAddr, TargetNames, verbose);

    close (ObjFileFD);
    
    return 0;

 show_syntax:
    fprintf (stderr, 
	     "\n"
	     "syntax:    %s  [-v|-q] object-file  target [target ...]\n\n"
	     "   where   -v           verbose output to stdout\n"
	     "           -q           no verbose output (default)\n"
	     "           object-file  is the name of the K42 kernel with debugging info\n"
	     "                           typically this is \"boot_image.dbg\"\n"
	     "           target       file name(s) such as \"lolita.S\" or \"locore.S\"\n"
	     "                           whose symbol and line entries should have their real\n"
	     "                           addresses forced from virtual to real\n\n",
	     argv[0]);
    return 1;

 open_error:
    perror ("open of input file failed");
    return 1;

 not_64:
    fprintf (stderr, "File %s is not a 64-bit XCOFF object module\n", ObjFileName);
    return 1;
}

