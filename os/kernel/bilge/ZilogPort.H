#ifndef _K42_ZILOGPORT_H
#define _K42_ZILOGPORT_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ZilogPort.H,v 1.2 2005/03/03 22:54:27 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: HW Communications port, simulator implementation
 * **************************************************************************/


#include <bilge/ComPort.H>
#include <misc/hardware.H>

struct ZilogPort:public ComPort{

    uval8 regs[16];
    DEFINE_PRIMITIVE_NEW(ZilogPort);
    ZilogPort(uval base, uval speed, uval wait);
    enum {
	CONTROL=	0,
	DATA=		0x10,
	TX_BUF_EMP=	0x4,	/* empty buffer */
	SYNC_HUNT=	0x10,	/* Sync/hunt == DSR */
	CTS=		0x20,

	// Write to register 5:
	RTS=		0x2,	/* RTS */
	TX_ENABLE=      0x8,	/* tx Enable */
	SND_BRK=	0x10,	/* Send Break */
	TX_8=		0x60,	/* 8 bits/char */
	DTR=		0x80,	/* DTR */

	// Write to register 3:
	RX_ENABLE=	0x1,
	RX_8=		0xc0,	/* rx 8 bit/char */

	// Write to register 4:
	PAR_EVEN=	0x2,	/* parity even/odd */
	STOP_BIT1=	0x4,	/* 1 stop bit/char */
	X16CLK=		0x40,	/* x16 clk */
	X32CLK=		0x80,	/* x16 clk */

	// Write to register 11:
	TX_BR_CLOCK=	0x10,	/* tx clock = br generator */
	RX_BR_CLOCK=	0x40,	/* rx clock = br generator */

	// Write to register 14:
	BR_ENABLE=	1,	/* enable br generator */

	// Read from reg 0:
	RX_CH_AV=	0x1	/* rx char Available */
    };


    virtual void setWReg(uval reg, uval8 v);
    virtual uval getDSR();
    virtual void putChar(char c);
    virtual uval getChar(char &c);

    virtual uval8 getWReg(uval reg) {
	return regs[reg];
    }

    virtual void setSpeed(uval val);

#undef setBIT
#define setBIT(name,val, reg)						\
    virtual void set##name(uval state) {				\
	return;								\
	uval v = val;							\
	uval r = getWReg(reg);						\
	if (state) { r |= v; } else { r &= ~v; }			\
	setWReg(reg,r);							\
    }

    setBIT(RTS,RTS,5);
    setBIT(DTR,DTR,5);
    setBIT(TxENABLE,TX_ENABLE,5);
    setBIT(RxENABLE,RX_ENABLE,3);

    virtual uval getCTS() {
	char c = ioInUchar((uval8*) comBase);
	return (c & CTS) != 0;
    }
    virtual uval isReadable();
};


#endif /* _K42_ZILOGPORT_H */
