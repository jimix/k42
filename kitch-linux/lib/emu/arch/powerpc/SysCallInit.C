/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysCallInit.C,v 1.38 2005/05/03 21:18:28 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arch specific syscall entry point initialization
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/Dispatcher.H>
#include <elf.h>
#include <asm-ppc/unistd.h>   //Syscall numbers
#include <sys/ProcessLinuxClient.H>
#define __NR_UNUSED_224 224
#include "SysCallTable.H"

uval TraceSyscalls = 0;

SysStatus initEntryPoints()
{
    Scheduler::SetupSVCDirect();
    TraceSyscalls = 0;
    return 0;
}

void traceLinuxSyscalls()
{
    TraceSyscalls = 1;
    passertMsg((sizeof(SyscallTraced64) == sizeof(Syscall64)) &&
	       (sizeof(SyscallTraced32) == sizeof(Syscall32)),
	       "Size mismatch between traced and untraced syscall vectors.\n");
}

#define ZERO_EXT(x) uval(uval32(x))

extern "C" sval
K42Linux_SVC64(uval a, uval b, uval c, uval d, uval e, uval f,
	       uval stkPtr, uval svcNum)
{
    uval rcode;

    tassertMsg(Scheduler::GetCurThreadPtr()->isActive(), "Not active!\n");

    if (svcNum < (sizeof(SyscallTraced64)/sizeof(SyscallTraced64[0]))) {
	if (TraceSyscalls) {
	    rcode = (SyscallTraced64[svcNum])(a, b, c, d, e, f, stkPtr);
	} else {
	    rcode = (Syscall64[svcNum])(a, b, c, d, e, f, stkPtr);
	}
    } else {
	rcode = uval(-ENOSYS);
    }

    return rcode;
}

extern "C" sval
K42Linux_SVC32(uval a, uval b, uval c, uval d, uval e, uval f,
	       uval stkPtr, uval svcNum)
{
    uval rcode;

    if (svcNum < (sizeof(SyscallTraced32)/sizeof(SyscallTraced32[0]))) {
	if (TraceSyscalls) {
	    rcode = (SyscallTraced32[svcNum])(ZERO_EXT(a), ZERO_EXT(b),
					      ZERO_EXT(c), ZERO_EXT(d),
					      ZERO_EXT(e), ZERO_EXT(f),
					      stkPtr);
	} else {
	    rcode = (Syscall32[svcNum])(ZERO_EXT(a), ZERO_EXT(b),
					ZERO_EXT(c), ZERO_EXT(d),
					ZERO_EXT(e), ZERO_EXT(f),
					stkPtr);
	}
    } else {
	rcode = uval(-ENOSYS);
    }

    return rcode;
}

extern "C" void
K42Linux_SandboxUpcall(VolatileState *vsp, NonvolatileState *nvsp)
{
    ProcessLinuxClient::SandboxUpcall(vsp, nvsp);
}

extern "C" void
K42Linux_SandboxTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
		     VolatileState *vsp, NonvolatileState *nvsp)
{
    ProcessLinuxClient::SandboxTrap(trapNumber, trapInfo, trapAuxInfo,
				    vsp, nvsp);
}
