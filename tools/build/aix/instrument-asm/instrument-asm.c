/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: instrument-asm.c,v 1.3 2000/11/30 16:05:59 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 * Instrument an assembly language program by adding ".line <nnn>"
 * statements where useful
 *
 * Command-line args:
 *     instrument-asm  <input-file>  [<output-file>]
 *
 * The input-file name is required.  The output-file name is optional;
 * if not supplied, it will be constructed from the input-file name
 * thusly: foo.s --> foo-asm.s
 *
 * The tool operates like this:
 *
 *   o It inserts a line at the start of the output file containing
 *     ".file <input-file>" to associate subsequent .line statements
 *     with the original input file.
 *
 *   o It copies lines from the input file to the output file, searching
 *     for a line beginning with "#begin-lines".
 *
 *   o Once "#begin-lines" has been found, it continues to copy input
 *     lines to the output file, inserting a ".line nnn" statement
 *     after * each line that might contain a line of assembly code.
 *
 *   o When EOF is reached on input, it inserts ".bf <last-line-number>"
 *     in the output file.
 *
 * Basically, the tool has no finesse.  It doesn't recognize
 * boundaries between functions.
 *
 * Its whole purpose is to source file line numbers associated with
 * lines of assembly code, so that a debugger like gdb can single step
 * through assembly code while displaying the source file.
 ****************************************************************************/


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char *argv[])
{
    char *infilename, *outfilename;
    FILE *infile, *outfile;
    char *p;
    int  file_line_number = 0;
    int  function_line_number = 0;
    char buffer[2048];
    int  found_start = 0;
    int  continued = 0;

    /*
     * Process command line arguments
     */
    switch (argc) {
	/*
	 * Only one file name supplied
	 */
    case 2:
	infilename = argv[1];
	if (0 == strncmp ("-h", infilename, 2))
	    goto syntax;
	if (0 == strncmp ("--h", infilename, 3))
	    goto syntax;
	outfilename = (char *) malloc (strlen (infilename) + 8);
	strcpy (outfilename, infilename);
	p = outfilename + strlen (outfilename) - 2;
	strcpy (p, "-asm.s");
	break;

	/*
	 * Two file names supplied
	 */
    case 3:
	infilename = argv[1];
	outfilename = argv[2];
	break;
	
    default:
	goto syntax;
    }

    /*
     * Open input and output files
     */
    infile = fopen (infilename, "r");
    if (!infile)
	goto no_in;
    outfile = fopen (outfilename, "w");
    if (!outfile)
	goto no_out;

    /*
     * Put ".file <input-file>" at start of output file
     */
    fprintf (outfile, "\t.file\t\"%s\"\n", infilename);


    /*
     * Read and process input file lines
     */
    for (;;) {
	if (NULL == fgets (buffer, sizeof (buffer), infile)) {
	    /*
	     * end of file reached 
	     */
	    fclose (infile);
	    fclose (outfile);
	    return 0;
	}
	
	file_line_number += 1;
	function_line_number += 1;
	p = buffer + strspn (buffer, " \t"); /* skip leading white space */

	/*
	 * Look for ending flag; when found, put out ".ef <n>" with the
	 * file line number *before* the END macro.
	 */
	if (found_start &&
	    (0 == strncmp (p, "CODE_END", 8) ||
	     0 == strncmp (p, "C_TEXT_END", 10))) {
	    fprintf (outfile, "\t.ef\t%d\n", file_line_number);
	}
	/*
	 * if we've found the starting flag, and if this line is one that should
	 * probably have a line number associated with it, put out ".line <n>"
	 */
	else if (found_start &&
	    !continued &&		/* continuation from previous line */
	    *p != '#' &&		/* assembly comment */
	    *p != '.' &&		/* pseudo-op */
	    *p != '\n' &&		/* empty line */
	    *p != '*' &&		/* part of C comment */
	    *p != '/') {		/* part of C comment */
	    fprintf (outfile, "\t.line\t%d\n", function_line_number);
	}

	continued = (p[strlen(p)-2] == '\\');

	/*
	 * Copy the input line to the output 
	 */
	fprintf (outfile, "%s", buffer);

	/*
	 * Look for starting flag; when found, put out ".bf <n+1>" with the
	 * file line number.
	 */
	if (!found_start &&
	    (0 == strncmp (p, "CODE_ENTRY", 10) ||
	     0 == strncmp (p, "C_TEXT_ENTRY", 12))) {
	    found_start = 1;
	    function_line_number = 0;
	    fprintf (outfile, "\t.bf\t%d\n", file_line_number + 1);
	}

	/*
	 * Look for ending flag; when found, turn off found_start
	 */
	if (0 == strncmp (p, "CODE_END", 8) ||
	    0 == strncmp (p, "C_TEXT_END", 10)) {
	    found_start = 0;
	}
    }
    return 0;

 syntax:
    fprintf (stderr, "syntax:  %s <infile> [<outfile>]\n", argv[0]);
    return -1;

 no_in:
    fprintf (stderr, "can't open input file: %s\n", infilename);
    return -1;

 no_out:
    fprintf (stderr, "can't open output file: %s\n", outfilename);
    return -1;


}
