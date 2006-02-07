#ifndef __SIGNAL_UTILS_H_
#define __SIGNAL_UTILS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SignalUtils.H,v 1.3 2004/08/27 20:16:24 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: utility routines for dealing with signals
 * **************************************************************************/

#include <sys/VolatileState.H>
#include <misc/asm.h>

class SignalUtils {
public:
    enum SignalReturnType {SIGRETURN_NONE,
			   SIGRETURN_64, SIGRETURN_RT_64,
			   SIGRETURN_32, SIGRETURN_RT_32};

    static void SignalReturn(SignalReturnType srType, uval stkPtr,
			     VolatileState *vsp, NonvolatileState *nvsp,
			     uval64& oldmask);

    static void PushSignal(VolatileState *vsp, NonvolatileState *nvsp,
			   __sighandler_t handler, uval flags, sval sig,
			   uval64 oldmask);
    static void ConvertTrapToSignal(uval trapNumber, uval trapInfo,
				    uval trapAuxInfo,
				    VolatileState *vsp, NonvolatileState *nvsp,
				    sval &sig);
};

#endif /* __SIGNAL_UTILS_H_ */
