#ifndef __NET_DEV_H_
#define __NET_DEV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NetDev.H,v 1.1 2004/10/26 13:52:23 mostrows Exp $
 *****************************************************************************/

#include "mem/FRKernelPinned.H"
#include <misc/HashSimple.H>

class NetDev;
typedef NetDev **NetDevRef;

/**
 * An interface to kernel pinned memory. Currently used for providing a
 * userspace process with a region in which it can place an object to be
 * loaded by the kernel.
 */
class NetDev : public FRKernelPinned __xbase(FR) {
protected:
    DEFINE_GLOBAL_NEW(NetDev);
    DEFINE_REFS(NetDev);
    uval ioaddr;
    uval size;
    virtual SysStatus init();

public:
    static void ClassInit(VPNum vp);

__xpublic:
    /**
     * Create a region of kernel pinned memory, and return the virtual
     * address that the kernel will use to reference it.
     *
     * @param frOH the Object handle
     * @param ioaddr phys io addr to map
     * @param size the size of the region to allocate
     * @param callerPID the calling process' PID
     * @return 0 on success, -1 on failure
     */
    static SysStatus _Create(__out ObjectHandle &frOH,
			     __in uval io_addr,
			     __in uval size,
			     __CALLER_PID callerPID);
};

#endif /* #ifndef __NET_DEV_H_ */
