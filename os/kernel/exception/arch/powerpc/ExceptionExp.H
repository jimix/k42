#ifndef __EXCEPTION_EXP_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __EXCEPTION_EXP_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionExp.H,v 1.19 2004/09/22 13:01:43 rosnbrg Exp $
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

struct ExceptionExpRegs {
    union {	uval64		r0;
		uval		disabledSave;
    };

    union {	uval64		r3;
		SysStatus	returnCode;
		ProcessAnnex *	srcProc;
		VolatileState *	volatileState;
		uval		Trap_trapNumber;
		XHandle		IPC_xHandle;
		IPCRegsArch *   IPC_ipcRegsP;
    };

    union {	uval64		r4;
		uval		Trap_trapInfo;
		uval		Pgflt_faultInfo;
    };

    union {	uval64		r5;
		uval		Trap_trapAuxInfo;
		uval		Pgflt_faultAddr;
    };

    union {	uval64		r6;
		uval		Pgflt_noReflection;
    };

		uval64		r7,  r8;

    union {	uval64		r9;
		CommID		IPCAsync_targetID;
    };

    union {	uval64		r10;
		uval		IPC_methodNum;
    };

    union {	uval64		r11;
		ProcessAnnex *	curProc;
    };

    union {	uval64		r12;
		DispatcherDefaultKern *
				dispatcher;
		Dispatcher *    curDispatcher;
		uval		IPC_ipcType;
    };

    union {	uval64		r14;
		CommID		IPC_targetID;
		CommID		IPC_callerID;
    };

    union {	uval64		r15;
		uval		IPC_threadID;
    };

		uval64		r16, r17, r18, r19, r20, r21, r22, r23,
				r24, r25, r26, r27, r28, r29, r30, r31;

		uval64		f0,  f1,  f2,  f3,  f4,  f5,  f6,  f7,
				f8,  f9,  f10, f11, f12, f13, f14, f15,
				f16, f17, f18, f19, f20, f21, f22, f23,
				f24, f25, f26, f27, f28, f29, f30, f31;

		uval64		cr;

    uval PgfltTrapNumber() {
	return EXC_DSI;  // may have been ISI, but we can't tell now
    }

    uval SVCTrapNumber() {
	return EXC_SC;
    }

    void saveSVCRegs(VolatileState *vs) {
	// already saved: iar, msr, lr, r1, r2
	vs->r0 = r0;
	memcpy(&vs->r3,  &r3,   (10 - 3 + 1) * sizeof(uval64));	// r3 - r10
	memcpy(&vs->f0,  &f0,   (13 - 0 + 1) * sizeof(uval64));	// f0 - f13
	vs->cr = cr;
	// not preserved: ctr, xer, fpscr
    }

    void saveIPCRegs(IPCRegsArch *ipcRegsP) {
	memcpy(&ipcRegsP->r3_r10,  &r3,   8 * sizeof(uval64));
	memcpy(&ipcRegsP->r15_r31, &r15, 17 * sizeof(uval64));
	memcpy(&ipcRegsP->f0_f31,  &f0,  32 * sizeof(uval64));
	ipcRegsP->cr = cr;
    }

    void restoreIPCRegs(IPCRegsArch *ipcRegsP) {
	memcpy(&r3,  &ipcRegsP->r3_r10,   8 * sizeof(uval64));
	memcpy(&r15, &ipcRegsP->r15_r31, 17 * sizeof(uval64));
	memcpy(&f0,  &ipcRegsP->f0_f31,  32 * sizeof(uval64));
	cr = ipcRegsP->cr;
    }
};
