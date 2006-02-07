#ifndef __PPCCORE_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __PPCCORE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppccore.H,v 1.8 2004/03/09 19:32:24 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine-specific declarations covering the interface
 * between assembler and C parts of the PPC implementation.
 * **************************************************************************/

#include <sys/syscalls.H>
#include <misc/asm.h>

extern "C" SysStatus DispatcherDefault_PPCAsync(XHandle xhandle,
                                               uval methnum,
                                               CommID targetID);

#if 0	// not needed in ppccoreuser.H
void
PPC_ASYNC(SysStatus &rc, CommID targetID, XHandle xhandle, uval methnum)
{
    rc = DispatcherDefault_PPCAsync(xhandle, methnum, targetID);
}
#endif /* #if 0	// not needed in ppccoreuser.H */


#if 0 // I could not make it work XXX
inline void
PPC_ASYNC(SysStatus &rc, ProcessID pid, XHandle xhandle, uval methnum)
{

    __asm__ __volatile__("										\
	pushq	%r12		/* save r12 */								\
	movq	%1, %r12	/* move PPC_callerID in r12 */						\
	movq	%2, %rdi	/* move xhandle in rdi */						\
	movq	%3, %r9		/* move PPC_methodNUM in r9 */						\
//        int %4; "											\
        : "=rax" (rc)											\
        : "r" (pid), "r" (xhandle), "r" (methnum), "i" (SYSCALL_IPC_ASYNC)				\
        : "cc", "memory", "rax", "rdi", "rsi", "rcx", "rdx", "r8", "r9", "r10", "r11", "r12"		\
    );
}
#endif /* #if 0 // I could not make it work XXX */


/* FIXME: these declarations should be in DispatcherDefault.H, probably.
 * this has been plagiarized from powerpc because I could not make the x86 asm to work
 * as expected below. DispatcherDefault_PPCClient related ??? XXX pdb
 */
extern "C" SysStatus DispatcherDefault_PPCCall(XHandle xhandle,
                                               uval methnum,
                                               CommID targetID);

inline void
PPC_CALL(SysStatus &rc, CommID targetID, XHandle xhandle, uval methnum)
{
    rc = DispatcherDefault_PPCCall(xhandle, methnum, targetID);
}

#if 0 // I could not make it work XXX

/* ProcessID pid is the PPC_calleeID in the Expedient code, which in the case of amd64,
 * should be  in r12, which means that r12 is clobbered.
 * methnum ends up in r9
 * xhandle into rdi
 * needs to check that this generate correct code, in particular int SYSCALL_IPC_ASYNC
 * the assembler code is somewhat more closely derived from mips64 than powerpc, hence
 * mips64 may be better reference here than powerpc. XXX
 */

inline void
PPC_CALL(SysStatus &rc, ProcessID pid, XHandle xhandle, uval methnum)
{
    __asm__ __volatile__("									 	\
        call C_TEXT(DispatcherDefault_PPCClient); " 								\
        : "=rax" (rc)											\
        : "r12" (pid), "rdi" (xhandle), "r9" (methnum)							\
        : "cc", "memory", "rax", "rdi", "rsi", "rcx", "rdx", "r8", "r9", "r10", "r11", "r12"		\
    );
}
#endif /* #if 0 // I could not make it work XXX */

