#ifndef __HARDWARE_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __HARDWARE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hardware.H,v 1.9 2002/01/04 23:16:16 pdb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: amd64 implementation of hardware specific features
 * **************************************************************************/

typedef uval InterruptState;

static __inline uval
hardwareInterruptsEnabled()
{
    uval data;

    __asm __volatile("pushfq; popq %0" : "=r" (data));
    return (data & 0x200);		// use menmonic pls XXX
}

static __inline void
disableHardwareInterrupts(InterruptState &/*is*/)
{
    tassert(hardwareInterruptsEnabled(),
	    err_printf("interrupts already disabled\n"));
    __asm__ ("cli");
}

static __inline void
enableHardwareInterrupts(InterruptState &/*is*/)
{
    tassert(!hardwareInterruptsEnabled(),
	    err_printf("interrupts already enabled\n"));
    __asm__ ("sti");
}

static __inline void
disableHardwareInterrupts()
{
    tassert(hardwareInterruptsEnabled(),
	    err_printf("interrupts already disabled\n"));
    __asm__ ("cli");
}

static __inline void
enableHardwareInterrupts()
{
    tassert(!hardwareInterruptsEnabled(),
	    err_printf("interrupts already enabled\n"));
    __asm__ ("sti");
}

// for the Intel 8253 Programmable Interval Timer

#define PIT_COUNTER        0x40
#define PIT_CONTROL	   0x43

#define PIT_MODE0_CMD      0x30
#define PIT_MODE3_CMD      0x36
#define PIT_LATCH_CMD      0x0

static __inline SysTime
getClock()
{
  uval32 low, high;
  uval64 val;
  
  __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));

  val = (((uval64)(high)) << 32) + low;
  return val;
}

static __inline uval
getInstrCount()
{
    return((uval)0);		// can do better with Simnow ? XXX
}

/*
 * true if clock can be read in user mode
 */
const uval usermodeClock=1;

static __inline void
setClock(SysTime now)
{
    (void)now;			// ask XXX pdb
}

/* in/out functions for I/O space, brought over from NetBSD */
static __inline uval8
inbc(uval port)
{
    uval8 data;

    __asm __volatile("inb %1,%0" : "=a" (data) : "id" ((uval16)(port)));
    return (data);
}

static __inline void
outbc(uval port, uval8 data)
{
    __asm __volatile("outb %0,%1" : : "a" (data), "id" ((uval16)(port)));
}

static __inline void
outbc_p(uval port, uval8 data)
{
    __asm __volatile("outb %0,%1; outb %%al,$0x80" : : "a" (data), "id" ((uval16)(port)));
}

static __inline void
outbv(uval port, uval8 data)
{
    uval8	al;
    /*
     * Use an unnecessary assignment to help gcc's register allocator.
     * This make a large difference for gcc-1.40 and a tiny difference
     * for gcc-2.6.0.  For gcc-1.40, al had to be ``asm("ax")'' for
     * best results.  gcc-2.6.0 can't handle this.
     */
    al = data;
    __asm __volatile("outb %0,%%dx" : : "a" (al), "d" (port));
}

static __inline uval8
inbv(uval port)
{
    uval8	data;
    /*
     * We use %%dx and not %1 here because i/o is done at %dx and not at
     * %edx, while gcc generates inferior code (movw instead of movl)
     * if we tell it to load (u_short) port.
     */
    __asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
    return (data);
}


#define Kinb(port) __extension__ ({					\
	uval8	_data;							\
	if (__builtin_constant_p(port) && ((port) & 0xffff) < 0x100	\
	    && (port) < 0x10000)					\
		_data = inbc(port);					\
	else								\
		_data = inbv(port);					\
	_data; })

#define Koutb(port, data) (						\
	__builtin_constant_p(port) && ((port) & 0xffff) < 0x100		\
	&& (port) < 0x10000						\
	? outbc(port, data) : outbv(port, data))

static __inline SysTime
setDecrementer(SysTime now)
{
    uval u,l, uo, lo;
    u = now>>8;
    l = now;
    Koutb(PIT_CONTROL, PIT_LATCH_CMD);
    lo = Kinb(PIT_COUNTER);
    uo = Kinb(PIT_COUNTER);
    Koutb(PIT_CONTROL, PIT_MODE0_CMD);
    Koutb(PIT_COUNTER, l);
    Koutb(PIT_COUNTER, u);
    return uo<<8+lo;
}

static __inline SysTime
getDecrementer()
{
    uval uo, lo;
    Koutb(PIT_CONTROL, PIT_LATCH_CMD);
    lo = Kinb(PIT_COUNTER);
    uo = Kinb(PIT_COUNTER);
    return uo<<8+lo;
}

static __inline SysTime
maxDecrementer()
{
    return 0xFFFF;
}

#define _HARDWARE_PIC1_IMR 0x21
#define _HARDWARE_PIC2_IMR 0xA1
#define _HARDWARE_PIC1_OCW 0x20
#define _HARDWARE_PIC2_OCW 0xA0

#ifdef notdef								//	XXX

static __inline void
imr_enable(uval intrno)
{
    uval imr;
    uval mask;
    if(intrno >= 8) {
	imr = Kinb(_HARDWARE_PIC2_IMR);
	mask = 1<<(intrno-8);
	imr &= ~mask;
	Koutb(_HARDWARE_PIC2_IMR, imr);
    } else {
	imr = Kinb(_HARDWARE_PIC1_IMR);
	mask = 1<<intrno;
	imr &= ~mask;
	Koutb(_HARDWARE_PIC1_IMR, imr);
    }
    return;
}

#endif /* #ifdef notdef								//	XXX */

static unsigned int cached_irq_mask = 0xffff;

#define __byte(x,y)     (((unsigned char *)&(y))[x])
#define cached_21       (__byte(0,cached_irq_mask))
#define cached_A1       (__byte(1,cached_irq_mask))

static __inline void
imr_enable(uval intrno)
{
        cached_irq_mask &= ~(1<<intrno);
        if (intrno & 8)
		outbc_p(0xA1, cached_A1);
        else
		outbc_p(0x21, cached_21);
}


static __inline void
imr_disable(uval intrno)
{
    uval imr;
    uval mask;
    if(intrno >= 8) {
	imr = Kinb(_HARDWARE_PIC2_IMR);
	mask = 1<<(intrno-8);
	imr |= mask;
	Koutb(_HARDWARE_PIC2_IMR, imr);
    } else {
	imr = Kinb(_HARDWARE_PIC1_IMR);
	mask = 1<<intrno;
	imr |= mask;
	Koutb(_HARDWARE_PIC1_IMR, imr);
    }
    return;
}

static __inline void
imr_mask(uval mask)
{
    Koutb(_HARDWARE_PIC1_IMR, mask & 0xff);
    Koutb(_HARDWARE_PIC2_IMR, (mask >> 8) & 0xff);
}

static __inline void
interrupt_eoi(uval intrno)
{
    uval ocw;
    if(intrno >=8) {
	/* second 8259 */
	ocw = 0x60 | (intrno-8);
	Koutb(_HARDWARE_PIC2_OCW, ocw);
	intrno = 2;			// cascade level
    }
    ocw = 0x60 | intrno;
    Koutb(_HARDWARE_PIC1_OCW, ocw);
}

#define _HARDWARE_PIC_EOI 0x20

static __inline void
interrupt_eoi_nonspecific()
{
    Koutb(_HARDWARE_PIC1_OCW, _HARDWARE_PIC_EOI);
    Koutb(_HARDWARE_PIC2_OCW, _HARDWARE_PIC_EOI);
}

