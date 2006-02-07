#ifndef _K42_HVPORT_H
#define _K42_HVPORT_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HVPort.H,v 1.1 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: HW Communications port, hypervisor implementation
 * **************************************************************************/

#include <bilge/ComPort.H>
#include <HVChan.H>

struct HVPort: public ComPort {
    /* get_term_char returns up to 16 characters */
    /* in case port->getStr does not retrieve them all */
    /* they need to be buffered                  */

    struct HVChannel *chan;
    HVPort(MemoryMgrPrimitive* mem):chan(new(mem) HVChannel(0)) {};


    DEFINE_PRIMITIVE_NEW(HVPort);
    virtual uval getChar(char &c) {
	return getStr(&c,1) > 0;
    }
    virtual void putChar(char c) {
	putStr(&c,1);
    }
    virtual uval putStr(const char* buf, uval length) {
	return chan->write(buf, length, 1);
    }
    virtual uval getStr(char* buf, uval length) {
	uval ret = 0;
	while (ret == 0) {
	    ret = chan->read(buf, length, 1);
	}
	return ret;
    }
};

#endif /* _K42_HVPORT_H */
