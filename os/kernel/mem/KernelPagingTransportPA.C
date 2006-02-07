/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelPagingTransportPA.C,v 1.4 2005/05/20 19:45:19 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: kernel paging transport object, which talks to a
 *                     paging transport in the file system, providing
 *                     flow control (specialized for PAPageServer)
 * **************************************************************************/

#include "kernIncs.H"
#include "KernelPagingTransportPA.H"
#include "StubFileHolder.H"
#include <stub/StubPAPageServer.H>
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaKernelPagingTransportPA.H>

/* static */ void
KernelPagingTransportPA::ClassInit(VPNum vp)
{
    if (vp != 0) return;
    MetaKernelPagingTransportPA::init();
}

/* virtual */ SysStatus
KernelPagingTransportPA::init(ProcessID pid, ObjectHandle toh,
			      ObjectHandle &kptoh, ObjectHandle &sfroh)
{
    stubTrans = new StubFileHolderImp<StubPAPageServer>(toh);
    giveAccessByServer(kptoh, pid, MetaKernelPagingTransportPA::typeID());
    return SharedBufferProducer::init(pid, sfroh);
}

/* virtual */ SysStatus
KernelPagingTransportPA::_Create(__in __CALLER_PID processID,
				 __in ObjectHandle toh,
				 __out ObjectHandle &kptoh,
				 __out ObjectHandle &sfroh)
{
    KernelPagingTransportPA *obj = new KernelPagingTransportPA();

    KernelPagingTransportRef ref;
    ref = (KernelPagingTransportRef)CObjRootSingleRep::Create(obj);

    SysStatus rc = DREF(ref)->init(processID, toh, kptoh, sfroh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    return 0;
}
