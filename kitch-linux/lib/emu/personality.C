/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: personality.C,v 1.7 2004/06/17 01:34:30 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: set the process execution domain
 *		Needed for calling linux lib init
 * **************************************************************************/
#include <sys/sysIncs.H>

/* FIXME:
 * this is really a glibc FIXME since mips/asm/ptrace.h does not require
 * linux/types.h, so we block it by using the guard.
 */
#define _LINUX_TYPES_H

#include <linux/personality.h>
#include "linuxEmul.H"

/* FIXME: this function is not actually doing anything.  */
int
__k42_linux_personality (unsigned long personality)
{
    if (personality != PER_LINUX) {
	tassertWrn(0, "Attempt to change to unsupported personality\n");
	return -EINVAL;
    }

    return PER_LINUX;
}

extern "C" sval32
__k42_linux_personality_32 (uval32 personality)
{
    return __k42_linux_personality(ZERO_EXT(personality));
}
