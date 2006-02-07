#ifndef _K42_UARTPORT_H
#define _K42_UARTPORT_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: UARTPort.H,v 1.1 2005/02/09 18:45:42 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: HW Communications port, simulator implementation
 * **************************************************************************/

#include <bilge/ComPort.H>
#include <misc/hardware.H>

struct UARTPort: public ComPort {
    enum{
	LCR= 3,		// Line Control Register
	MCR= 4,		// Modem Control Register
	BD_UB= 1,	// Baudrate Divisor - Upper Byte
	BD_LB= 0,	// Baudrate Divisor - Lower Byte
	LSR= 5,		// Line Status Register
	MSR= 6,		// Modem Status Register
	FCR= 2,

	LCR_BD=	 0x80,	// LCR value to enable baudrate change
	LCR_8N1= 0x03,	// LCR value for 8-bit word, no parity, 1 stop bit
	MCR_DTR= 0x01,	// MCR value for Data-Terminal-Ready
	MCR_RTS= 0x02,	// MCR value for Request-To-Send
	LSR_THRE=0x20,	// LSR value for Transmitter-Holding-Register-Empty
	LSR_DR=  0x01,	// LSR value for Data-Ready
	MSR_DSR= 0x20,	// MSR value for Data-Set-Ready
	MSR_CTS= 0x10	// MSR value for Clear-To-Send

    };


    uval mcr;
    uval msr;
    DEFINE_PRIMITIVE_NEW(UARTPort);
    UARTPort(uval base, uval speed, uval wait);
    virtual uval getDSR();
    virtual uval getCTS();
    virtual void setDTR(uval state);
    virtual void setRTS(uval state);
    virtual void setSpeed(uval val);
    virtual uval getChar(char &c);
    virtual void putChar(char c);
    virtual uval isReadable();
};

#endif /* _K42_UARTPORT_H */
