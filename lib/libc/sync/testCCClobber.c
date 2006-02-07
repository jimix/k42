/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testCCClobber.c,v 1.5 2000/05/11 11:29:10 rosnbrg Exp $
 *****************************************************************************/

/*
 * test of gcc clobber of cr
 * compile fails
 * compile egcc -S -O2 asm.c
 * if it compiles, look at kaka line in asm.s and make sure condition register
 * is not reused afterwards without saving and reloading it.
 * currently, this causes a compiler error with no known workaround. (1.0.3a)
 */
extern int x,y;
void foo(int a)
{
    int dummy;
    if(a+1)
	x=0;
    asm volatile("kaka" : : : "cc");
    if(a+1) y=0;
}
