/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bios_client.C,v 1.3 2001/11/01 01:09:25 mostrows Exp $
 *****************************************************************************/

// probably not needed, staright from x86 XXX

#include "kernIncs.H"
#include "bios_client.H"
#include <proc/Process.H>
#include <mem/HATKernel.H>
#include <sync/SLock.H>

struct {
    BiosServiceRoutine eip;
    uval16 cs;
} bios_service_ptr = {NULL, 0x0000};	// must be in .data, not .bss


sval
_bios_service(sval request, va_list ap)
{
    sval rcode;
    uval saved_eflags;
    static SLock lock;

    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT;);

    // FIXME: dos restoring eflags do anything other than enabling interrupts,
    // if not get rid of this

    /*
     * Preserve %eflags and disable interrupts.  Interrupts must be disabled
     * before we make the far call to bios, because after the call, the
     * invariant (%ds == %es == (%cs + 8)), which our interrupt handlers
     * depend on, does not hold.
     */
    asm("pushfl; popl %0; cli" : "=g" (saved_eflags) : );

    // NOTE THIS MUST HAPPEN AFTER DISABLE INTERRUPTS
    AutoLock<SLock> al(&lock); // locks now, unlocks on return

    /*
     * We need to call the bios service routine via an lcall (using the far
     * pointer initialized in bios_client_init()), rather than a simple call.
     */

    asm("pushl	%1;"			// second parameter: ap
	"pushl	%2;"			// first parameter: request
	"lcall	_bios_service_ptr;"	// call service routine
					//     (return value in %eax)
	"lea	8(%%esp), %%esp"	// pop arguments from stack
	: "=a" (rcode)
	: "g" (ap), "g" (request)
	: "ecx", "edx", "cc", "memory"
    );

    /*
     * Restore %eflags, possibly re-enabling interrupts.
     */
    asm("pushl %0; popfl" : : "g" (saved_eflags) : "cc");

    return rcode;
}

sval
bios_service(sval request, ...)
{
    va_list ap;
    // FIXME: assume we have somehow logically disabled IPI here

    /*
     * We must switch back to the real kernel address space.  We may be
     * running in a borrowed address space which doesn't have low addresses
     * mapped V=R as expected by bios.
     */
    if (exceptionLocal.currentSegmentTable !=
			    exceptionLocal.kernelSegmentTable) {
	exceptionLocal.kernelSegmentTable->switchToAddressSpace();
    }

    va_start(ap, request);
    sval rc = _bios_service(request, ap);
    va_end(ap);

    /*
     * We may have to restore the address space.
     */
    if (exceptionLocal.currentSegmentTable !=
			    exceptionLocal.kernelSegmentTable) {
	exceptionLocal.currentSegmentTable->switchToAddressSpace();
    }

    return rc;
}

static sval
initial_bios_service(sval request, ...)
{
    va_list ap;
    va_start(ap, request);
    sval rc = _bios_service(request, ap);
    va_end(ap);
    return rc;
}


void
bios_client_init(BiosServiceRoutine bsr)
{
    bios_service_ptr.eip = bsr;
    bios_service_ptr.cs = FIRMWARE_CS;
}

sval
bios_reboot(void)
{
    return bios_service(BIOS_REBOOT);
}

sval
bios_display_setmode(sval mode)
{
    return bios_service(BIOS_DISPLAY_SETMODE, mode);
}

sval
bios_putchar(sval c)
{
    return bios_service(BIOS_PUTCHAR, c);
}

sval
bios_init_putchar(sval c)
{
    return initial_bios_service(BIOS_PUTCHAR, c);
}

sval
bios_getchar(void)
{
    return bios_service(BIOS_GETCHAR);
}

sval
bios_peekchar(void)
{
    return bios_service(BIOS_PEEKCHAR);
}

sval
bios_serial_putchar(sval c)
{
    return bios_service(BIOS_SERIAL_PUTCHAR, c);
}

sval
bios_serial_getchar(void)
{
    return bios_service(BIOS_SERIAL_GETCHAR);
}

sval
bios_memsize(sval mtype)
{
    /*
     * Must use initial_bios_service(), because exceptionLocal isn't
     * accessible before translation is turned on.
     */
    return initial_bios_service(BIOS_MEMSIZE, mtype);
}

sval
bios_diskinfo(sval drive)
{
    return bios_service(BIOS_DISKINFO, drive);
}

sval
bios_diskread(sval drive, sval cylinder, sval track, sval sector,
						sval count, void *buffer)
{
    sval retvalue;
    retvalue = bios_service(BIOS_DISKREAD, drive, cylinder, track, sector,
							    count, buffer);
    return (retvalue);
}

sval
bios_diskwrite(sval drive, sval cylinder, sval track, sval sector,
						sval count, void *buffer)
{
    sval retvalue;
    retvalue = bios_service(BIOS_DISKWRITE, drive, cylinder, track, sector,
							    count, buffer);
    return (retvalue);
}

sval
bios_xlate_on(uval cr3, DTParam *gdtrp)
{
    /*
     * Must use initial_bios_service(), because exceptionLocal isn't
     * accessible before translation is turned on.
     */
    return initial_bios_service(BIOS_XLATE_ON, cr3, gdtrp);
}

