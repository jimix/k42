/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NetDev.C,v 1.1 2004/10/26 13:52:23 mostrows Exp $
 *****************************************************************************/
#include "kernIncs.H"
#include "bilge/NetDev.H"
#include "mem/FCMStartup.H"
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaNetDev.H>
#include <mem/RegionDefault.H>
#include <proc/ProcessSetKern.H>

/* virtual */ SysStatus
NetDev::init()
{
    FRKernelPinned::init();
    return 0;
}

void
NetDev::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaNetDev::init();
}


SysStatus
NetDev::_Create(ObjectHandle &frOH, uval io_addr, uval size,
		__CALLER_PID callerPID)
{
    SysStatus rc;
    
    size = PAGE_ROUND_UP(size);

    NetDev *nd = new NetDev;

    if (nd == NULL) {
	return -1;
    }

    nd->init();
    nd->pageSize = PAGE_SIZE;
    nd->size = size;
    nd->ioaddr = io_addr;
    
    rc = nd->giveAccessByServer(frOH, callerPID);
    if (_FAILURE(rc)) {
	goto destroy;
    }

    rc = FCMStartup::Create(nd->fcmRef, io_addr, size);
    if (_FAILURE(rc)) {
	err_printf("allocation of fcm failed\n");
	goto destroy;
    }

    return rc;

destroy:
    nd->destroy();
    return rc;
}

