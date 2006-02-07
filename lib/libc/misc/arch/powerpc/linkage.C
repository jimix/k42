/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linkage.C,v 1.11 2003/03/12 13:55:50 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions and services related to linkage
 *                     conventions
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linkage.H"

/*
 * In power pc, the frame pointer points to the lowest address
 * in the frame.  The caller address is in the third word of the
 * callers frame
 */
struct PwrPCFrame {
    PwrPCFrame*	backchain;
    uval	cr;
    uval	lr;
    uval	compilerReserved;
    uval	linkerReserved;
    uval	toc;
    uval	param[8];
};

SysStatus
GetCallChain(uval startFrame, uval* callChain, uval callCount)
{

    PwrPCFrame *frame = (PwrPCFrame *) startFrame;

    for (uval i = 0; i < callCount; i++) {
	if (frame != NULL) {
	    callChain[i] = frame->lr;
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
    PwrPCFrame* frame;
    asm volatile("mr %0,r1" : "=r" (frame));
    /*
     * Skip the first two frames unconditionally.  The first is for use by
     * our callee.  The second holds our immediate caller's address, which
     * we assume is uninteresting to our caller.
     */
    skip += 2;
    while ((skip-- > 0) && (frame != NULL)) {
	frame = frame->backchain;
    }
    return GetCallChain((uval) frame, callChain, callCount);
}
