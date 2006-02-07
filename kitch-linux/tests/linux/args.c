/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: args.c,v 1.8 2002/11/05 22:25:01 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: print out the arguements
 * **************************************************************************/
#include <stdio.h>
#include <unistd.h>

int
main(int argc, char *argv[], char *envp[])
{
    int i;

    printf("argc = %d, argv = %p, envp = %p\n",
	    argc, argv, envp);


    for (i = 0; i < argc; i++) {
	printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }

    if (argv[argc] != NULL) {
	printf("oops argv[%d] != NULL\n", argc);
    }


    for (i = 0; envp[i] != NULL; i++) {
	fprintf(stdout, "envp[%d] = \"%s\"\n", i, envp[i]);
    }

    fflush(stdout);
    fsync(1);
    return (0);
}
