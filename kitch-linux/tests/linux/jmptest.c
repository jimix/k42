/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: jmptest.c,v 1.8 2002/11/05 22:25:02 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for setjmp() longjmp()
 * **************************************************************************/
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

static int
jmp_rec(jmp_buf env, int count)
{
    int ret;
    int c;
    int *inta;

    if (count > 0) {
	inta = (int *)alloca(count * sizeof (int));
	putchar('.');
	/* use the alloca'd memory */
	for (c = 0; c < count ; c++) {
	    inta[c] = c;
	}
	ret = jmp_rec(env, count - 1);
    } else {
	putchar('\n');
	longjmp(env, 1);
    }
    return (ret);
}

int
main(int argc, char *argv[])
{
    int ret;
    volatile int count; /* don't clobber after setjmp */
    jmp_buf env;

    /* default is 100 */
    if (argc > 1) {
	count = atof(argv[1]);
    } else {
	count = 100;
    }

    printf("will call longjmp() %d call deep\n", count);

    if (setjmp(env) != 0) {
	printf("Yay, returned from longjump()\n");
	exit(1);
    }
    ret = jmp_rec(env, count);

    printf("Oh Oh, returned without longjmp() ret = %d\n", ret);

    return (0);
}
