/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: thinwireArch.C,v 1.6 2005/07/13 16:40:54 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: PwrPC implementation of machinery for communicating
 * over a multiplexed thin-wire connection
 * **************************************************************************/

#include "kernIncs.H"
#include <sys/KernelInfo.H>
#include <sys/thinwire.H>
#include <scheduler/Scheduler.H>
#include <sync/SLock.H>
#include <misc/hardware.H>
#include <bilge/libksup.H>
#include <bilge/arch/powerpc/openfirm.h>
#include <bilge/arch/powerpc/BootInfo.H>
#include <exception/ExceptionLocal.H>
#include <init/memoryMapKern.H>
#include <init/kernel.H>

#include <sys/ComPort.H>
#include "HVPort.H"
#include "UARTPort.H"
#include "ZilogPort.H"
#include "bilge/arch/powerpc/simos.H"

#include <sys/hcall.h>

void pause()
{
    volatile uval x = (getClock()+(1<<24)&~((1<<24)-1));
    while (getClock()<x);
}

volatile uval64 _base = 0;
void
writeCOM2(char c)
{
    uval32 lsr;

    if (_BootInfo->platform == PLATFORM_POWERMAC) {
	char x;
	do {
	    x = ioInUchar((uval8*)_base);
	} while ( (x & ZilogPort::TX_BUF_EMP)  == 0);

	ioOutUchar((uval8*)_base + ZilogPort::DATA, c);

	do {
	    x = ioInUchar((uval8*)_base);
	} while ( (x & ZilogPort::TX_BUF_EMP)  == 0);

    } else {

	_base = 0x000002F8ULL + 0xffff000000000000ULL;
	ioOutUchar((uval8*)(_base + UARTPort::MCR),   UARTPort::MCR_DTR);

	do {
	    lsr = ioInUchar((uval8*)(_base + UARTPort::LSR));
	} while ((lsr & UARTPort::LSR_THRE) != UARTPort::LSR_THRE);

	ioOutUchar((uval8*)_base, c);
    }
}

void
writeCOM2Hex(uval x)
{
    char c;
    for (uval i = 0; i<16; ++i) {
	c = 0xf & (x>>(4ULL*(15ULL-i)));
	if (c<10) {
	    writeCOM2(c + '0');
	} else {
	    writeCOM2(c - 10 + 'a');
	}
    }
}

void
writeCOM2Str(char* str)
{
    while (*str) {
	writeCOM2(*str);
	++str;
    }
}

const char match[]="\000**thinwire**\000";
void
ThinWireChan::ClassInit(VPNum vp, MemoryMgrPrimitive *memory)
{
    if (vp != 0) return;
    uval lockptr;
    uval comBase = 0;
    uval oldSpeed = 9600;
    uval newSpeed = 115200;
    char *speed  = NULL;;
    ComPort *serial = NULL;

    if (_BootInfo->naca.serialPortAddr) {
	comBase = ioReserveAndMap(_BootInfo->naca.serialPortAddr, 1);
	_base = comBase;
    }

    enabled = 1;
    memory->alloc(lockptr, sizeof (*TWLock), 8);
    TWLock = (SLock *)lockptr;
    TWLock->init();

    for (uval i = 0; i < NUM_CHANNELS; ++i) {
	ThinWireChan::twChannels[i] = new(memory) ThinWireChan(i);
    }

    if (KernelInfo::OnHV()) {
	port = new(memory) HVChannel(0);
	oldSpeed = 0;
	newSpeed = 0;
    } else if (KernelInfo::OnSim()) {
	port = getSimTWChan(memory);
	newSpeed = 0;
	oldSpeed = 0;
    } else if (_BootInfo->platform == PLATFORM_POWERMAC) {
	// Hack to determine whether we are on a G5 where
	// we only support 57600 or an XServe where it can
	// handle 115200
	if (_BootInfo->modelName[0]=='P' &&
	    _BootInfo->modelName[1]=='o' &&
	    _BootInfo->modelName[2]=='w') {
	    newSpeed = 57600;
	} else {
	    newSpeed = 115200;
	}
	/* Not all victims seem to be able to run at 115200, so it's
	 * easiest to just ramp this down for now.
	 */
	newSpeed = 57600;

	oldSpeed = 57600;
	if (_BootInfo->wireInit) {
	    oldSpeed = newSpeed;
	}
	port = serial = new(memory) ZilogPort(comBase, oldSpeed, 1);
    } else {
	newSpeed = 115200;
	oldSpeed = 9600;
	if (_BootInfo->wireInit) {
	    oldSpeed = newSpeed;
	}
	port = serial =new(memory) UARTPort(comBase, oldSpeed, 1);
    }

    if (!_BootInfo->wireInit) {
	_BootInfo->wireInit = 1;

	switch (newSpeed) {
	case 9600:
	    speed = "9600";
	    break;

	case 57600:
	    speed = "57600";
	    break;

	case 115200:
	    speed = "115200";
	    break;

	}
	port->write("Enable thinwire\n\r",17);

	port->write(match,14);
	if (serial != NULL && oldSpeed != newSpeed) {
	    char buf[32];
	    int i = 0;
	    int j = 0;
	    buf[i++] = 'S';
	    buf[i++] = ' ';
	    buf[i++] = ' ';
	    buf[i++] = ' ' + strlen(speed);
	    buf[i++] = ' ';

	    while (speed[j]) {
		buf[i++] = speed[j++];
	    }

	    serial->write(buf, i, 1);
	    serial->read(buf, i, 1);

	    serial->setSpeed(newSpeed);

	    serial->read(buf, i, 1);
	}
    }
}

