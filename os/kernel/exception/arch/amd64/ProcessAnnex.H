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
 * $Id: ProcessAnnex.H,v 1.9 2002/05/02 20:15:19 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the machine-dependent parts of the
 *                     exception-level kernel structure that represents a
 *                     virtual processor.
 * **************************************************************************/

// VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV fixme straight x86 VVVVVVVVVVVVVVVVVVVVVVV

#include <misc/hardware.H>
#include __MINC(amd64_hw.H)
#include <exception/ExceptionLocal.H>
#include <arch/amd64/segnum.h>              // KERNEL_CS, ...

struct EntryPointLauncher {
    codeAddress launchCode;
    FaultFrame ff;
public:
    void set(uval userMode, EntryPointDesc entry) {
	ff.rip = entry.rip;
	ff.rflags = 0x200;	// FIXME: need symbolic name for interrupts-enabled
	ff.rsp = 0;		// gets the stack later (provided in the launched code)

	/* on amd64 in long mode all segment register values are
	 * ignored except for priority levels in CS.
	 *
	 * The intent here is to launch where the launch code
	 * will set up its own stack and should run interrupt
	 * enabled while we are currently interrupt disabled.  By
	 * using an iret to launch we make sure that the RFFLAGS
	 * gets enabled as part of the iret execution and any
	 * interrupt hereafter will be handled on the normal ITS
	 * defined stack.  */

	if(userMode) {
	   ff.cs = USER_CS;
	   ff.ss = USER_SS;	// must be non zero if going to user mode
	}
	else
	   ff.cs = KERNEL_CS;

	launchCode = &ExceptionLocal_LaunchUserOrKernel;

    }

    void setException(codeAddress handler) {
	launchCode = handler;
	// FaultFrame is not used.
	ff.rip = 0;
	ff.cs = 0;
	ff.rflags = 0;
	ff.rsp = 0;
	ff.ss = 0;
    }
};

struct ProcessAnnexMachine {
    uval esp0Base;		// base address for computing esp0
    uval esp0;			// points to end of exc-level save area
    uval syscallFramePtr;	// place to save frame pointer when a syscall
				//     (e.g. PPCPrimitive) suspends its
				//     ProcessAnnex.

public:
    void init(uval userMode, Dispatcher *sp, SegmentTable *segTable) {
	esp0Base = ((uval) sp) + sizeof(VolatileState);
    };

    void setExcStateOffset(uval offset) {
	esp0 = esp0Base + offset;
    }

    void switchExcStateSaveArea();
};
