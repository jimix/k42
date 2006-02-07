/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linkage.C,v 1.2 2001/12/30 18:06:45 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions and services related to linkage
 *                     conventions
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linkage.H"

struct X86Frame {
    X86Frame*	backchain;
    uval	caller;
};

SysStatus
GetCallChain(uval startFrame, uval* callChain, uval callCount)
{

    X86Frame *frame = (X86Frame *) startFrame;

    for (uval i = 0; i < callCount; i++) {
	if (frame != NULL) {
	    callChain[i] = frame->caller;
	    frame = frame->backchain;
	} else {
	    callChain[i] = 0;
	}
    }

    return 0;
}

SysStatus
GetCallChainSelf(uval skip, uval* callChain, uval callCount)
{
#ifndef NDEBUG
    tassertMsg(0, "must compile w/ frame pointer\n");
#endif /* #ifndef NDEBUG */
    X86Frame* frame;
    asm("movq %%rbp,%0" : "=r" (frame));
    // Start one level back to skip our immediate caller.
    /*
     * Skip the first frame unconditionally.  It holds our immediate
     * caller's address, which we assume is uninteresting to our caller.
     */
    skip += 1;
    while ((skip-- > 0) && (frame != NULL)) {
	frame = frame->backchain;
    }
    return GetCallChain((uval) frame, callChain, callCount);
}
