/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysCallInitKern.C,v 1.4 2004/08/27 20:17:03 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arch specific syscall entry point initialization
 * **************************************************************************/

#include <kernIncs.H>
#include <emu/sandbox.H>

extern "C" sval
K42Linux_SVC64(uval a, uval b, uval c, uval d, uval e, uval f,
	       uval stkPtr, uval svcNum)
{
    passertMsg(0, "K42Linux_SVC64 called in kernel!.\n");
    return 0;
}

extern "C" sval
K42Linux_SVC32(uval a, uval b, uval c, uval d, uval e, uval f,
	       uval stkPtr, uval svcNum)
{
    passertMsg(0, "K42Linux_SVC32 called in kernel!.\n");
    return 0;
}

extern "C" void
K42Linux_SandboxUpcall(VolatileState *vsp, NonvolatileState *nvsp)
{
    passertMsg(0, "K42Linux_SandboxUpcall called in kernel!.\n");
}

extern "C" void
K42Linux_SandboxTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		     VolatileState *vsp, NonvolatileState *nvsp)
{
    passertMsg(0, "K42Linux_SandboxTrap called in kernel!.\n");
}
