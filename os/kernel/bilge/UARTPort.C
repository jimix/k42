/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: UARTPort.C,v 1.2 2005/02/11 02:40:40 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: HW Communications port, uart 16550 implementation
 * **************************************************************************/

#include "kernIncs.H"
#include <sys/types.H>
#include <bilge/libksup.H>
#include "UARTPort.H"

extern void writeCOM2(char c);

UARTPort::UARTPort(uval base, uval speed, uval wait)
    :ComPort(base), mcr(0), msr(0)
{
    ioOutUchar((uval8*)comBase + MCR,   0);
    /*
     * Wait for CTS.
     */
    while (wait && getCTS()==0);

    /*
     * Set baudrate and other parameters, and raise Data-Terminal-Ready.
     */
    ioOutUchar((uval8*)comBase + LCR,   LCR_BD);

    ioOutUchar((uval8*)comBase + BD_LB, (BAUDRATE/speed) & 0xff);
    ioOutUchar((uval8*)comBase + BD_UB, (BAUDRATE/speed) >> 8);
    ioOutUchar((uval8*)comBase + LCR,   LCR_8N1);
    ioOutUchar((uval8*)comBase + MCR,   MCR_DTR);
    mcr = MCR_DTR;
}

/* virtual */ uval
UARTPort::getDSR()
{
    msr = ioInUchar((uval8*)comBase+MSR);
    return (msr & MSR_DSR)==MSR_DSR;
}

/* virtual */ uval
UARTPort::getCTS()
{
    msr = ioInUchar((uval8*)comBase+MSR);
    return (msr & MSR_CTS)==MSR_CTS;
}

/* virtual */ void
UARTPort::setDTR(uval state)
{
    if (state) {
	mcr |= MCR_DTR;
    } else {
	mcr &= ~MCR_DTR;
    }
    ioOutUchar((uval8*)comBase+MCR, mcr);
}

/* virtual */ void
UARTPort::setRTS(uval state)
{
    if (state) {
	mcr |= MCR_RTS;
    } else {
	mcr &= ~MCR_RTS;
    }
    ioOutUchar((uval8*)comBase+MCR, mcr);
}

/* virtual */ void
UARTPort::setSpeed(uval speed)
{
    /*
     * Set baudrate and other parameters, and raise Data-Terminal-Ready.
     */
    ioOutUchar((uval8*)comBase + MCR,   0);
    ioOutUchar((uval8*)comBase + LCR,   LCR_BD);

    ioOutUchar((uval8*)comBase + BD_LB, (BAUDRATE/speed) & 0xff);
    ioOutUchar((uval8*)comBase + BD_UB, (BAUDRATE/speed) >> 8);
    ioOutUchar((uval8*)comBase + LCR,   LCR_8N1);
    ioOutUchar((uval8*)comBase + MCR,   MCR_DTR);
    mcr = MCR_DTR;
}

/* virtual */ void
UARTPort::putChar(char c)
{
    uval32 lsr;
    do {
	lsr = ioInUchar((uval8*)comBase + LSR);
    } while ((lsr & LSR_THRE) == 0);

    ioOutUchar((uval8*)comBase, c);
}

/* virtual */ uval
UARTPort::getChar(char &c)
{
    uval32 lsr = ioInUchar((uval8*)comBase + LSR);
    if (lsr & LSR_DR) {
	c = ioInUchar((uval8*)comBase);
	return 1;
    }
    return 0;
}

/* virtual */ uval
UARTPort::isReadable()
{
    uval32 lsr = ioInUchar((uval8*)comBase + LSR);
    if (lsr & LSR_DR) {
	return 1;
    }
    return 0;
}
