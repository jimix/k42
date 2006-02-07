/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: thinwire.C,v 1.15 2001/12/29 18:43:52 peterson Exp $
 *****************************************************************************/

/* Thinwire protocol has changed -- this code is not up to date */

// just a placholder x86 like now XXX
//  changes by jimix are lost (locking changes thinwire.C,v 1.3 2001/06/26 15:03:54 jimix Exp $
// XXX probably not good XXX


/*****************************************************************************
 * Module Description: x86 implementation of machinery for communicating
 * over a multiplexed thin-wire connection
 * **************************************************************************/

#include "kernIncs.H"
#include "libksup.H"
#include <misc/arch/amd64/bios.H>
#include <misc/hardware.H>
#include "vps.H"
#include <sys/thinwire.H>
#include <sync/SLock.H>

static uval vpsThinwireFound = 0;

#define COM1 0x3f8
#define COM2 0x2f8

#define LCR 3		// Line Control Register
#define MCR 4		// Modem Control Register
#define BD_UB 1		// Baudrate Divisor - Upper Byte
#define BD_LB 0		// Baudrate Divisor - Lower Byte
#define LSR 5		// Line Status Register
#define MSR 6		// Modem Status Register

// #define BAUDRATE 9600
#define BAUDRATE 115200

#define LCR_BD 0x80	// LCR value to enable baudrate change
#define LCR_8N1 0x03	// LCR value for 8-bit word, no parity, 1 stop bit
#define MCR_DTR 0x01	// MCR value for Data-Terminal-Ready
#define MCR_RTS 0x02	// MCR value for Request-To-Send
#define LSR_THRE 0x20	// LSR value for Transmitter-Holding-Register-Empty
#define LSR_DR 0x01	// LSR value for Data-Ready
#define MSR_DSR 0x20	// MSR value for Data-Set-Ready
#define MSR_CTS 0x10	// MSR value for Clear-To-Send

extern "C"  void early_printk(const char *fmt, ...);

#define comIn(r, v) \
   asm volatile ("xorl %%eax,%%eax; inb %%dx,%%al" : "=a" (v) : "d" (r))
#define comOut(r, v) \
   asm volatile ("outb %%al,%%dx" : : "a" (v), "d" (r))

static uval comBase = 0;

void
comInit(void)
{
    uval32 msr;

#ifndef CONFIG_SIMICS
    /*
     * Wait for Data-Set-Ready on either COM1 or COM2.
     */
    for (; ;) {
	comIn(COM1 + MSR, msr);
	if ((msr & MSR_DSR) == MSR_DSR) {
	    comBase = COM1;
	    early_printk("using com1 for thinwire connection\n");
	    break;
	}
	else
	    early_printk("com1 msr %x \n", msr);
	comIn(COM2 + MSR, msr);
	if ((msr & MSR_DSR) == MSR_DSR) {
	    comBase = COM2;
	    early_printk("using com2 for thinwire connection\n");
	    break;
	}
	else
	    early_printk("com2 msr %x \n", msr);
    }
#else /* #ifndef CONFIG_SIMICS */
    comBase = COM1;
#endif /* #ifndef CONFIG_SIMICS */


    /*
     * Set baudrate and other parameters, and raise Data-Terminal-Ready.
     */
    comOut(comBase + LCR,   LCR_BD);
    comOut(comBase + BD_LB, (115200 / BAUDRATE) & 0xff);
    comOut(comBase + BD_UB, (115200 / BAUDRATE) >> 8);
    comOut(comBase + LCR,   LCR_8N1);
    comOut(comBase + MCR,   MCR_DTR);
#ifdef CONFIG_SIMICS
    comIn(COM1 + MSR, msr);
    if ((msr & MSR_DSR) == MSR_DSR) {
        comBase = COM1;
        early_printk("using com1 for thinwire connection, msr %x\n", msr);
    }
    else
        early_printk("using com1 for thinwire connection despite lack of DSRx\n");
#endif /* #ifdef CONFIG_SIMICS */

}

static uval skipped;

void
comPutChar(char c)
{
    uval32 lsr;
    uval maxcount = 1000;
    uval max_test = 4;

    uval32 msr;
    do {
	max_test--;
	comIn(comBase + MSR, msr);
    } while ((msr & MSR_CTS) != MSR_CTS && max_test);
    do {
	maxcount--;
	comIn(comBase + LSR, lsr);
    } while ((lsr & LSR_THRE) != LSR_THRE && maxcount );
    if(maxcount)  {
    	comOut(comBase, c);
    }
    else {
	skipped++;
        early_printk("skipped one, total %d\n", skipped);
    }
}

char
comGetChar(void)
{
    uval32 lsr;
    char c;

    do {
	comIn(comBase + LSR, lsr);
    } while ((lsr & LSR_DR) != LSR_DR);
    comIn(comBase, c);
    return(c);
}

uval
thinwireTest()
{
    uval id;
    asm("inl %%dx, %%eax" : "=a" (id) : "d" (VPS_THINWIRE_ID_PORT));
    return(id == VPS_MAGIC);
}

static SLock twlock;

void
thinwireInit(MemoryMgrPrimitive *memory)
{
    twlock.init();

    if (thinwireTest()) {
	vpsThinwireFound = 1;
    } else {
	comInit();
	vpsThinwireFound = 0;
    }
}

void
thinwireWrite(uval channel, const char *buf, uval length)
{
    uval inchan, inlen, i;
    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT );
    AutoLock<SLock> al(&twlock); // locks now, unlocks on return

    if (vpsThinwireFound) {
    while(1)
	early_printk(" no thinwire available\n");
#ifndef CONFIG_THINWIRE  // XXX
	asm("addb	$'0, %%al;"	// send ('0' + channel),
	    "outb	%%al, %%dx;"	//     '0' indicates "write"
	    "movl	%%ecx, %%eax;"	// send high-order 6 length bits,
	    "shrl	$12, %%eax;"	//     encoded as a printable character
	    "andl	$0x3f, %%eax;"
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx;"
	    "movl	%%ecx, %%eax;"	// send middle 6 length bits,
	    "shrl	$6, %%eax;"	//     encoded as a printable character
	    "andl	$0x3f, %%eax;"
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx;"
	    "movl	%%ecx, %%eax;"	// send low-order 6 length bits,
	    "andl	$0x3f, %%eax;"	//     encoded as a printable character
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx;"
	    "cld; rep; outsb"		// send data
	    :
	    : "a" (channel),
	      "c" (length),
	      "S" (buf),
	      "d" (VPS_THINWIRE_IO_PORT)
	    : "cc", "eax", "ecx", "esi");
	for (;;) {			// read until we get a channel number
	    asm("xorl %%eax, %%eax;"
		"inb %%dx, %%al;"
		"subb $'A, %%al"	// convert from printable character
		: "=a" (inchan)
		: "d" (VPS_THINWIRE_IO_PORT));
	    if (inchan < NUM_CHANNELS) break;
	}
	tassert(inchan == channel,
		err_printf("thinwireWrite: expected ack on chan %ld, got %ld\n",
							    channel, inchan));
	asm("xorl	%%ecx, %%ecx;"	// clear length register
	    "inb	%%dx, %%al;"	// read high-order 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl;"
	    "shll	$6, %%ecx;"
	    "inb	%%dx, %%al;"	// read middle 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl;"
	    "shll	$6, %%ecx;"
	    "inb	%%dx, %%al;"	// read low-order 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl"
	    : "=c" (inlen)
	    : "d" (VPS_THINWIRE_IO_PORT)
	    : "cc", "eax", "ecx");
	tassert(inlen == length,
		err_printf("thinwireWrite: expected ack length %ld, got %ld\n",
							    length, inlen));
#endif /* #ifndef CONFIG_THINWIRE  // XXX */
    } else {
#ifndef CONFIG_SIMICS  // CONFIG_THINWIRE XXX
//	comOut(comBase + MCR, MCR_DTR|MCR_RTS);	// Raise Request-To-Send 		// from Terminal RTS but does not work ?? pdb
	comPutChar('0' + channel);	// '0' indicates "write"
	comPutChar(' ' + ((length >> 12) & 0x3f));
	comPutChar(' ' + ((length >>  6) & 0x3f));
	comPutChar(' ' + ((length >>  0) & 0x3f));
#endif /* #ifndef CONFIG_SIMICS  // ... */
	for (i = 0; i < length; i++) {
	    comPutChar(buf[i]);
	    if(buf[i] == '\n') {
	      comPutChar('\r');
	    }
	}
//	comOut(comBase + MCR, MCR_DTR);		// Clear Request-To-Send
#ifndef CONFIG_SIMICS  // CONFIG_THINWIRE  XXX
//	comOut(comBase + MCR, MCR_DTR|MCR_RTS);	// Raise Request-To-Send
	inchan = comGetChar() - 'A';
	tassert(inchan == channel,
		err_printf("thinwireWrite: expected ack on chan %ld, got %ld\n",
							    channel, inchan));
	inlen = ((comGetChar() - ' ') << 12) |
		((comGetChar() - ' ') <<  6) |
		((comGetChar() - ' ') <<  0);
	tassert(inlen == length,
		err_printf("thinwireWrite: expected ack length %ld, got %ld\n",
							    length, inlen));
//	comOut(comBase + MCR, MCR_DTR);		// Clear Request-To-Send
#endif /* #ifndef CONFIG_SIMICS  // ... */
    }
}

uval
thinwireRead(uval channel, char *buf, uval length)
{
    uval inchan;
    uval inlen;

    tassertSilent(!hardwareInterruptsEnabled(), BREAKPOINT;);

    AutoLock<SLock> al(&twlock); // locks now, unlocks on return

    if (vpsThinwireFound) {
#ifndef CONFIG_THINWIRE  // XXX
    while(1)
	early_printk(" no thinwire available\n");
	asm("addb	$'A, %%al;"	// send ('A' + channel),
	    "outb	%%al, %%dx;"	//     'A' indicates "read"
	    "movl	%%ecx, %%eax;"	// send high-order 6 length bits,
	    "shrl	$12, %%eax;"	//     encoded as a printable character
	    "andl	$0x3f, %%eax;"
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx;"
	    "movl	%%ecx, %%eax;"	// send middle 6 length bits,
	    "shrl	$6, %%eax;"	//     encoded as a printable character
	    "andl	$0x3f, %%eax;"
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx;"
	    "movl	%%ecx, %%eax;"	// send low-order 6 length bits,
	    "andl	$0x3f, %%eax;"	//     encoded as a printable character
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx"
	    :
	    : "a" (channel),
	      "c" (length),
	      "d" (VPS_THINWIRE_IO_PORT)
	    : "cc", "eax", "ecx");
	for (;;) {			// read until we get a channel number
	    asm("xorl %%eax, %%eax;"
		"inb %%dx, %%al;"
		"subb $'0, %%al"	// convert from printable character
		: "=a" (inchan)
		: "d" (VPS_THINWIRE_IO_PORT));
	    if (inchan < NUM_CHANNELS) break;
	    /*
	     * The VisualProbe simulator checks for external events (screen
	     * refresh, mouse click, etc.) every thousand simulated
	     * instructions.  The inb instruction above probably took a
	     * l-o-n-g time, so we execute a thousand dummy instructions now
	     * to keep the simulator responsive.
	     */
	    asm("1: loop 1b" : : "c" (1000) : "ecx");
	}
	tassert(inchan == channel,
		err_printf("thinwireRead: expected channel %ld, got %ld\n",
							    channel, inchan));
	asm("xorl	%%ecx, %%ecx;"	// clear length register
	    "inb	%%dx, %%al;"	// read high-order 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl;"
	    "shll	$6, %%ecx;"
	    "inb	%%dx, %%al;"	// read middle 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl;"
	    "shll	$6, %%ecx;"
	    "inb	%%dx, %%al;"	// read low-order 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl"
	    : "=c" (inlen)
	    : "d" (VPS_THINWIRE_IO_PORT)
	    : "cc", "eax", "ecx");
	tassert(inlen <= length,
		err_printf("thinwireRead: expected at most %ld bytes, "
								"got %ld\n",
							    length, inlen));
	asm("cld; rep; insb"		// read data
	    :
	    : "c" (inlen),
	      "D" (buf),
	      "d" (VPS_THINWIRE_IO_PORT)
	    : "memory", "cc", "ecx", "edi");
#endif /* #ifndef CONFIG_THINWIRE  // XXX */
    } else {
#ifndef CONFIG_THINWIRE  // XXX
	uval i;
	comPutChar('A' + channel);	// 'A' indicates "read"
	comPutChar(' ' + ((length >> 12) & 0x3f));
	comPutChar(' ' + ((length >>  6) & 0x3f));
	comPutChar(' ' + ((length >>  0) & 0x3f));
	comOut(comBase + MCR, MCR_DTR|MCR_RTS);	// Raise Request-To-Send
	inchan = comGetChar() - '0';
	tassert(inchan == channel,
		err_printf("thinwireRead: expected channel %ld, got %ld\n",
							    channel, inchan));
	inlen = ((comGetChar() - ' ') << 12) |
		((comGetChar() - ' ') <<  6) |
		((comGetChar() - ' ') <<  0);
	tassert(inlen <= length,
		err_printf("thinwireRead: expected at most %ld, got %ld\n",
							    length, inlen));
	for (i = 0; i < inlen; i++) {
	    buf[i] = comGetChar();
	}
	comOut(comBase + MCR, MCR_DTR);		// Clear Request-To-Send
#else /* #ifndef CONFIG_THINWIRE  // XXX */
	early_printk(" no reading form the xterm console \n");
#endif /* #ifndef CONFIG_THINWIRE  // XXX */
    }

    return(inlen);
}


uval32
thinwireSelect()
{
  /* for now just return no.  thinwire is not set up for amd port */
  return(0);
    uval channel=0;
    uval length=0;
    uval32 avail;
    char buffer[8];
    char *buf = &buffer[0];
    uval inchan, inlen, i;
    tassertSilent(!hardwareInterruptsEnabled(), BREAKPOINT;);

    AutoLock<SLock> al(&twlock); // locks now, unlocks on return

    if (vpsThinwireFound) {
#ifndef CONFIG_THINWIRE  // XXX
    while(1)
	early_printk(" no thinwire available\n");
	asm("addb	$'!, %%al;"	// send ('A' + channel),
	    "outb	%%al, %%dx;"	//     'A' indicates "select"
	    "movl	%%ecx, %%eax;"	// send high-order 6 length bits,
	    "shrl	$12, %%eax;"	//     encoded as a printable character
	    "andl	$0x3f, %%eax;"
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx;"
	    "movl	%%ecx, %%eax;"	// send middle 6 length bits,
	    "shrl	$6, %%eax;"	//     encoded as a printable character
	    "andl	$0x3f, %%eax;"
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx;"
	    "movl	%%ecx, %%eax;"	// send low-order 6 length bits,
	    "andl	$0x3f, %%eax;"	//     encoded as a printable character
	    "addb	$' , %%al;"
	    "outb	%%al, %%dx"
	    :
	    : "a" (channel),
	      "c" (length),
	      "d" (VPS_THINWIRE_IO_PORT)
	    : "cc", "eax", "ecx");
	for (;;) {			// read until we get a channel number
	    asm("xorl %%eax, %%eax;"
		"inb %%dx, %%al;"
		"subb $'!, %%al"	// convert from printable character
		: "=a" (inchan)
		: "d" (VPS_THINWIRE_IO_PORT));
	    if (inchan < NUM_CHANNELS) break;
	    /*
	     * The VisualProbe simulator checks for external events (screen
	     * refresh, mouse click, etc.) every thousand simulated
	     * instructions.  The inb instruction above probably took a
	     * l-o-n-g time, so we execute a thousand dummy instructions now
	     * to keep the simulator responsive.
	     */
	    asm("1: loop 1b" : : "c" (1000) : "ecx");
	}
	tassert(inchan == 0,
		err_printf("thinwireSelect: expected no channel %ld, got %ld\n",
							    channel, inchan));
	asm("xorl	%%ecx, %%ecx;"	// clear length register
	    "inb	%%dx, %%al;"	// read high-order 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl;"
	    "shll	$6, %%ecx;"
	    "inb	%%dx, %%al;"	// read middle 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl;"
	    "shll	$6, %%ecx;"
	    "inb	%%dx, %%al;"	// read low-order 6 length bits
	    "subb	$' , %%al;"	//     convert from printable character
	    "orb	%%al, %%cl"
	    : "=c" (inlen)
	    : "d" (VPS_THINWIRE_IO_PORT)
	    : "cc", "eax", "ecx");
	tassert(inlen == 4,
		err_printf("thinwireRead: expected 4 bytes, got %ld\n",
			   inlen));
	asm("cld; rep; insb"		// read data
	    :
	    : "c" (inlen),
	      "D" (buf),
	      "d" (VPS_THINWIRE_IO_PORT)
	    : "memory", "cc", "ecx", "edi");
#endif /* #ifndef CONFIG_THINWIRE  // XXX */
    } else {
	comPutChar('!' + channel);	// 'A' indicates "read"
	comPutChar(' ' + ((length >> 12) & 0x3f));
	comPutChar(' ' + ((length >>  6) & 0x3f));
	comPutChar(' ' + ((length >>  0) & 0x3f));
	comOut(comBase + MCR, MCR_DTR|MCR_RTS);	// Raise Request-To-Send
	inchan = comGetChar() - '!';
	tassert(inchan == 0,
		err_printf("thinwireSelect: expected channel %ld, got %ld\n",
			   channel, inchan));
	inlen = ((comGetChar() - ' ') << 12) |
		((comGetChar() - ' ') <<  6) |
		((comGetChar() - ' ') <<  0);
	tassert(inlen <= 4,
		err_printf("thinwireSelect: expected 4 bytes, got %ld\n",
			   inlen));
	for (i = 0; i < inlen; i++) {
	    buf[i] = comGetChar();
	}
	comOut(comBase + MCR, MCR_DTR);		// Clear Request-To-Send
    }

    avail = (buffer[0]<<24)+(buffer[1]<<16)+(buffer[2]<<8)+buffer[3];
#if 0
    err_printf("value received is %lx <%lx,%lx,%lx,%lx>\n", avail,
	       buffer[0], buffer[1], buffer[2], buffer[3] );
#endif /* #if 0 */
    return(avail);
}
