/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRepArbiterTarget.H,v 1.1 2004/01/24 20:58:15 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#ifndef __COBJ_REP_ARBITERTARGET_H_
#define __COBJ_REP_ARBITERTARGET_H_

#include <cobj/sys/COVTable.H>
// FIXME This refers to arch/powerpc
#include <sys/arch/powerpc/asmConstants.H>
#include <cobj/CObjRepArbiter.H>
#include <cobj/CObjRootArbiter.H>

class CObjRepArbiterTarget : public CObjRep{
protected:
    static const uval numStacks = 16;
    friend void genConstants();
    friend class CORootArbiter;
    friend class CORootArbiter::CORootArbiterTarget;
    struct TempStack{
        DEFINE_LOCALSTRICT_NEW(TempStack);
        // this order is important since this structure is used in some
        // assembly language bits that aren't yet asmConstants-ified
        uval stackOffset;     // stack pointer
        uval stackBase;       // top of stack
        uval bottomSP;        // stack bottom, excluding reserved part
        uval truebottomSP;    // stack bottom, excluding reserved part
        TempStack* next;
	uval recurCount;
        static const uval stackSize = KERN_THREAD_SIZE;
        static const uval stackReserved = KERN_PGFLT_STK_SPACE;
        uval stack[stackSize / sizeof(uval)];
    };
    COSTransObject* rep;        // call this rep normally
    ArbiterRef arbitrater;	// ref to associated Arbiter interface
    TempStack* tempStacks;
    BLock stackLock;
    static COVTable* vtable;

    // get target rep when instantiated
    CObjRepArbiterTarget(ArbiterRef cr);
    DEFINE_LOCALSTRICT_NEW(CObjRepArbiterTarget);
    void initVTable();

public:
    // have to destroy target rep as well
    ~CObjRepArbiterTarget();
};

#endif
