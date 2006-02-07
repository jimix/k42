/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppc32.C,v 1.2 2004/06/14 20:48:02 apw Exp $
 *****************************************************************************/
#include <lk/lkIncs.H>

extern "C" {
#include <asm-ppc64/compat.h>
}

/*
 * we can't include linuxEmul.H here because of header file clashes
 */

extern "C"
uval __k42_linux_stime(time_t* t);

extern "C"
uval __k42_linux_stime_32(const compat_time_t* t32p)
{
    time_t t64;
    t64 = *t32p;
    return __k42_linux_stime(&t64);
}
