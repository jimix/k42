#ifndef __GENERIC64_DISPATCHER_DEFAULT_EXP_H_
#define __GENERIC64_DISPATCHER_DEFAULT_EXP_H_
//#ifndef __DISPATCHER_DEFAULT_EXP_H_
//<<<< include machine independant file - not this machine dependent file >>>>
//#endif
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultExp.H,v 1.3 2001/11/13 18:29:19 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Define architecture specific entities for the base threads packages
 * **************************************************************************/

class Thread;
class XBaseObj;
class DispatcherDefault;


struct DispatcherDefaultExpRegs {


    union {
		SysStatus	returnCode; // match abi rc reg
		XHandle		PPC_xHandle; // ipc syscall parm
		XBaseObj *	PPC_xObj; // converted from xHandle
		uval		Trap_trapNumber; // trap upcall parm
		uval		Pgflt_faultID; // must match returncode
    };

    union {
		uval		PPC_methodNum; // must match exc reg
		uval		PPC_function; // converted from methodnum
    };

    union {
		uval		Trap_trapInfo; // trap upcall parm
		uval		Pgflt_faultInfo; // pf upcall parm
    };

    union {
		uval		Trap_trapAuxInfo; // trap upcall parm
		uval		Pgflt_faultAddr; // pf upcall parm
    };

    union {
		DispatcherDefault *dispatcher; // sched code only, also scratch
    };


    union {
		CommID		PPC_callerID; // must match exp, syscall parm
		CommID		PPC_calleeID; // same reg as caller for return
    };

    union {
		uval		PPC_threadID; // must match exp, syscall parm
    };

};

#endif /* #ifndef ... */
