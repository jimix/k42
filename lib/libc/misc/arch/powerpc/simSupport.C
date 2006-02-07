/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simSupport.C,v 1.17 2004/09/29 08:32:52 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: low-level interface to simos
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <misc/arch/powerpc/simSupport.H>
#include <misc/hardware.H>
#include <misc/baseStdio.H>

extern "C" sval
SimOSSupport(unsigned int foo, ...)
{
    sval rc;
    InterruptState is;
    uval enabled = hardwareInterruptsEnabled();
    if (enabled) disableHardwareInterrupts(is);
    asm("mr r3,%2\n"
	"mfmsr %0\n"
	"xori %0,%0,%1\n"
	"mtmsrd %0\n"
        ".long 0x7C0007CC\n"
	"mfmsr %0\n"
	"xori %0,%0,%1\n"
	"mtmsrd %0\n"
        "mr %0,r3"
	: "=r" (rc) : "i" (PSL_DR), "r" (foo): "r3");
    if (enabled) enableHardwareInterrupts(is);
    return rc;
}

extern "C" sval
MamboSupport(unsigned int foo, ...)
{
    sval rc;
    InterruptState is;
    uval enabled = hardwareInterruptsEnabled();
    if (enabled) disableHardwareInterrupts(is);
    asm("mr r3,%2\n"
	"mfmsr %0\n"
	"xori %0,%0,%1\n"
	"mtmsrd %0\n"
        ".long 0x000eaeb0\n"
	"mfmsr %0\n"
	"xori %0,%0,%1\n"
	"mtmsrd %0\n"
        "mr %0,r3"
	: "=r" (rc) : "i" (PSL_DR), "r" (foo): "r3");
    if (enabled) enableHardwareInterrupts(is);
    return rc;
}

extern "C" int
MamboCallthru2(int command, unsigned long arg1, unsigned long arg2)
{
    register int c asm ("r3") = command;
    register unsigned long a1 asm ("r4") = arg1;
    register unsigned long a2 asm ("r5") = arg2;
    asm volatile (".long 0x000EAEB0" : "=r" (c): "r" (c), "r" (a1), "r" (a2));
    return((c));
}

extern "C" int
MamboCallthru3(int command, unsigned long arg1, unsigned long arg2, unsigned long arg3)
{
    register int c asm ("r3") = command;
    register unsigned long a1 asm ("r4") = arg1;
    register unsigned long a2 asm ("r5") = arg2;
    register unsigned long a3 asm ("r6") = arg3;
    asm volatile (".long 0x000EAEB0" : "=r" (c): "r" (c), "r" (a1), "r" (a2), "r" (a3));
    return((c));
}

extern "C" int
MamboBogusDiskRead( int devno, void *buf, unsigned long sect,
			    unsigned long nrsect)
{
    return MamboCallthru3(SimBogusDiskReadCode, (unsigned long)buf,
			  (unsigned long)sect,
			  (unsigned long)((nrsect<<16)|devno));
}

extern "C" 
int MamboBogusDiskWrite(int devno, void *buf, unsigned long sect,
			unsigned long nrsect)
{
    return MamboCallthru3(SimBogusDiskWriteCode, (unsigned long)buf,
			  (unsigned long)sect,
			  (unsigned long)((nrsect<<16)|devno));
}
    
extern "C" int 
MamboBogusDiskInfo( int op, int devno)
{
    return(MamboCallthru2(SimBogusDiskInfoCode,(unsigned long)op,
			  (unsigned long)devno));
}

extern "C" int
MamboRunTCLCommand(char *tclCmd, uval tclCmdLen)
{
    return MamboCallthru2(SimCallTCLCode, (unsigned long)tclCmd, tclCmdLen);
}

extern "C" int
MamboGetEnv(const char *varName, int varNameLen, char *varValue)
{
    char tclcode[1024];
    uval tclcodelen;

    tclcodelen = baseSprintf(tclcode, "%s 0x%p %d 0x%p", "k42_getenv",
			     varName, varNameLen, varValue);
    return MamboCallthru2(SimCallTCLCode, (unsigned long)tclcode, tclcodelen);
}

extern "C" int
MamboGetKParmsSize(unsigned int *blockSize)
{
    char tclcode[1024];
    uval tclcodelen;
    uval bs;
    int retval;

    tclcodelen = baseSprintf(tclcode, "%s 0x%p", "k42_getkparms_size",
			     &bs);

    retval = MamboCallthru2(SimCallTCLCode, (unsigned long)tclcode, tclcodelen); 
    *blockSize = (unsigned int)bs;
    return retval;
}

extern "C" int
MamboGetKParms(void *dataBlock)
{
    char tclcode[1024];
    uval tclcodelen;

    tclcodelen = baseSprintf(tclcode, "%s 0x%p", "k42_getkparms",
			     dataBlock);

    return MamboCallthru2(SimCallTCLCode, (unsigned long)tclcode, tclcodelen); 
}

