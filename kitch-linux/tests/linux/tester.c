/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: tester.c,v 1.3 2000/12/15 20:32:15 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: invokes available tests
 * **************************************************************************/
#include <stdio.h>
#include <string.h>

typedef int (*mainptr_t)(int, const char *[], const char *[]);
typedef int (main_t)(int, const char *[], const char *[]);

extern main_t FPTR main;

mainptr_t funcs[] = { FPTR main };

const char *syms[] = { SYMS NULL };


static void
usage(void)
{
    int i;
    fputs("Supported commands:\n",stderr);
    for (i = 0; syms[i] != NULL; i++) {
	fprintf(stderr, "\t%s\n", syms[i]);
    }
}

int
main(int argc, const char *argv[], const char *envp[])
{
    int i = 0;
    int idx = 0;
    const char *prog;

    /* we could do this in a loop but we want to limit it to 2 passes so.. */

    prog = strrchr(argv[idx], '/');
    if (prog == NULL) {
	prog = argv[idx];
    } else {
	prog++;
    }

    /* check to see if we called tester or not */
    if (strcmp(prog, "tester") == 0) {
	if (argc < 2) {
	    fputs("Error: no command specified\n", stderr);
	    usage();
	    return 1;
	}
	idx = 1;
	--argc;
	prog = argv[idx];
    }

    while (syms[i] != NULL) {
	if (strcmp(syms[i], prog) == 0) {
	    return funcs[i](argc, &argv[idx], envp);
	}
	++i;
    }
    fprintf(stderr, "The command: \"%s\" is not supported.\n", prog);

    usage();

    return 1;
}



