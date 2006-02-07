#ifndef __VOLATILE_STATE_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __VOLATILE_STATE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VolatileState.H,v 1.16 2004/06/28 17:01:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <misc/hardware.H>

struct VolatileState {
    uval64 iar;		// instruction address register
    uval64 msr;		// machine status register
    uval64 cr;		// condition register
    uval64 ctr;		// count register
    uval64 lr;		// link register
    uval64 xer;		// fixed-point exception register
    uval64 fpscr;	// floating-point status and control register
    uval64 r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13; // volatile gprs
    uval64 f0,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13; // volatile fprs

    codeAddress codeAddr() { return (codeAddress) iar; }
    codeAddress branchReg() { return (codeAddress) lr; }

    void setStackPtr(uval stkptr) { r1 = stkptr; }
    void setReturnCode(uval rcode) { r3 = rcode; }

    void init(uval stkptr, EntryPointDesc *entry, uval is32Bit) {
	memset(this, 0, sizeof(*this));
	r1 = stkptr;
	r2 = entry->toc;
	iar = uval64(entry->iar);
	msr = is32Bit ? (PSL_USERSET & ~PSL_SF) : PSL_USERSET;
    }
};
