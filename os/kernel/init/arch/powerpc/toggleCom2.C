/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: toggleCom2.C,v 1.2 2001/04/20 14:51:28 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Turn off the COM2 port during early hardware bring-up.  The dropping of
 * the line is visible, as minicom (displaying the hardware support console)
 * shows that the terminal has gone "offline."  Sometimes this is the only
 * debugging information available early in the boot.
 *
 * This code is mostly copied from thinwire.C
 * **************************************************************************/

#include "kernIncs.H"
#include <scheduler/Scheduler.H>
#include <bilge/arch/powerpc/BootInfo.H>

//FIXME - need to confiture uargs correctly
//        this is the io v maps r address
static uval isaIOBase;

#define COM1 0x3f8
#define COM2 0x2f8

#define LCR 3		// Line Control Register
#define MCR 4		// Modem Control Register
#define BD_UB 1		// Baudrate Divisor - Upper Byte
#define BD_LB 0		// Baudrate Divisor - Lower Byte
#define LSR 5		// Line Status Register
#define MSR 6		// Modem Status Register

//FIXME - aix can't talk faster than 38400, should be 115200
#define BAUDRATE 115200
//#define BAUDRATE 38400

#define LCR_BD 0x80	// LCR value to enable baudrate change
#define LCR_8N1 0x03	// LCR value for 8-bit word, no parity, 1 stop bit
#define MCR_DTR 0x01	// MCR value for Data-Terminal-Ready
#define MCR_RTS 0x02	// MCR value for Request-To-Send
#define LSR_THRE 0x20	// LSR value for Transmitter-Holding-Register-Empty
#define LSR_DR 0x01	// LSR value for Data-Ready
#define MSR_DSR 0x20	// MSR value for Data-Set-Ready
#define MSR_CTS 0x10	// MSR value for Clear-To-Send

#define comIn(r, v) \
   v = ioInUchar((uval8*)(isaIOBase+r));
#define comOut(r, v) \
   ioOutUchar((uval8*)(isaIOBase+r),v);

void
com2TurnOff ()
{
    isaIOBase = 0xffffffff00000000ULL + _BootInfo->isaIOBase;
    comOut(COM2 + MCR, 0);
    asm volatile ("eieio");
}














