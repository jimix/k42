/************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageCopy.C,v 1.5 2005/08/25 18:39:57 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: PowerPC specific implemenation, optimized to
 * use instructions to zero cache lines.
 * **************************************************************************/
#include <kernIncs.H>
#include <mem/PageCopy.H>
#include <sys/KernelInfo.H>
#include <mem/PerfStats.H>
#define _FAST_ZERO

#ifndef _FAST_ZERO
#include "PageCopy.I"
#else
template <uval CLL>
static void
MemsetImp(void * t, int cin, size_t count)
{
    tassertMsg(((uval(t)|uval(count))&(CLL-1)) == 0,
	       "bad alignment PageCopy::Memset\n");
    uval64 *p = (uval64 *)t;
    uval i;
    unsigned char c = cin;
    uval w = c|(c<<8);
    w = w|(w<<16);
    w = w|(w<<32);

    for (; count; count-=CLL) {
	__asm__ ("dcbz 0,%0" : : "r" (p));
	for (i=0;i<CLL/sizeof(uval64);i++) {
	    p[i]=w;
	}
	p+=CLL/sizeof(uval64);
    }
}

void
PageCopy::Memset(void * t, int cin, size_t count)
{
    if (KernelInfo::PCacheLineSize() == 128) {
	MemsetImp<128>(t, cin, count);
    } else {
	passertMsg(0, "need case to handle cache line size of %d\n",
		   KernelInfo::PCacheLineSize());
    }
}


template <uval CLL>
static void
Memset0Imp(void * t, size_t count)
{
    tassertMsg(((uval(t)|uval(count))&(CLL-1)) == 0,
	       "bad alignment PageCopy::Memset\n");
    register uval p=(uval)t;

    for (p=(uval)t;p<(((uval)t)+count);p+=CLL) {
	__asm__ ("dcbz 0,%0" : : "r" (p));
    }
}

void
PageCopy::Memset0(void * t, size_t count)
{
    ScopeTime timer(ZFillTimer);
    if (KernelInfo::PCacheLineSize() == 128) {
	Memset0Imp<128>(t, count);
    } else {
	passertMsg(0, "need case to handle cache line size of %d\n",
		   KernelInfo::PCacheLineSize());
    }
}

void
Memcpy128(void * t, const void * s, size_t count)
{
    tassertMsg(((uval(t)|uval(s)|uval(count))&127) == 0,
	       "bad alignment in Memcpy128\n");
    uval64 * tp = (uval64 *) t;
    uval64 * sp = (uval64 *) s;
    uval64 w0,w1,w2,w3;
#if 1
    uval i=0;
#endif
    w0 = sp[0];
    w1 = sp[1];
    w2 = sp[2];
    w3 = sp[3];
    count -=64;
    for (; count; count-=64) {
#if 1
	/*
	 * for 128 byte cache line, alternate between zeroing
	 * to avoid cache line load, this doesn't clearly have a
	 * benefit UP (where data likely in cache) but might be advantageous MP
	 */
	if (i==0) {
	    __asm__ ("dcbz 0,%0" : : "r" (tp));
	    i=1;
	} else {
	    i=0;
	}
#endif

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

// Note count== number of 16-byte quantities to copy
extern "C" void CopyPage(void* to, const void* from, uval count);

void
PageCopy::Memcpy(void * t, const void * s, size_t count)
{
    ScopeTime timer(MemCpyTimer);
#if 0
    if (! ( (((uval)t)|((uval)s)|count) & (PAGE_SIZE - 1) )) {
	CopyPage(t, s, count>>4);
	return;
    }
#endif
    if (KernelInfo::PCacheLineSize() == 128) {
	Memcpy128(t, s, count);
    } else {
	passertMsg(0, "need case to handle cache line size of %d\n",
		   KernelInfo::PCacheLineSize());
    }
}

#endif //_FAST_ZERO
