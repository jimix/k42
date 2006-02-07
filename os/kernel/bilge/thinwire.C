/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: thinwire.C,v 1.60 2005/02/09 18:45:42 mostrows Exp $
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



SLock* ThinWireChan::TWLock = NULL;
IOChan* ThinWireChan::port = NULL;
uval ThinWireChan::enabled = 1;
IOChan* ThinWireChan::twChannels[NUM_CHANNELS] = { NULL,};

void
turnOffThinwire()
{
    ThinWireChan::enabled = 0;
}

void
ThinWireChan::EmergWrite(char* buf, sval len)
{
    if (len < 0) len = strlen(buf);

    char header[5];

    header[0] = '#';     //'0'indicates "write"
    header[1] = ' ' + ((len >> 12) & 0x3f);
    header[2] = ' ' + ((len >>  6) & 0x3f);
    header[3] = ' ' + ((len >>  0) & 0x3f);
    header[4] = ' ' + 0;

    port->write(header,5);
    port->write(buf, len);
}

uval
ThinWireChan::write(const char* buf, uval length, uval block)
{
    HWInterruptDisableScope disable; // Disable interrupt for scope of function
    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT );
    AutoLock<SLock> al(TWLock); // locks now, unlocks on return

    if (KernelInfo::OnSim() == SIM_MAMBO && !enabled) {
	return 0;
    }

    sval outlen;
    sval len;
    char header[5];

    header[0] = '0';     //'0'indicates "write"
    header[1] = ' ' + ((length >> 12) & 0x3f);
    header[2] = ' ' + ((length >>  6) & 0x3f);
    header[3] = ' ' + ((length >>  0) & 0x3f);
    header[4] = ' ' + id;

    len = port->write(header,5);
    tassertMsg(len==5, "thinwire partial write: %ld",len);

    outlen = length;
    sval i, inlen;

    while (outlen > 0) {
	if ((i = port->write((char *)buf, outlen)) < 0) {
	    tassert(0, err_printf("thinwire write failed with %ld\n", i));
	    return 0;
	}
	outlen -= i;
	buf    += i;
    }
    memset(header, 0, 5);

    i = 0;
    do {
	uval j = port->read(&header[i], 5 - i, 1);
	i += j;
    } while (i < 5);
    tassert(((uval) (header[4] - ' ')) == id,
	    err_printf("thinwireWrite: expected ack on channel %ld, got %d\n",
		       id, (header[4] - ' ')));
    inlen = ((header[1] - ' ') << 12) |
	((header[2] - ' ') <<  6) |
	((header[3] - ' ') <<  0);
    tassert((uval) inlen == length,
	    err_printf("thinwireWrite:  expected ack length %ld, got %ld\n",
		       length, inlen));
    return 0;
}


uval
ThinWireChan::read(char* buf, uval length, uval block)
{
    HWInterruptDisableScope disable; // Disable interrupt for scope of function
    tassertSilent(!hardwareInterruptsEnabled(), BREAKPOINT;);
    AutoLock<SLock> al(TWLock); // locks now, unlocks on return

    if (KernelInfo::OnSim() == SIM_MAMBO && !enabled) {
	return 0;
    }

    sval i,inlen,len;
    uval8 header[5];
    header[0] = 'A'; // 'A' indicates "read"
    header[1] = ' ' + ((length >> 12) & 0x3f);
    header[2] = ' ' + ((length >>  6) & 0x3f);
    header[3] = ' ' + ((length >>  0) & 0x3f);
    header[4] = ' ' + id;

    len = port->write((char *)header,5);
    tassertMsg(len==5, "thinwire partial write: %ld",len);

    i = 0;

    do {
	i += port->read((char *)&header[i], 5 - i);
    } while (i < 5);

    tassert(((uval)(header[0]-'0'))== id,
	    err_printf("thinwireRead:  expected channel %ld, got %d\n",
		       id, (header[0]-'0')));

    inlen = ((header[1] - ' ') << 12) |
	((header[2] - ' ') <<  6) |
	((header[3] - ' ') <<  0);
    tassert((uval)inlen <= length,
	    err_printf("thinwireRead:  expected at most %ld bytes, got %ld\n",
		       length, inlen));
    length = inlen;
    while (length > 0) {
	if ((i = port->read((char *)buf, length)) < 0) {
	    tassert(0, err_printf("thinwire read failed with %ld\n", i));
	}
	length -= i;
	buf    += i;
    }

    return inlen;
}


sval32
ThinWireChan::thinwireSelect()
{
    tassertSilent(!hardwareInterruptsEnabled(), BREAKPOINT;);
    AutoLock<SLock> al(TWLock); // locks now, unlocks on return

    if (KernelInfo::OnSim() == SIM_MAMBO && !enabled) {
	return -1;
    }

    uval length = 0;
    uval32 avail;

    sval i,inlen,len;
    uval8 buffer[9];
    // '!' indicates "select"
    buffer[0] = '!';
    buffer[1] = ' ' + ((length >> 12) & 0x3f);
    buffer[2] = ' ' + ((length >>  6) & 0x3f);
    buffer[3] = ' ' + ((length >>  0) & 0x3f);
    buffer[4] = ' ' + _BootInfo->wireChanOffset;
    len = port->write((char *)buffer,5);
    tassertMsg(len==5, "thinwire partial write: %ld",len);

    i = 0;
    do {
	i += port->read((char *)&buffer[i], 9 - i);
    } while (i<9);

    inlen = ((buffer[1] - ' ') << 12) |
	((buffer[2] - ' ') <<  6) |
	((buffer[3] - ' ') <<  0);
    tassert((uval)inlen == 4,
	    err_printf("thinwireSelect:  expected 4 bytes, got %ld\n",
		       inlen));
    avail = (buffer[5]<<24)+(buffer[6]<<16)+(buffer[7]<<8)+buffer[8];
    return avail;
}

void
ThinWireChan::thinwireExit()
{
    uval8 buffer[9];

    tassertSilent(!hardwareInterruptsEnabled(), BREAKPOINT);
    AutoLock<SLock> al(TWLock); // locks now, unlocks on return

    buffer[0] = '$'; 	// '$' indicates "exit"
    buffer[1] = ' ';	// length is 0
    buffer[2] = ' ';
    buffer[3] = ' ';
    buffer[4] = ' ';
    port->write((char *)buffer,5);


    uval64 start = getClock();

    // Do a 5 second delay
    // In the case of a fast-reboot this gives us a chance for
    // thinwire to go away , so we don't pick up a high DSR bit.
    // This enables us to correctly wait for a new thinwire to appear.
    while ((getClock()- start) < 5 * _BootInfo->timebase_frequency);

    _BootInfo->wireInit = 0;
}
