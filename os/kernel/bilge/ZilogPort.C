/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ZilogPort.C,v 1.5 2005/03/15 00:56:53 cyeoh Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: HW Communications port, simulator implementation
 * **************************************************************************/

#include "kernIncs.H"
#include <sys/types.H>
#include <bilge/libksup.H>
#include <bilge/arch/powerpc/BootInfo.H>
#include "ZilogPort.H"
extern void writeCOM2Hex(uval x);
extern void writeCOM2(char c);

ZilogPort::ZilogPort(uval base, uval speed, uval wait):ComPort(base)
{
    char c;
    unsigned int j;
    struct command {
	uval8 reg;
	uval8 cmd;
    };
    struct command init_sequence[] = {
	{ 13, 0 },			/* set baud rate divisor */
	{ 12, 0 },
	{ 14, BR_ENABLE},
	{ 11, TX_BR_CLOCK | RX_BR_CLOCK},
	{ 5,  TX_ENABLE | TX_8 | DTR | RTS},
	{ 4,  X16CLK | PAR_EVEN | STOP_BIT1},
	{ 3,  RX_ENABLE | RX_8},
	{ 0, 0}
    };
    for (int i=0; i<16; ++i) {
	regs[i]=0;
    }

    // 0xc0 to register 9 --> reset
    ioOutUchar((uval8*)comBase, 9);
    ioOutUchar((uval8*)comBase, 0xC0);

    for (j = 0; init_sequence[j].reg!=0; ++j) {
	regs[init_sequence[j].reg] = init_sequence[j].cmd;
	ioOutUchar((uval8*)comBase, init_sequence[j].reg);
	ioOutUchar((uval8*)comBase, init_sequence[j].cmd);
    }

    if (speed == 115200) {
	setSpeed(115200);
    }

    // Hack to determine whether we are on a G5 where
    // the CTS line is inverted or an XServe where it is not
    bool CTS_inverted = false;
    if (_BootInfo->modelName[0]=='P' &&
	_BootInfo->modelName[1]=='o' &&
	_BootInfo->modelName[2]=='w') {
	CTS_inverted = true;
    }

    if (wait) {
	do {
	    c = ioInUchar((uval8*)comBase);
	} while ( ((c & CTS) == 0) == !CTS_inverted);
    }
}


/* virtual */ uval
ZilogPort::getDSR() {
    char c = ioInUchar((uval8*)comBase);
    return (c & SYNC_HUNT);
}

/* virtual */ void
ZilogPort::setWReg(uval reg, uval8 v) {
    regs[reg] = v;
    ioOutUchar((uval8*)comBase, reg);
    ioOutUchar((uval8*)comBase, v);
}


/* virtual */void
ZilogPort::putChar(char c) {
    char x = 0;
    do {
	x = ioInUchar((uval8*)comBase);
    } while ( (x & TX_BUF_EMP)  == 0);

    ioOutUchar((uval8*)comBase + DATA, c);
}

/* virtual */ uval
ZilogPort::getChar(char &c) {
    char x = ioInUchar((uval8*)comBase);
    if (x & RX_CH_AV) {
	   c = ioInUchar((uval8*)comBase + DATA);
	   return 1;
    }
    return 0;
}


/* virtual */ uval
ZilogPort::isReadable()
{
#if 0
    char x = ioInUchar((uval8*)comBase);
    if (x & RX_CH_AV) {
	return 1;
    }
#endif
    return 0;
}

/* virtual */ void
ZilogPort::setSpeed(uval val)
{
    if (val != 115200) return;

    /* Magic sequence to set line speed to 115200 */
    ioOutUchar((uval8*)comBase, 4);
    ioOutUchar((uval8*)comBase, X32CLK | PAR_EVEN | STOP_BIT1);
    ioOutUchar((uval8*)comBase, 14);
    ioOutUchar((uval8*)comBase, 0);
    ioOutUchar((uval8*)comBase, 11);
    ioOutUchar((uval8*)comBase, 0);
}
