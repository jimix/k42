/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: openfirm.C,v 1.31 2004/08/20 11:14:52 mostrows Exp $
 *****************************************************************************/

//FIXME rtas does not work when called from K42!
//      routines do not seem to return - problem unkown.
//      specifically, time routines cannot be used
#include "kernIncs.H"
#include "openfirm.h"
#include "BootInfo.H"
#include <misc/hardware.H>
#include <sync/SLock.H>

typedef SLock LockType;		// spin lock
static LockType rtasLock;

static codeAddress rtasentry, rtasbase;

static uval ofVirtBase;
static int *ofVirtArgs;

#define V2R(addr) ((uval)(addr) - (ofVirtBase))
#define R2V(addr) ((uval)(addr) + (ofVirtBase))

extern "C" sval openfirmware(uval32, codeAddress, codeAddress, codeAddress);
extern "C" sval ofreal(uval32, codeAddress);

// Routine to interface with OF/RTAS early on during system boot
extern "C" sval
ofSetupBoot(int *virtArgs)
{
    sval rc;
    InterruptState is;

    uval32 realArgs = (uval32) V2R(virtArgs);
    codeAddress ofRealFunc = (codeAddress) V2R(*(codeAddress *)ofreal);

    uval wasEnabled = hardwareInterruptsEnabled();

    if (wasEnabled) {
        disableHardwareInterrupts(is);
    }

    rc = openfirmware(realArgs, rtasbase, ofRealFunc, rtasentry);

    if (wasEnabled) {
        enableHardwareInterrupts(is);
    }

    return rc;
}


extern "C" sval
ofSetup(int *virtArgs)
{
    sval rc;
    InterruptState is;

    uval32 realArgs = (uval32) V2R(virtArgs);
    codeAddress ofRealFunc = (codeAddress) V2R(*(codeAddress *)ofreal);

    uval wasEnabled = hardwareInterruptsEnabled();

    if (wasEnabled) {
        disableHardwareInterrupts(is);
    }

    rtasLock.acquire();
    rc = openfirmware(realArgs, rtasbase, ofRealFunc, rtasentry);
    rtasLock.release();

    if (wasEnabled) {
        enableHardwareInterrupts(is);
    }

    return rc;
}

#if 0
// Called from linux kernel code, same as ofSetup, but arg is already phys addr
extern "C" sval
enter_rtas(int *realArgs)
{
    sval rc;
    InterruptState is;

    //realArgs is really a 32-bit pointer
    uval32 args = (uval32)((uval64)realArgs);
    codeAddress ofRealFunc = (codeAddress) V2R(*(codeAddress *)ofreal);

    uval wasEnabled = hardwareInterruptsEnabled();

    if (wasEnabled) {
        disableHardwareInterrupts(is);
    }

    rtasLock.acquire();
    rc = openfirmware(args, rtasbase, ofRealFunc, rtasentry);
    rtasLock.release();

#if 0 /* turn this on to print all rtas calls */
    err_printf("enter_rtas(%x ->", args);
    int i, *p;
    for (i = 0, p = (int *)R2V(args); i < 12; i++, p++) {
	err_printf(" %x", *p);
    }
    err_printf(") = %d\n", (int)rc);
#endif /* #if 0 */

    if (wasEnabled) {
        enableHardwareInterrupts(is);
    }

    return rc;
}
#endif

extern "C" void call_rtas_display_status(char c);
extern "C" sval32
rtas_display_character(sval32 value)
{
    call_rtas_display_status((char)value);
    return 0;
}


void
rtas_init(uval virtBase)
{
    rtasentry = (codeAddress) _BootInfo->rtas.entry;
    rtasbase = (codeAddress) _BootInfo->rtas.base;

    ofVirtBase = virtBase;
    ofVirtArgs = (int *) _BootInfo->rtas_parameter_buf;

    return;
}


// temporary Debugging facilities
#if 0
extern "C" uval __tick(uval real_tick, uval ioAddr, char c);
extern uval real_tick[0];
void tick(char c)
{
    uval addr = 0x80013020;
    uval rt = ((uval)real_tick) - 0xc000000000000000ULL;
    asm volatile ("":::"memory");
    __tick(rt, addr,c);
}


void printUval(uval x)
{
    for (uval i=0; i<64; i+=4) {
	uval y = (x >> 60) & 0xf;
	x=x<<4;
	if (y<10)
	    tick(y+'0');
	else
	    tick(y+'A'-10);
    }
}
#else
uval tick(char c) {
    return 0;
}
void printUval(uval x) {
    return;
}

#endif
