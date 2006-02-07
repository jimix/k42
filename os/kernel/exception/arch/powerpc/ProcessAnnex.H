#ifndef __PROCESS_ANNEX_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __PROCESS_ANNEX_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessAnnex.H,v 1.14 2002/05/02 20:15:31 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the machine-dependent parts of the
 *                     exception-level kernel structure that represents a
 *                     virtual processor.
 * **************************************************************************/

#include <misc/hardware.H>

struct EntryPointLauncher {
    codeAddress iar;
    uval64 msr;
    uval64 toc;
public:
    void set(uval userMode, EntryPointDesc entry) {
	iar = entry.iar;
	toc = entry.toc;
	msr = PSL_EE | (userMode ? PSL_USERSET : PSL_KERNELSET);
    }
    void setException(codeAddress handler) {
	iar = handler;
	msr = PSL_KERNELSET;
	asm("std r2,%0" : "=m" (toc) : ); // FIXME
    }
};

struct ProcessAnnexMachine {
    uval64 msr;
    uval64 asr;
    uval64 syscallStackPtr;
    uval64 syscallReturnAddr;
    uval dispatcherPhysAddr;
    uval excStatePhysAddr;

public:
    void init(uval userMode, Dispatcher *disp, SegmentTable *segTable);

    void setExcStateOffset(uval offset) {
	excStatePhysAddr = dispatcherPhysAddr + offset;
    }

    void switchExcStateSaveArea() {
	asm volatile("mtsprg 1,%0"  : : "r" (excStatePhysAddr));
    }
};
