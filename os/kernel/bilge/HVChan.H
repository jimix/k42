#ifndef _K42_HVCHAN_H
#define _K42_HVCHAN_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HVChan.H,v 1.1 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: IO Channel, hypervisor implementation
 * **************************************************************************/

#include <bilge/ComPort.H>
#include <sys/hcall.h>
#include <sys/IOChan.H>

struct HVChannel: public IOChan {
protected:
    uval buflen;
    char buf[16];
public:
    DEFINE_PRIMITIVE_NEW(HVChannel);
    HVChannel(uval num): IOChan(num), buflen(0) {};
    virtual uval isReadable();
    virtual uval read(char* buffer, uval length, uval block);
    virtual uval write(const char* buffer, uval length, uval block);
};

#endif /* _K42_HVCHAN_H */
