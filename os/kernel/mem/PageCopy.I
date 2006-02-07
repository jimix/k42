/************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageCopy.I,v 1.2 2002/04/17 19:42:45 dje Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Generic implementations can be included by
 * machine specific implementation.
 * **************************************************************************/
#include <kernIncs.H>
#include <mem/PageCopy.H>

void
PageCopy::Memset(void * t, int cin, size_t count)
{
    tassert(((uval(t)|uval(count))&63) == 0,
	    err_printf("bad alignment PageCopy::Memset\n"));
    uval64 *p = (uval64 *)t;
    unsigned char c = cin;
    uval w = c|(c<<8);
    w = w|(w<<16);
    w = w|(w<<32);

    for(; count; count-=64) {
	p[0] = w;
	p[1] = w;
	p[2] = w;
	p[3] = w;
	p[4] = w;
	p[5] = w;
	p[6] = w;
	p[7] = w;
	p+=8;
    }
}


void
PageCopy::Memset0(void * t, size_t count)
{
    tassert(((uval(t)|uval(count))&63) == 0,
	    err_printf("bad alignment PageCopy::Memset0\n"));
    uval64 *p = (uval64 *)t;
    const uval w = 0;

    for(; count; count-=64) {
	p[0] = w;
	p[1] = w;
	p[2] = w;
	p[3] = w;
	p[4] = w;
	p[5] = w;
	p[6] = w;
	p[7] = w;
	p+=8;
    }
}

void
PageCopy::Memcpy(void * t, const void * s, size_t count)
{
    tassert(((uval(t)|uval(s)|uval(count))&63) == 0,
	    err_printf("bad alignment PageCopy::Memcpy\n"));
    uval64 * tp = (uval64 *) t;
    uval64 * sp = (uval64 *) s;
    uval64 w0,w1,w2,w3;
    w0 = sp[0];
    w1 = sp[1];
    w2 = sp[2];
    w3 = sp[3];
    count -=64;
    for(; count; count-=64) {
	tp[0] = w0;
	w0 = sp[4];
	tp[1] = w1;
	w1 = sp[5];
	tp[2] = w2;
	w2 = sp[6];
	tp[3] = w3;
	w3 = sp[7];
	sp+=8;
	tp[4] = w0;
	w0 = sp[0];
	tp[5] = w1;
	w1 = sp[1];
	tp[6] = w2;
	w2 = sp[2];
	tp[7] = w3;
	w3 = sp[3];
	tp+=8;
    }
    tp[0] = w0;
    w0 = sp[4];
    tp[1] = w1;
    w1 = sp[5];
    tp[2] = w2;
    w2 = sp[6];
    tp[3] = w3;
    w3 = sp[7];
    tp[4] = w0;
    tp[5] = w1;
    tp[6] = w2;
    tp[7] = w3;
    return;
}

