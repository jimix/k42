#ifndef __DISPATCHER_DEFAULT_EXP_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __DISPATCHER_DEFAULT_EXP_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultExp.H,v 1.10 2001/11/13 18:29:17 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic user-level dispatcher functionality.
 * **************************************************************************/

// VVV

#include "misc/arch/amd64/fpr.h"          /* floating point register save area size */

class Thread;
class XBaseObj;
class DispatcherDefault;


/* KEEP THIS STRUCTURE the same as struct struct ExpRegs */
/* KEEP THIS STRUCTURE the same as struct ExceptionExpRegs */
struct DispatcherDefaultExpRegs {

    uval64 float_savearea[FPR_SAVE_AREA_SIZE/sizeof(uval64)];	// floating point save/restore area

    union {
                uval64		rax;
		SysStatus	returnCode;		// match ABI for INTEGER return code,  w/ variable arguments is # of SSE's used.
		uval		Pgflt_faultID;		// must match returncode, passed as a return code in the old days, historical
    };

		uval64		rbx;			// callee saved, non volatile, optionally used as base pointer

		uval64		rcx;			// arg 4 register for INTEGER

    union {	uval64		rdx;			// arg 3 ABI register for INTEGER
		uval		Trap_trapAuxInfo;	// trap upcall parm
		uval		Pgflt_faultAddr;	// pf upcall parm
    };

		uval64		rbp;

    union {	uval64		rsi;			// arg 2 ABI register for INTEGER
		uval		Trap_trapInfo;		// trap upcall parm
		uval		Pgflt_faultInfo;	// pf upcall parm
    };


    union {	uval64		rdi;			// arg 1 ABI register for INTEGER
		XHandle		PPC_xHandle;		// ipc syscall parm
		XBaseObj *	PPC_xObj;		// converted from xHandle
		uval		Trap_trapNumber;	// trap upcall parm
    };

		uval64		r8;			// arg 5 ABI register for INTEGER

    union {	uval64		r9;			// arg 6 ABI
		uval		PPC_methodNum;		// must match exc reg
		uval		PPC_function;		// converted from methodnum
		uval            Trap_x86ErrorCode;
    };

    		uval64		r10;			// scratch ABI

    union {	uval64		r11;			// scratch ABI
		DispatcherDefault * dispatcher;		// sched code only, also scratch XXX
    };

    union {	uval64		r12;			// must be  non volatile ABI
		CommID		PPC_callerID;		// must match exp, syscall parm
		CommID		PPC_calleeID;		// same reg as caller for return
    };

    union {	uval64		r13;			// must be  non volatile ABI
		uval		PPC_threadID;		// must match exp, syscall parm
    };

		uval64		r14;		// non volatile ABI
		uval64		r15;		// non volatile ABI

  		uval64		rsp;

};


