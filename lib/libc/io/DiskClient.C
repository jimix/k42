/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskClient.C,v 1.31 2005/08/23 18:38:19 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Toy Disk class implementation
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ProcessWrapper.H>
#include <io/MemTrans.H>
#include <stub/StubMemTrans.H>
#include <scheduler/Scheduler.H>
#include <cobj/TypeMgr.H>
#include <misc/ListSimpleKeyLocked.H>
#include <io/FileLinux.H>
#include <stub/StubBlockDev.H>
#include <io/BlockDev.H>
#include <cobj/CObjRootSingleRep.H>
#include "DiskClient.H"
#include <alloc/PageAllocator.H>
#include <meta/MetaCallBackObj.H>
#include <xobj/XCallBackObj.H>
#include <stub/StubCallBackObj.H>
#include <trace/traceDisk.h>

// FIXME! This shouldn't be including arch-specific code
#include "../../../os/kernel/bilge/arch/powerpc/BootInfo.H" // for SIM_MAMBO

SysStatus
DiskClient::syncRequest(StubBlockDev &stub,
			uval type, uval addr, uval sz, uval offset)
{
    TraceOSDiskSyncRequest((uval)this, type, addr, sz, offset);

    volatile uval ptr = (uval)Scheduler::GetCurThread();
    SysStatus rc;

    do {
	rc = stub._IORequest(type, addr, sz, offset, (uval)&ptr);
	if (_FAILURE(rc)) {
	    // retry only if EBUSY
	    if (_SGENCD(rc) == EBUSY) {
		Scheduler::DelayMicrosecs(10000);
#if 0 // for debugging only
		err_printf(0, "DiskClientAsync::asyncRequest: error calling "
			   "_IORequest EBUSY\n");
#endif
	    } else {
		tassertWrn(0, "DiskClient::asyncRequest: error calling "
			   "_IORequest rc 0x%lx\n", rc);
		break;
	    }
	}
    } while (_FAILURE(rc));

    if (!_FAILURE(rc)) {
	while (ptr == (uval)Scheduler::GetCurThread()) {
	    TraceOSDiskBlock((uval)this, offset, (uval)ptr);
	    Scheduler::Block();
	}
	rc = (SysStatus)ptr;
    }

    return rc;
}

SysStatus
DiskClient::readBlock(uval blockNumber, void *block)
{
    uval offset = blockNumber * BLOCKSIZE;
    tassertMsg(blockNumber < numBlocks, "Bad disk op: %ld\n",blockNumber);

    if ((BLOCKSIZE-1) & (uval)block) {
	uval ptr;
	SysStatus rc=DREFGOBJ(ThePageAllocatorRef)
	    ->allocPagesAligned(ptr, BLOCKSIZE, BLOCKSIZE);
	_IF_FAILURE_RET(rc);

	err_printf("Unaligned block read: %p (%ld)\n",block, blockNumber);

	rc = syncRequest(sbd, BlockDev::DevRead, ptr, BLOCKSIZE, offset);

	if (_SUCCESS(rc)) memcpy(block,(void*)ptr, BLOCKSIZE);
	DREFGOBJ(ThePageAllocatorRef)->deallocPages(ptr,BLOCKSIZE);
	return rc;
    }

    return syncRequest(sbd, BlockDev::DevRead, (uval)block, BLOCKSIZE, offset);
}

SysStatus
DiskClient::writeBlock(uval blockNumber, void *block)
{
    uval offset = blockNumber * BLOCKSIZE;
    tassertMsg(blockNumber < numBlocks, "Bad disk op: %ld\n",blockNumber);

    if ((BLOCKSIZE-1) & (uval)block) {
	uval ptr;
	SysStatus rc=DREFGOBJ(ThePageAllocatorRef)
	    ->allocPagesAligned(ptr, BLOCKSIZE, BLOCKSIZE);
	_IF_FAILURE_RET(rc);

	memcpy((void*)ptr,block, BLOCKSIZE);
	err_printf("Unaligned block write: %p (%ld)\n",block, blockNumber);
	rc = syncRequest(sbd, BlockDev::DevWrite, ptr, BLOCKSIZE, offset);
	DREFGOBJ(ThePageAllocatorRef)->deallocPages(ptr,BLOCKSIZE);
	return rc;
    }

    return syncRequest(sbd, BlockDev::DevWrite,(uval)block, BLOCKSIZE, offset);
}

SysStatus
DiskClient::readBlockPhys(uval blockNumber, uval paddr)
{
    uval offset = blockNumber * BLOCKSIZE;
    uval sz = BLOCKSIZE;

    if (sbdPhys.getOH().invalid()) {
	return _SERROR(2280, 0, EPERM);
    }

    tassertMsg(blockNumber < numBlocks, "Bad disk op: %ld\n",blockNumber);

    tassertMsg(!((BLOCKSIZE-1) & paddr),
	       "Unaligned phys block read: %lx\n",paddr);

    return syncRequest(sbdPhys, BlockDev::DevRead, paddr, sz, offset);
}

/* virtual __async */ SysStatusUval
DiskClient::_processCallBacks(uval arg)
{
    TraceOSDiskProcessCallBacks((uval)this, arg);

    DiskTransportConsumer *transp = NULL;
    switch (arg) {
    case BlockDev::Physical:
	transp = transportPhys;
	break;
    case BlockDev::Virtual:
	transp = transportVirt;
	break;
    default:
	passertMsg(0, "bug\n");
    }
    tassertMsg(transp != NULL, "?");
    SysStatus rc;
    uval token, err;
    do {
	rc = transp->getRequest(token, err);
	if (_SUCCESS(rc)) {
	    uval* addr = (uval*)token;
	    ThreadID thr = (ThreadID)*addr;
	    *addr = err;
	    TraceOSDiskUnblock((uval)this, thr);
	    Scheduler::Unblock(thr);
	}
    } while (_SUCCESS(rc));
    return 0;

}

SysStatus
DiskClient::writeBlockPhys(uval blockNumber, uval paddr)
{
    uval offset = blockNumber * BLOCKSIZE;
    uval sz = BLOCKSIZE;

    if (sbdPhys.getOH().invalid()) {
	return _SERROR(2281, 0, EPERM);
    }

    tassertMsg(blockNumber < numBlocks, "Bad disk op: %ld\n",blockNumber);
    tassertMsg(!((BLOCKSIZE-1) & paddr),
	       "Unaligned phys block write: %lx\n",paddr);

    return syncRequest(sbdPhys, BlockDev::DevWrite, paddr, sz, offset);
}

DiskClient::DiskClient():sbd(StubObj::UNINITIALIZED),
			 sbdPhys(StubObj::UNINITIALIZED),
			 transportPhys(NULL), transportVirt(NULL)
{
    /* empty body */
}

uval
DiskClient::Init(char* blockDev)
{
    breakpoint();
    return 0;
}

/*static */ void
DiskClient::ClassInit(VPNum vp)
{
    if (vp==0)
	MetaCallBackObj::init();
}

/*static*/ SysStatus
DiskClient::Create(DiskClientRef &dcr, char* blockDev,
		   uval needPhysAddr /* = 1 */)
{
    DiskClient *dc = new DiskClient;

    SysStatus rc = dc->init(blockDev, needPhysAddr);

    _IF_FAILURE_RET(rc);

    dcr = dc->getRef();

    return 0;
}

/* virtual*/ SysStatus
DiskClient::init(char* blockDev, uval needPhysAddr /* = 1 */)
{
    TraceOSDiskInit((uval)this, needPhysAddr, blockDev);

    SysStatus rc=0;
    FileLinuxRef flr;
    uval count = 0;

    // We may have to wait until the block devices are initialized
    // There should be a smarter way of doing it... FS should not
    // be started until we know that the device is there, but there's
    // no mechanism for that right now.
    do {
	rc = FileLinux::Create(flr, blockDev, 0, 0);
	if (_SUCCESS(rc) || _SGENCD(rc)==ENXIO) {
	    break;
	}
	// FIXME FIXME FIXME! Kludge to get polling timeouts better on mambo
	// The right fix would be to fix K42's initialization dependencies
	if (KernelInfo::OnSim() == SIM_MAMBO) {
	    Scheduler::DelayMicrosecs(10000);
	} else {
	    Scheduler::DelaySecs(2);
	}
	++count;
	if (count == 15) {
	    return _SERROR(2005, 0, ENODEV);
	}
    } while (_FAILURE(rc));
    _IF_FAILURE_RET(rc);

    ObjectHandle oh;

    FileLinux::Stat stat;
    DREF(flr)->getStatus(&stat);
    blkSize = stat.st_blksize;

    passertWrn(blkSize == BLOCKSIZE, "blkSize 0x%lx (We're working with "
	       "0x%lx)\n", blkSize, BLOCKSIZE);

    diskSize = stat.st_size;
    /* Using BLOCKSIZE insteade of blkSize, because BLOCKSIZE is still the
     * value we use on read/write operations to check if given offset makes
     * sense */
    numBlocks = diskSize/BLOCKSIZE;

#if 0
    err_printf("Disk Client has size 0x%lx, blkSize 0x%lx\n",
	       diskSize, blkSize);
#endif

    DREF(flr)->getOH(oh);

    if (!oh.valid()) {
	err_printf("Bad OH: %016lx %016lx\n", oh.commID(), oh.xhandle());
	DREF(flr)->detach();
	return _SERROR(2282, 0, EINVAL);
    }

    CObjRootSingleRep::Create(this);

    // By default, we get an ObjectHandle for a file, but we want a
    // block device, so we ask for a block device here
    StubObj sobj(StubObj::UNINITIALIZED);
    sobj.setOH(oh);

    ObjectHandle newOH;
    rc = sobj._giveAccess(newOH,DREFGOBJ(TheProcessRef)->getPID(),
			  MetaObj::controlAccess|MetaObj::read|MetaObj::write,
			  MetaObj::none,
			  StubBlockDev::typeID());

    if (_FAILURE(rc)) goto end;

    sbd.setOH(newOH);

    ObjectHandle cbOH;
    rc = giveAccessByServer(cbOH, newOH.pid(), StubCallBackObj::typeID());

    if (_FAILURE(rc)) goto end;

    ObjectHandle transpFROH;
    // Specify the addressing mode
    rc = sbd._useMode(BlockDev::Virtual, cbOH, transpFROH);

    if (_FAILURE(rc)) goto end;

    // set transport area for getting callbacks from sbd
    transportVirt = new DiskTransportConsumer();
    rc = transportVirt->init(transpFROH, transpFROH.pid());
    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    if (needPhysAddr) {
	/* We need to create a second OH which will use a physical
	 * addressing mode */
	rc = sobj._giveAccess(
	    newOH, DREFGOBJ(TheProcessRef)->getPID(),
	    MetaObj::controlAccess|MetaObj::read|MetaObj::write, MetaObj::none,
	    StubBlockDev::typeID());
	
	if (_FAILURE(rc)) goto end;
	
	sbdPhys.setOH(newOH);
	
	// Specify the addressing mode
	rc = sbdPhys._useMode(BlockDev::Physical, cbOH, transpFROH);
	
	if (_FAILURE(rc)) {
	    tassertMsg(0, "hitting failure in _useMode\n");
	    //Can't use physical addressing mode
	    ObjectHandle invalid;
	    sbdPhys.setOH(invalid);
	} else {
	    // set transport area for getting callbacks from sbdPhys
	    transportPhys = new DiskTransportConsumer();
	    rc = transportPhys->init(transpFROH, transpFROH.pid());
	    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
	}
    } else {
	ObjectHandle invalid;
	sbdPhys.setOH(invalid);
    }

  end:
    //Now close the FileLinux object -- don't need it anymore
    DREF(flr)->detach();

    return rc;
}

/* virtual */ SysStatus
DiskClient::DiskTransportConsumer::init(ObjectHandle tFROH, ProcessID pidProducer)
{
    SharedBufferConsumer::init(tFROH, pidProducer, BlockDev::TRANSPORT_SIZE,
			       BlockDev::TRANSPORT_ENTRY_UVALS,
			       BlockDev::TransportNumEntries);
    return 0;
}

/* virtual */ SysStatus
DiskClient::DiskTransportConsumer::getRequest(uval &token, uval &err)
{
    SysStatus rc;
    AutoLock<BLock> al(&lock);
    BlockDev::CallBackRequest req;
    rc = locked_getRequest((uval*)&req);
    _IF_FAILURE_RET(rc);
    token = req.token;
    err = req.err;
    return 0;
}
