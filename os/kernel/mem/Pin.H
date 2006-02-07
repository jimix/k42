#ifndef __PIN_H_
#define __PIN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Pin.H,v 1.4 2005/01/10 15:28:15 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Utilities to handle pinning of pages for IO
 * **************************************************************************/

#include <mem/Region.H>
#include <mem/FCM.H>
#include <sys/uio.h>

struct PinnedMapping {
    RegionRef rr;
    FCMRef fcm;
    uval fcmOffset;
    void *addr;
    DEFINE_GLOBAL_NEW(PinnedMapping);
    PinnedMapping():rr(NULL), fcm(NULL), fcmOffset(0), addr(NULL) {};
    SysStatus unpin() {
	tassertMsg(addr != NULL, "?\n");
	if (addr) {
	    addr = NULL;
	    return DREF(fcm)->releasePage(fcmOffset);
	}
	return 0;
    }
    enum { FullPagesOnly = 1 };  //Flag to indicate that all user data
				 //must represent full pages only



    // Takes "len" bytes at address "addr" in process "pref" and fills
    // in iovec "vec" to refer to pinned memory for said address
    // range. Length of vec and pages arrays must be 2+len/PAGE_SIZE
    // pages array must be 0 filled
    static SysStatusUval pinToIOVec(ProcessRef pref, struct iovec* in,
				    uval veclen, uval writeable,
				    PinnedMapping *pages, struct iovec* vec,
				    uval flags = 0);

    SysStatusUval pinAddr(ProcessRef pref, uval inAddr, uval &outAddr,
			  uval writeable, uval flags);


};


#endif /* #ifndef __PIN_H_ */
