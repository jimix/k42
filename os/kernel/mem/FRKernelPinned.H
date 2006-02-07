#ifndef __FRKERNEL_PINNED_H_
#define __FRKERNEL_PINNED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRKernelPinned.H,v 1.4 2005/06/07 03:46:38 jk Exp $
 *****************************************************************************/

#include "mem/FRCommon.H"
#include <misc/HashSimple.H>

class FRKernelPinned;
typedef FRKernelPinned **FRKernelPinnedRef;

/**
 * An interface to kernel pinned memory. Currently used for providing a
 * userspace process with a region in which it can place an object to be
 * loaded by the kernel.
 */
class FRKernelPinned : public FRCommon __xbase(FR) {
protected:
    DEFINE_GLOBAL_NEW(FRKernelPinned);
    DEFINE_REFS(FRKernelPinned);
    uval size;

    virtual SysStatus init();

public:
    static void ClassInit(VPNum vp);

    virtual SysStatus putPage(uval physAddr, uval objOffset);

    virtual SysStatus startPutPage(uval physAddr, uval objOffset,
				   IORestartRequests *rr=0);

    virtual SysStatusUval startFillPage(uval physAddr, uval objOffset);

    virtual SysStatus _fsync();

    virtual SysStatus locked_getFCM(FCMRef &r);

__xpublic:
    /**
     * Create a region of kernel pinned memory, and return the virtual
     * address that the kernel will use to reference it.
     *
     * @param frOH the Object handle
     * @param kaddr the kernel's virtual address for this page
     * @param size the size of the region to allocate
     * @param callerPID the calling process' PID
     * @return 0 on success, -1 on failure
     */
    static SysStatus _Create(__out ObjectHandle &frOH,
			     __out uval &kaddr,
			     __in uval size,
			     __CALLER_PID callerPID);

    static SysStatus _InitModule(__in uval initfn);
};

#endif /* #ifndef __FRKERNEL_PINNED_H_ */
