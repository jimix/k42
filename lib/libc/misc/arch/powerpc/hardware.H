#ifndef __HARDWARE_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __HARDWARE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000,2001,2002,2003,2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hardware.H,v 1.46 2005/02/16 00:05:47 mergen Exp $
 *****************************************************************************/

/* some definitions from linux include/asm-ppc64/processor.h */

/*****************************************************************************
 * Module Description: PwrPC implementation of hardware specific features
 *
 * >>>>>>>>>>>>>>WARNING
 * In PowerPC, the base register in X or D form addresses cannot be R0.
 *      R0 is taken as zero.  (The base in X form is the first of the two)
 *      In asm statements, such registers must use "b" rather than "r"
 *      constraints to avoid accidental use of R0.
 *      We have avoided displacement form use when displacement is 0
 *      because the equivalent X form, with a base register of 0, allows
 *      us to use "r" constraints for the addressing register.  See, for
 *      example, ioInUchar.
 *      (e.g. lwb %0,0(%1)  is replaced by lwbx %0,0,%1)
 * **************************************************************************/


/* Processor Version Register (PVR) field extraction */

#define	PVR_VER(pvr)  (((pvr) >>  16) & 0xFFFF)	/* Version field */
#define	PVR_REV(pvr)  (((pvr) >>   0) & 0xFFFF)	/* Revison field */

/* Processor Version Numbers */
#define	PV_NORTHSTAR	0x0033
#define	PV_PULSAR	0x0034
#define	PV_POWER4	0x0035
#define	PV_ICESTAR	0x0036
#define	PV_SSTAR	0x0037
#define	PV_POWER4p	0x0038
#define PV_970		0x0039
#define PV_970FX	0x003C
#define	PV_POWER5	0x003A
#define	PV_630        	0x0040
#define	PV_630p	        0x0041


#define	SPRN_HID0	0x3F0	/* Hardware Implementation Register 0 */
#define SPRN_HID1	0x3F1	/* Hardware Implementation Register 1 */
#define SPRN_HID4	0x3F4	/* 970 HID4 */
#define SPRN_HID5	0x3F6	/* 970 HID5 */

/*
 * Flags in MSR:
 */
#define PSL_SF		0x8000000000000000
#define PSL_ISF		0x2000000000000000
#define PSL_HV  	0x1000000000000000
#define PSL_POW		0x0000000000040000
#define PSL_ILE		0x0000000000010000
#define PSL_EE		0x0000000000008000
#define PSL_PR		0x0000000000004000
#define PSL_FP		0x0000000000002000
#define PSL_ME		0x0000000000001000
#define PSL_FE0		0x0000000000000800
#define PSL_SE		0x0000000000000400
#define PSL_BE		0x0000000000000200
#define PSL_FE1		0x0000000000000100
#define PSL_IP		0x0000000000000040
#define PSL_IR		0x0000000000000020
#define PSL_DR		0x0000000000000010
#define PSL_RI		0x0000000000000002
#define PSL_LE		0x0000000000000001

#define PSL_SF_SHIFT	63

/*
 * Flags in srr1 after a trap
 */
#define PSL_TRAP_FE     0x0000000000100000  /* IEEE Floating Point Exception */
#define PSL_TRAP_IOP    0x0000000000080000  /* Illegal instruction*/
#define PSL_TRAP_PRV    0x0000000000040000  /* Privileged instruction */
#define PSL_TRAP        0x0000000000020000  /* Trap instruction */
#define PSL_TRAP_NEXT   0x0000000000010000  /* pc is at following instruction */
#define PSL_TRAP_FIELD  (PSL_TRAP_FE|PSL_TRAP_IOP|PSL_TRAP_PRV|PSL_TRAP)

/*
 * Floating-point exception modes:
 */
#define PSL_FE_DIS	0
#define PSL_FE_NONREC	PSL_FE1
#define PSL_FE_REC	PSL_FE0
#define PSL_FE_PREC	(PSL_FE0 | PSL_FE1)
#define PSL_FE_DFLT	PSL_FE_DIS

/*
 * Note that PSL_POW and PSL_ILE are not in the saved copy of the MSR
 */
#define PSL_MBO		0
#define PSL_MBZ		0

#define PSL_KERNELSET   (PSL_SF | PSL_ISF | PSL_HV | PSL_ME | PSL_RI | \
					PSL_DR | PSL_IR | PSL_FP)

#define PSL_USERSET     ((PSL_KERNELSET) | PSL_PR)
#define PSL_USERCHANGE  (PSL_SF | PSL_SE | PSL_BE | PSL_FE0 | PSL_FE1 | PSL_LE)


/*
 * Data Storage Interrupt Status Register (DSISR) bit meanings:
 */
#define DSISR_NOPTE      0x40000000     /* Page table miss and no BAT mapping */
#define DSISR_PROTECTION 0x08000000     /* Protection (key) violation */
#define DSISR_WRITE      0x02000000     /* Store (vs. load) */
#define DSISR_DABR       0x00400000     /* Reserved for DABR */
#define DSISR_NOSEG      0x00200000     /* Segment table miss */

#ifndef __ASSEMBLER__

#include "pwrPC.H"
#include "trap.h"

typedef uval InterruptState;

static __inline uval
hardwareInterruptsEnabled()
{
    uval data;

    __asm __volatile("mfmsr %0" : "=r" (data));
    return (data&PSL_EE);
}

/*
 * For the future, for efficiency, the interface allows for a variable
 * to be passed with the saved state. Currently we don't use this to
 * ensure that we are not depending on a recursive implementation.
 */
static __inline void
disableHardwareInterrupts(InterruptState &/*is*/)
{
    uval tmp;
    tassert(hardwareInterruptsEnabled(),
	    err_printf("interrupts already disabled\n"));
    // FIXME: "andi." won't work now because we have to preserve the high order
    //        bits.  "xori" works only because we know the PSL_EE bit is on.
    // __asm __volatile("mfmsr %0; andi. %0,%0,%1; mtmsrd %0" : "=r" (tmp)
    //		     : "i" ((~(PSL_EE))&0xffff));
    __asm __volatile("mfmsr %0; xori %0,%0,%1; mtmsrd %0" : "=&r" (tmp)
		     : "i" (PSL_EE));
}

static __inline void
enableHardwareInterrupts(InterruptState &/*is*/)
{
    uval tmp;
    tassert(!hardwareInterruptsEnabled(),
	    err_printf("interrupts already enabled\n"));
    __asm __volatile("mfmsr %0; ori %0,%0,%1; mtmsrd %0" : "=&r" (tmp)
		     : "i" (PSL_EE));
}

static __inline void
disableHardwareInterrupts()
{
    uval tmp;
    tassert(hardwareInterruptsEnabled(),
	    err_printf("interrupts already disabled\n"));
    // FIXME: "andi." won't work now because we have to preserve the high order
    //        bits.  "xori" works only because we know the PSL_EE bit is on.
    // __asm __volatile("mfmsr %0; andi. %0,%0,%1; mtmsrd %0" : "=r" (tmp)
    //		     : "i" ((~(PSL_EE))&0xffff));
    __asm __volatile("mfmsr %0; xori %0,%0,%1; mtmsrd %0" : "=&r" (tmp)
		     : "i" (PSL_EE));
}

static __inline void
enableHardwareInterrupts()
{
    uval tmp;
    tassert(!hardwareInterruptsEnabled(),
	    err_printf("interrupts already enabled\n"));
    __asm __volatile("mfmsr %0; ori %0,%0,%1; mtmsrd %0" : "=&r" (tmp)
		     : "i" (PSL_EE));
}

static __inline SysTime
getClock()
{
    uval64 now;
    __asm __volatile(" mftb %0 " : "=r"(now));
    return now;
}

/*
 * true if clock can be read in user mode
 */
const uval usermodeClock=1;

static __inline void
setClock(SysTime now)
{
    uval32 u,l;
    u = now>>32;
    l = now;
    __asm __volatile(" mttbl %0; mttbu  %1; mttbl %2 "
			 : : "r"(0),"r"(u),"r"(l));
}

uval getInstrCount();

static __inline SysTime
setDecrementer(SysTime now)
{
    uval32 dec,olddec;
    dec = now;
    __asm __volatile(" mfdec %0; mtdec %1 " : "=&r"(olddec) : "r"(dec));
    return olddec;
}

static __inline SysTime
maxDecrementer()
{
    return 0x7FFFFFFFULL;
}

/*
 * operations to read and write memory mapped bytes.  Addr is always
 * the real address of the memory mapped byte.
 * assumes that neither the stack nor the code is at the very end of
 * memory.
 *
 * Assume that data bat 0 is available for this use and is alway
 * invalid on entry.  It is left invalid on exit.
 *
 * Operation must be done disabled since the bat is a serial reusable
 * resource.
 */

static __inline void
ioOutUchar(uval8* addr,uval8 val)
{
    __asm __volatile(
	" stbx    %0,0,%1;"
	" eieio ;"
        :
        : "r"(val),"r"(addr));
}

static __inline uval8
ioInUchar(uval8* addr)
{
    uval val;
    __asm __volatile(
	" lbzx   %0,0,%1;"
	" eieio ;"
        : "=r"(val)
        : "r"(addr));
    return val;
}

static __inline void
ioOutUval32(uval32* addr,uval32 val)
{
    __asm __volatile(
	" stwx   %0,0,%1;"
	" eieio ;"
        :
        : "r"(val),"r"(addr));
}

static __inline uval32
ioInUval32(uval32* addr)
{
    uval val;
    __asm __volatile(
	" lwzx  %0,0,%1;"
	" eieio ;"
        : "=r"(val)
        : "r"(addr));
    return val;
}

// byte reversed versions
static __inline void
ioOutUval32BR(uval32* addr,uval32 val)
{
    __asm __volatile(
	" stwbrx %0,0,%1;"
	" eieio ;"
        :
        : "r"(val),"r"(addr));
}

static __inline uval32
ioInUval32BR(uval32* addr)
{
    uval val;
    __asm __volatile(
	" lwbrx  %0,0,%1;"
	" eieio ;"
        : "=r"(val)
        : "r"(addr));
    return val;
}

static __inline void
DCacheStoreLine(uval vaddr, uval offset)
{
    __asm __volatile(" dcbst %0,%1" : : "b"(vaddr), "r"(offset));
}

static __inline void
ICacheInvalidateLine(uval vaddr, uval offset)
{
    __asm __volatile(" icbi %0,%1" : : "b"(vaddr), "r"(offset));
}

static __inline void
CacheSyncRange(uval start, uval end)
{
    uval cacheLineLength = KernelInfo::PCacheLineSize();
    uval vaddr;

    start = start & ~(cacheLineLength-1);
    end = (end + (cacheLineLength-1)) & ~(cacheLineLength-1);
    for (vaddr = start; vaddr < end; vaddr += cacheLineLength) {
	DCacheStoreLine(vaddr, 0);
    }
    __asm(" sync");
    for (vaddr = start; vaddr < end; vaddr += cacheLineLength) {
	ICacheInvalidateLine(vaddr, 0);
    }
    __asm(" isync");
}

static __inline uval
LocalToGlobalAddress(uval vaddr)
{ return vaddr; }

/*
 * eieio is a mini sync
 * it guarantees that all preceeding stores
 * complete before and subsequent stores complete
 * and so is good enough for most lock release sequences
 *
 * For IO memory (cache-inhibited/guarded) stores are always in
 * order, but eieio is needed to order stores and loads
 *
 */
static __inline void
eieio()
{
    __asm("eieio");
}

static __inline uval32
readRealUval32(uval32* addr)
{
    uval tmp1,tmp2;
    __asm __volatile(
	"mfmsr %0;"
	"xori %1,%0,%2;"
	"mtmsrd %1;"
	"isync;"
	"lwzx %1,0,%3;"
	"mtmsrd %0;"
	"isync"
	: "=&r" (tmp1),"=&r" (tmp2)
	: "i" (PSL_DR),"r"(addr));
    return tmp2;
}

#endif /* #ifndef __ASSEMBLER__ */
