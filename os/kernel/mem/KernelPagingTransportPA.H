#ifndef __KERNEL_PAGING_TRANSPORT_PA_H_
#define __KERNEL_PAGING_TRANSPORT_PA_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelPagingTransportPA.H,v 1.3 2004/11/02 19:38:47 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: kernel paging transport object, which talks to a
 *                     paging transport in the file system, providing
 *                     flow control (specialized for PAPageServer)
 * **************************************************************************/

#include "KernelPagingTransport.H"
#include <meta/MetaKernelPagingTransportPA.H>

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class KernelPagingTransportPA :
    public KernelPagingTransport __xbase(KernelPagingTransport) {
protected:
    DEFINE_GLOBAL_NEW(KernelPagingTransportPA);
    // FIXME: put proper rights
    DEFINE_ACCESS_SERVER(KernelPagingTransportPA,
			 MetaObj::none, MetaObj::none);
    KernelPagingTransportPA() {}
    ~KernelPagingTransportPA() {}

    virtual SysStatus init(ProcessID pid, ObjectHandle toh, ObjectHandle &kptoh,
			   ObjectHandle &sfroh);

__xpublic:
    static SysStatus _Create(__in __CALLER_PID processID,
			     __in ObjectHandle toh,
			     __out ObjectHandle &kptoh,
			     __out ObjectHandle &sfroh);
 public:
    static void ClassInit(VPNum vp);
};

#endif /* #ifndef __KERNEL_PAGING_TRANSPORT_PA_H__ */

