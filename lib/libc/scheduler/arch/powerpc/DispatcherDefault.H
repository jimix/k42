#ifndef __DISPATCHER_DEFAULT_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __DISPATCHER_DEFAULT_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefault.H,v 1.13 2004/08/27 20:16:41 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    Basic user-level dispatcher functionality.
 * **************************************************************************/

#include <misc/hardware.H>

class Thread;

struct DispatcherDefaultArch {
    uval64 msr;			// MSR value when not in sandbox mode
    uval64 fpscr;		// current default for floating-point
				//     status/control register
    uval64 alignHdlrStkPtr;	// stack pointer for short-circuiting the
				//     the alignment handler

    void init() { fpscr = 0; msr = PSL_USERSET; alignHdlrStkPtr = 0; }
};

register Thread *CurrentThread __asm__("r13");
