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
 * $Id: ExceptionExp.H,v 1.5 2004/04/06 21:00:41 rosnbrg Exp $
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

#include <misc/arch/generic64/trap.h>   // for EXC_DSI

struct ExceptionExpRegs {
    union {
		uval		disabledSave;
    };

    union {
		uval		IPC_methodNum;
    };

    union {
		SysStatus	returnCode;
		ProcessAnnex *	srcProc;
		VolatileState *	volatileState;
		uval		Trap_trapNumber;
		XHandle		IPC_xHandle;
		IPCRegsArch *   IPC_ipcRegsP;
    };

    union {
		uval		Trap_trapInfo;
		uval		Pgflt_faultInfo;
    };

    union {
		uval		Trap_trapAuxInfo;
		uval		Pgflt_faultAddr;
    };

    union {
		uval		Pgflt_noReflection;
    };

    union {
		CommID		IPCAsync_targetID;
    };

    union {
		ProcessAnnex *	curProc;
    };

    union {
		DispatcherDefaultKern *dispatcher;
		Dispatcher *    curDispatcher;
		uval		IPC_ipcType;
    };

    union {
		CommID		IPC_targetID;
		CommID		IPC_callerID;
    };

    union {
		uval		IPC_threadID;
    };

    uval SVCTrapNumber() {
	return EXC_SC;
    }

    void saveSVCRegs(VolatileState *vs) {
    }

    uval PgfltTrapNumber() {
      return EXC_DSI;
    }

    void saveIPCRegs(IPCRegsArch *ipcRegsP) {
    }

    void restoreIPCRegs(IPCRegsArch *ipcRegsP) {
    }
};
