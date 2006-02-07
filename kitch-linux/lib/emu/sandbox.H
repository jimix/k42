/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sandbox.H,v 1.4 2004/08/27 20:16:24 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: declarations for calls that are made up to this level
 *                     from the sandbox support code.
 * **************************************************************************/

#ifndef __SANDBOX_H_
#define __SANDBOX_H_

#include <sys/VolatileState.H>
#include <misc/asm.h>

typedef uval (*SyscallHandler)(uval, uval, uval, uval, uval, uval, uval);

SysStatus initEntryPoints();
void traceLinuxSyscalls();

extern "C" sval K42Linux_SVC64(uval a, uval b, uval c, uval d, uval e, uval f,
			       uval stkPtr, uval svcNum);

extern "C" sval K42Linux_SVC32(uval a, uval b, uval c, uval d, uval e, uval f,
			       uval stkPtr, uval svcNum);

extern "C" void K42Linux_SandboxUpcall(VolatileState *vsp,
				       NonvolatileState *nvsp);

extern "C" void K42Linux_SandboxTrap(uval trapNumber,
				     uval trapInfo,
				     uval trapAuxInfo,
				     VolatileState *vsp,
				     NonvolatileState *nvsp);

#endif /* #ifndef __SANDBOX_H_ */
