#ifndef __EXCEPTION_EXP_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __EXCEPTION_EXP_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionExp.H,v 1.8 2004/04/06 21:00:40 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Define architecture specific entities for the base threads packages
 * **************************************************************************/

class ProcessAnnex;
class VolatileState;
class Thread;
class DispatcherDefaultKern;
class Dispatcher;

#include "misc/arch/amd64/fpr.h"          /* floating point register save area size */

/* KEEP THIS STRUCTURE the same as struct struct ExpRegs */
/* KEEP THIS STRUCTURE the same as struct DispatcherDefaultExpRegs */
struct ExceptionExpRegs {

    uval64 float_savearea[FPR_SAVE_AREA_SIZE/sizeof(uval64)];	// floating point save/restore area


    union {	uval64		rax;			// popwerpc part of r3
		SysStatus	returnCode;		// must match ABI for returning INTEGER return code
		uval		disabledSave;
							// temporary register, w/ variable arguments passes number of SSE's used.
    };


		uval64		rbx;			// callee saved, non volatile, optionally used as base pointer


    union {	uval64		rcx;			// powwerpc r6
		uval		Pgflt_noReflection;
    };


    union {	uval64		rdx;			// powerpc r5
		uval		Trap_trapAuxInfo;	// trap upcall parm
		uval		Pgflt_faultAddr;	// pf upcall parm
    };


		uval64		rbp;


    union {	uval64		rsi;			// powerpc r4
		uval		Trap_trapInfo;
		uval		Pgflt_faultInfo;
    };


    union {	uval64		rdi;			// powerpc r3 part of
		ProcessAnnex *	srcProc;
		VolatileState *	volatileState;
		uval		Trap_trapNumber;
		XHandle		IPC_xHandle;
		IPCRegsArch *   IPC_ipcRegsP;
    };


    union {		uval64		r8;			// arg 5 ABI register for INTEGER
		CommID		IPCAsync_targetID; 	// must match sched
    };


    union {	uval64		r9;			// ppc r0
		uval		IPC_methodNum;
    };


    union { 	uval64		r10;			// powerpc r11
		ProcessAnnex *	curProc;
    };


    union {	uval64		r11;			// powerpc r12
		DispatcherDefaultKern *
				dispatcher;
		Dispatcher *    curDispatcher;
		uval		IPC_ipcType;
    };


    union {	uval64		r12;			// powerpc r14
		CommID		IPC_targetID;
		CommID		IPC_callerID;
    };


    union {	uval64		r13;			// powerpc r15
		uval		IPC_threadID;
    };


		uval64		r14;		// non volatile ABI
		uval64		r15;		// non volatile ABI

  		uval64		rsp;


    uval PgfltTrapNumber() {
        return 14;  // pgflt interrupt numberI believe still the case on amd64, XXX
    }

    uval SVCTrapNumber() {
#if 0	// FIXME:  translate from powerpc64:
	return EXC_SC;
#endif
	return uval(-1);
    }

    void saveSVCRegs(VolatileState *vs) {
#if 0	// FIXME:  translate from powerpc64:
	// already saved: iar, msr, lr, r1, r2
	vs->r0 = r0;
	memcpy(&vs->r3,  &r3,   (10 - 3 + 1) * sizeof(uval64));	// r3 - r10
	memcpy(&vs->f0,  &f0,   (13 - 0 + 1) * sizeof(uval64));	// f0 - f13
	vs->cr = cr;
	// not preserved: ctr, xer, fpscr
#endif
    }

    void saveIPCRegs(IPCRegsArch *ipcRegsP) {
	ipcRegsP->rax = rax;
	ipcRegsP->rbx = rbx;
	ipcRegsP->rcx = rcx;
	ipcRegsP->rdx = rdx;
	ipcRegsP->rbp = rbp;
	ipcRegsP->rsi = rsi;
	ipcRegsP->rdi = rdi;
	ipcRegsP->r8  = r8;
	ipcRegsP->r9  = r9;
	ipcRegsP->r13 = r13;
	ipcRegsP->r14 = r14;
	ipcRegsP->r15 = r15;
	memcpy(&ipcRegsP->float_savearea, &float_savearea, FPR_SAVE_AREA_SIZE);
    }

    void restoreIPCRegs(IPCRegsArch *ipcRegsP) {
	rax = ipcRegsP->rax;
	rbx = ipcRegsP->rbx;
	rcx = ipcRegsP->rcx;
	rdx = ipcRegsP->rdx;
	rbp = ipcRegsP->rbp;
	rsi = ipcRegsP->rsi;
	rdi = ipcRegsP->rdi;
	r8  = ipcRegsP->r8;
	r9  = ipcRegsP->r9;
	r13 = ipcRegsP->r13;
	r14 = ipcRegsP->r14;
	r15 = ipcRegsP->r15;
	memcpy(&float_savearea, &ipcRegsP->float_savearea, FPR_SAVE_AREA_SIZE);
    }
};
