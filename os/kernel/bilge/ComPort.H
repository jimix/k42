#ifndef _K42_COMPORT_H
#define _K42_COMPORT_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ComPort.H,v 1.2 2005/02/11 02:40:39 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: HW Communications port. Serial-port specialization.
 * **************************************************************************/

#include <sys/IOChan.H>

#define BAUDRATE 115200

struct ComPort: public IOChan{
    uval comBase;
    DEFINE_NOOP_NEW(ComPort);
    ComPort(uval base=0):IOChan(0), comBase(base) {};
    virtual uval getDSR() = 0;
    virtual uval getCTS() = 0;
    virtual void putChar(char c)=0;
    virtual uval getChar(char &c)=0;
    virtual void setSpeed(uval val) { return; };
    virtual uval write(const char* buf, uval len, uval block = 1) {
	uval i = 0;
	setDTR(1);
#ifdef HW_FLOWCONTROL
	while (getCTS()==0);
#endif
	while (i<len) {
	    putChar(buf[i]);
	    ++i;
	}
	setDTR(0);
	return len;
    }
    virtual uval read(char* buffer, uval len, uval block = 1) {
#ifdef THINWIRE_TIMEOUT
	uval64 start = getClock();
#endif

	// turn on request to send to accept input
	setRTS(1);
	uval i = 0;
	while (i < len) {
	    char c;
	    if (getChar(c)) {
		buffer[i] = c;
		++i;
		continue;
	    }
	    if (block == 0) break;
#ifdef THINWIRE_TIMEOUT
	    if ((getClock()- start) >= 5 * _BootInfo->timebase_frequency) {
		return 0;
	    }
#endif
	}
	setRTS(0);
	return i;
    }
    virtual void writeHex(uval x) {
	char c;
	for (uval i = 0; i<16; ++i) {
	    c = 0xf & (x>>(4ULL*(15ULL-i)));
	    if (c<10) {
		putChar(c + '0');
	    } else {
		putChar(c - 10 + 'a');
	    }
	}
    }
#define setBIT(name)						\
    virtual void set##name(uval state) { return; }
    setBIT(RTS);
    setBIT(DTR);
    setBIT(TxENABLE);
    setBIT(RxENABLE);

};


#endif /* _K42_COMPORT_H */
