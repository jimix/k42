#ifndef __DISPATCHER_DEFAULT_EXP_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __DISPATCHER_DEFAULT_EXP_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultExp.H,v 1.39 2003/10/27 15:59:29 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic user-level dispatcher functionality.
 * **************************************************************************/

class Thread;
class XBaseObj;
class DispatcherDefault;

struct DispatcherDefaultExpRegs {
		uval64		r0;

    union {	uval64		r3;
		SysStatus	returnCode;
		XHandle		PPC_xHandle;
		XBaseObj *	PPC_xObj;
		uval		Trap_trapNumber;
		uval		Pgflt_faultID;
    };

    union {	uval64		r4;
		uval		Trap_trapInfo;
		uval		Pgflt_faultInfo;
    };

    union {	uval64		r5;
		uval		Trap_trapAuxInfo;
		uval		Pgflt_faultAddr;
    };

		uval64		r6, r7, r8, r9;

    union {	uval64		r10;
		uval		PPC_methodNum;
    };

    union {	uval64		r11;
		uval		PPC_function;
    };

    union {	uval64		r12;
		DispatcherDefault *
				dispatcher;
    };

    union {	uval64		r14;
		CommID		PPC_callerID;
		CommID		PPC_calleeID;
    };

    union {	uval64		r15;
		uval		PPC_threadID;
    };

		uval64		r16, r17, r18, r19, r20, r21, r22, r23,
				r24, r25, r26, r27, r28, r29, r30, r31;

		uval64		f0,  f1,  f2,  f3,  f4,  f5,  f6,  f7,
				f8,  f9,  f10, f11, f12, f13, f14, f15,
				f16, f17, f18, f19, f20, f21, f22, f23,
				f24, f25, f26, f27, f28, f29, f30, f31;

		uval64		cr;
};
