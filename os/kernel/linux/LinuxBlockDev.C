/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxBlockDev.C,v 1.3 2005/01/10 15:30:34 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface for linux block devices.
 * **************************************************************************/
#define __KERNEL__
#include <kernIncs.H>

#define eieio __k42_eieio
#include <misc/hardware.H>
#undef eieio
extern "C" {
#include <linux/linkage.h>
#define ffs __lk_ffs
#include <asm/bitops.h>
#undef ffs
#include <linux/socketlinux.h>
#include <linux/sockios.h>
#undef __attribute_used__
#undef __attribute_pure__
#undef likely
#undef unlikely
typedef long __kernel_off_t;
typedef __kernel_off_t off_t;
#undef PAGE_SIZE
#undef PAGE_MASK
#include <linux/thread_info.h>
#include <asm/current.h>
}

#undef __KERNEL__ //We don't really use anything lk specific here

#include <cobj/CObjRootSingleRep.H>
#include "LinuxBlockDev.H"

#include <io/PAPageServer.H>
#include <meta/MetaPAPageServer.H>

#include <stub/StubFRPA.H>

#include <stub/StubRegionFSComm.H>
#include <lk/LinuxEnv.H>
extern "C"{
#define __KERNEL__
#undef major
#undef minor
#include <linux/kdev_t.h>
#include <linux/k42devfs.h>
#undef __KERNEL__
}

struct iocmd {
    struct bio* cmd;
    struct page* pages;
    uval32 page_count;
    void (*complete)(struct iocmd* ic, int err);
};


extern "C" {
    int LinuxBlockOpen(int dev, void **data, unsigned long *size,
		       unsigned long *blkSize);
    void LinuxBlockClose(void *data);
    int LinuxIoctl(int dev, void *wrap,
			 unsigned cmd, unsigned long arg);
    unsigned long AsyncLinuxBlockOp(int opType, void* devData,
				    unsigned long location, void *ptr,
				    unsigned long size, struct iocmd *ic);
    int k42_make_blkdev(unsigned int major,
			const char* name,
			void *data);
}

SysStatus
DestroyDevice(devfs_handle_t dev, unsigned int major,
	      unsigned int minor, void* devData)
{
    LinuxBlockDevRef lbdr = (LinuxBlockDevRef)devData;
    DREF(lbdr)->destroy();
    return 0;
}

SysStatus
BlockCreateDevice(ObjectHandle dirOH,
		  const char *name, mode_t mode, unsigned int major,
		  unsigned int minor, ObjectHandle &deviceNode,
		  void* &devData)
{
    SysStatus rc;

    LinuxBlockDev* lbd = new LinuxBlockDev;
    rc = lbd->init(new_encode_dev(MKDEV(major,minor)),
		   name, dirOH, deviceNode);

    if (_FAILURE(rc)) {
	DREF(lbd->getRef())->destroy();
    }

    devData = (void*)lbd->getRef();
    return rc;
}

// A cludge to type cheat XBaseObj clientData to ClientData
static inline
LinuxBlockDev::ClientData* clnt(XHandle xhandle)
{
    LinuxBlockDev::ClientData* retvalue;
    retvalue = (LinuxBlockDev::ClientData *)
	(XHandleTrans::GetClientData(xhandle));
    return (retvalue);
}


void
LinuxBlockDev::ClassInit(VPNum vp)
{
    BlockDevBase::ClassInit();
}


/* virtual */ SysStatus
LinuxBlockDev::devOpen()
{
    LinuxEnv sc(SysCall);
    int err = LinuxBlockOpen(old_decode_dev(devID), &devData,
			     &devSize, &blkSize);
    if (err<0) {
	return _SERROR(1959, 0, -err);
    }
    return 0;
}

/*virtual*/ SysStatus
LinuxBlockDev::init(int deviceID, const char* name,
		    ObjectHandle dir, ObjectHandle &node)
{
    const int perm = S_IFBLK|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
    devData = NULL;

    SysStatus rc = BlockDevBase::init(name, deviceID, perm, dir, node);

    return rc;
}

/*virtual*/ SysStatus
LinuxBlockDev::_write(__in uval srcAddr,
		      __in uval objOffset,
		      __in uval len,
		      __XHANDLE xhandle)
{
    SysStatus rc = 0, rc2 = 0;
    ClientData *cd = clnt(xhandle);
    PinnedMapping pm;
    uval addr = 0;

    rc = fixupAddrPreWrite(&pm, addr, srcAddr, len, cd);
    _IF_FAILURE_RET(rc);

    rc = putBlock(addr, len, objOffset);

    rc2 = fixupAddrPostWrite(&pm, addr, srcAddr, len, cd);

    if (_SUCCESS(rc)) {
	_IF_FAILURE_RET(rc2);
	rc = 0;
    }

    return rc;
}

struct UnblockComplete: public LinuxBlockDev::IOCompletion {
    DEFINE_NOOP_NEW(UnblockComplete);
    ThreadID thr;
    SysStatus error;
    UnblockComplete():thr(Scheduler::GetCurThread()) {};
    virtual void complete(BlockDevRef ref, SysStatus err) {
	ThreadID t = thr;
	thr = Scheduler::NullThreadID;
	error = err;
	Scheduler::Unblock(t);
    }
    int wait() {
	while (thr != Scheduler::NullThreadID) {
	    Scheduler::Block();
	}
	return _SGENCD(error);
    }
};

/* virtual */ SysStatus
LinuxBlockDev::putBlock(uval physAddr, uval len, uval objOffset)
{
    SysStatus rc;
    UnblockComplete wait;

    rc = asyncOp(DevWrite, physAddr, len, objOffset, &wait);
    _IF_FAILURE_RET(rc);

    int ret = wait.wait();
    if (ret<0) {
	return  _SERROR(2793, 0, -ret);
    }
    return 0;
}

/* virtual */ SysStatus
LinuxBlockDev::getBlock(uval physAddr, uval len, uval objOffset)
{
    SysStatus rc;
    UnblockComplete wait;

    rc = asyncOp(DevRead, physAddr, len, objOffset, &wait);
    _IF_FAILURE_RET(rc);

    int ret = wait.wait();
    if (ret<0) {
	return  _SERROR(2792, 0, -ret);
    }
    return 0;
}

/* virtual */ SysStatus
LinuxBlockDev::_getBlock(__in uval srcAddr, __in uval len,
			 __in uval objOffset, __XHANDLE xhandle)
{
    SysStatus rc = 0, rc2 = 0;
    ClientData *cd = clnt(xhandle);
    PinnedMapping pm;
    uval addr;
    rc = fixupAddrPreRead(&pm, addr, srcAddr, len, cd);
    _IF_FAILURE_RET(rc);

    rc = getBlock(addr, len, objOffset);

    rc2 = fixupAddrPostRead(&pm, addr, srcAddr, len, cd);

    if (_SUCCESS(rc)) {
	_IF_FAILURE_RET(rc2);
    }

    return rc;
}

struct IOCmdState : public iocmd {
    DEFINE_PINNEDGLOBALPADDED_NEW(IOCmdState);
    LinuxBlockDevRef lbd;
    LinuxBlockDev::IOCompletion *ioc;
};


//Call through this trivial function to activate the LinuxBlockDev ref
/* virtual */ SysStatus
LinuxBlockDev::ioComplete(struct IOCmdState* is, SysStatus err) {
    is->ioc->complete((BlockDevRef)getRef(), err);
    delete is;
    return 0;
}

extern "C" void notify_io_complete(struct iocmd* ic, int err);
void notify_io_complete(struct iocmd* ic, int err) {
    IOCmdState *state = (IOCmdState*)ic;
    SysStatus rc=0;
    if (err < 0) {
	rc = _SERROR(2286, 0, -err);
    }
    DREF(state->lbd)->ioComplete(state, rc);
}

/* virtual */ SysStatus
LinuxBlockDev::asyncOp(uval type, uval physAddr, uval len,
		       uval objOffset, IOCompletion *ioc)
{
    IOCmdState *state = new IOCmdState;
    int ret;
    memset(state,0,sizeof(IOCmdState));
    state->ioc = ioc;
    state->lbd = getRef();
    state->complete = notify_io_complete;

    LinuxEnv sc;
    ret = AsyncLinuxBlockOp(type, devData, objOffset,
			    (void*)physAddr, len, state);
    if (ret<0) {
	ioComplete(state, _SERROR(2285, 0, -ret));
    }
    return 0;

}

/* virtual */ SysStatusUval
LinuxBlockDev::_ioctl(__in uval req,
		      __inoutbuf(size:size:size) char* buf,
		      __inout uval &size)
{

    LinuxEnv sc; //Linux environment object
    int err= LinuxIoctl(old_decode_dev(devID), devData,
			req, (unsigned long)buf);
    SysStatusUval rc;
    if (err<0) {
	return _SERROR(1966, 0, -err);
    }
    rc = (uval)err;
    return rc;

}


extern "C" struct page *
_alloc_pages(unsigned int gfp_mask, unsigned int order);

static void
linux_io_complete(struct iocmd* ic, int err) {
    IOCmdState *state = (IOCmdState*)ic;
    SysStatus rc=0;
    if (err < 0) {
	rc = _SERROR(2794, 0, -err);
    }
    state->ioc->complete(NULL, rc);
}

extern "C" int
__read_dev_page(struct block_device *bd, unsigned long offset, void *buffer);

int
__read_dev_page(struct block_device *bd, unsigned long offset, void* buffer)
{
    UnblockComplete wait;
    IOCmdState state;
    int ret;
    memset(&state,0,sizeof(IOCmdState));
    state.ioc = &wait;
    state.complete = linux_io_complete;

    ret = AsyncLinuxBlockOp(BlockDev::DevRead, bd, offset, buffer,
			    PAGE_SIZE, &state);
    if (ret<0) {
	return ret;
    }

    return wait.wait();
}


extern SysStatus
BlockCreateDevice(ObjectHandle dirOH,
		  const char *name, mode_t mode, unsigned int major,
		  unsigned int minor, ObjectHandle &deviceNode,
		  void* &devData);

extern SysStatus
CharCreateDevice(ObjectHandle dirOH,
		 const char *name, mode_t mode, unsigned int major,
		 unsigned int minor, ObjectHandle &deviceNode,
		 void* &devData);

SysStatus
CreateDevice(ObjectHandle dirOH,
	     const char *name, mode_t mode, unsigned int major,
	     unsigned int minor, ObjectHandle &deviceNode,
	     void* &devData)
{
    if (S_ISBLK(mode)) {
	return BlockCreateDevice(dirOH, name, mode, major, minor,
				 deviceNode,devData);
    }
    if (S_ISCHR(mode)) {
	return CharCreateDevice(dirOH, name, mode, major, minor,
				deviceNode,devData);
    }
    passertMsg(0, "Unknown device type\n");
    return 0;
}
