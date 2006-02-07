/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxCharDev.C,v 1.5 2005/07/15 17:14:30 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: interface to Linux pty/tty code
 ****************************************************************************/

#define __KERNEL__
#define eieio __linux_eieio
#define ffs __linux_ffs
#define FASTCALL(x) x
#include <linux/bitops.h>
#undef eieio
#undef ffs
#undef __KERNEL__

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>


#include <lk/LinuxEnv.H>
#include <misc/HashSimple.H>

//#undef PAGE_SIZE
//#undef PAGE_MASK
#include "LinuxCharDev.H"
#include <stub/StubDevFSDir.H>
#include <stub/StubDevFSRemote.H>
#include <io/FileLinux.H>

//#include <lk/Utils.H>
#include <asm/ioctls.h>
#include <meta/MetaDevOpener.H>
#include <xobj/XDevOpener.H>
#include <linux/k42devfs.h>
#include <lk/InitCalls.H>

#define INIT_LIST_HEAD(ptr) do {		  \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
    } while (0)

extern "C" {
    void remove_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
    struct DevInfo;
    int chardev_open(struct DevInfo** pdi, dev_t dev,
		     unsigned long oflag);
    int chardev_ioctl(struct DevInfo *di,
		      unsigned int cmd, unsigned long *size, char* arg);
    int chardev_release(struct DevInfo** pdi);
    int chardev_poll(struct DevInfo* di);
    int chardev_read(struct DevInfo* di,
		     char* buf, size_t count, unsigned long *avail);
    int chardev_write(struct DevInfo* di,
		      const char* buf, size_t count,
		      unsigned long *avail);
}

/* static */ SysStatus
CharDevOpener::Create(unsigned int major, unsigned int minor,
		      ProcessID devfsPID,
		      CharDevOpenerRef &ref, ObjectHandle &opener)
{
    SysStatus rc = 0;
    CharDevOpener* cdo = new CharDevOpener();
    cdo->devNum = MKDEV(major,minor);
    CObjRootSingleRep::Create(cdo);
    ref = (CharDevOpenerRef)cdo->getRef();

    rc = DREF(ref)->giveAccessSetClientData(opener, devfsPID,
					    MetaObj::controlAccess,
					    MetaObj::none);

    if (_FAILURE(rc)) {
	DREF(ref)->destroy();
	ref = NULL;
    }
    return rc;
}

CharDevOpener::DeviceHash* CharDevOpener::charDevs = NULL;

/* static */ SysStatus
LinuxCharDev::ClassInit(VPNum vp)
{
    if (vp!=0) return 0;

    CharDevOpener::charDevs = new CharDevOpener::DeviceHash;

    MetaStreamServer::init();
    MetaDevOpener::init();

    return 0;
}
#if 0
SysStatus
DestroyDevice(devfs_handle_t dev, unsigned int major,
	      unsigned int minor, void* devData)
{
    err_printf("devfs remove for %x %x\n", major, minor);
    return 0;
}
#endif
SysStatus
CharCreateDevice(ObjectHandle dirOH,
		 const char *name, mode_t mode, unsigned int major,
		 unsigned int minor, ObjectHandle &device,
		 void* &devData)
{
    SysStatus rc = 0;
    StubDevFSRemote devStub(StubBaseObj::UNINITIALIZED);
    ObjectHandle serverOH;

    ObjectHandle devfsServer;
    rc = DREFGOBJ(TheTypeMgrRef)->getTypeHdlr(StubDevFSRemote::typeID(),
					      devfsServer);

    _IF_FAILURE_RET(rc);
    CharDevOpenerRef cdoRef;
    CharDevOpener::charDevs->acquireLock();
    if (CharDevOpener::charDevs->locked_find(MKDEV(major,minor),cdoRef)) {
	rc = DREF(cdoRef)->giveAccessSetClientData(serverOH,
						   devfsServer.pid(),
						   MetaObj::controlAccess,
						   MetaObj::none);
    } else {
	rc = CharDevOpener::Create(major, minor, devfsServer.pid(),
				   cdoRef, serverOH);
	CharDevOpener::charDevs->locked_add(MKDEV(major,minor), cdoRef);
    }
    CharDevOpener::charDevs->releaseLock();

    _IF_FAILURE_RET(rc);

    rc = devStub._CreateNode((char*)name, mode, dirOH, serverOH,
			     new_encode_dev(MKDEV(major,minor)), device);

    tassertRC(rc,"device node creation\n");
    return 0;
}


/* virtual */ SysStatus
CharDevOpener::_open(__out ObjectHandle &oh, __out TypeID &type,
		     __in ProcessID pid, __in uval oflag, __in uval token)
{
    return LinuxCharDev::Create(oh, type, devNum, pid, oflag);
}

/* virtual */ SysStatus
CharDevOpener::destroy()
{
    CharDevOpenerRef cdoRef;
    SysStatus rc = 0;
    if (MAJOR(devNum)>=UNIX98_PTY_SLAVE_MAJOR  &&
	MAJOR(devNum)<UNIX98_PTY_SLAVE_MAJOR + UNIX98_PTY_MAJOR_COUNT) {
	rc = DREFGOBJ(TheProcessLinuxRef)->removeTTY(
	    devNum - MKDEV(UNIX98_PTY_SLAVE_MAJOR,0));
	tassertMsg(_SUCCESS(rc),
		   "Should have succeeded removing TTY: %lx\n", rc);
    }
    charDevs->remove(devNum,cdoRef);
    return Obj::destroy();
}


/* virtual */ SysStatus
LinuxCharDev::giveAccessSetClientData(ObjectHandle &oh, ProcessID toProcID,
				      AccessRights match,
				      AccessRights nomatch, TypeID type)
{
    SysStatus rc;
    ClientData *cd = new ClientData(toProcID);
    rc = giveAccessInternal(oh, toProcID, match, nomatch, type, (uval)cd);
    if (_FAILURE(rc)) {
	delete cd;
    }
    return rc;
}

static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}


/* virtual */ void
LinuxCharDev::notify(unsigned long arg) {
    SysStatus rc;
    AutoLock<LockType> al(&_lock);

    availMask = FileLinux::INVALID;
    rc = locked_signalDataAvailable();
    tassertMsg(_SUCCESS(rc),
	       "CharDev locked_signalDataAvailable failed: %lx\n", rc);

}

/* static */ SysStatus
LinuxCharDev::Create(ObjectHandle &oh, TypeID &type,
		     dev_t num, ProcessID pid, uval oflag)
{

    SysStatus rc;
    int ret;
    LinuxCharDev *lcd = new LinuxCharDev();
    LinuxCharDevRef ref;

    lcd->devNum = num;
    lcd->availMask = FileLinux::INVALID;
    CObjRootSingleRep::Create(lcd);
    ref = (LinuxCharDevRef)lcd->getRef();

    rc = DREF(ref)->giveAccessByServer(oh, pid);
    if (_FAILURE(rc)) {
	DREF(ref)->destroy();
	return rc;
    }

    LinuxEnv sc(SysCall);
    ret = chardev_open(&lcd->di, num, oflag|O_NONBLOCK);

    if (ret<0) {
	rc = _SERROR(2365, 0, -ret);
	// releaseAccess will trigger destroy
	DREF(ref)->releaseAccess(oh.xhandle());
    } else {
	lcd->devNum = ret;
	type = FileLinux_STREAM;
    }
    return rc;
}


/* virtual */ SysStatusUval
LinuxCharDev::_ioctl(__in uval request, __inout uval &size,
		     __inoutbuf(size:size:size) char* arg,
		     __XHANDLE xhandle)
{
    SysStatusUval rc;

    LinuxEnv sc(SysCall, NULL);

    int ret = chardev_ioctl(di, request, &size, arg);
    if (ret<0) {
	rc = _SERROR(2366, 0, -ret);
    } else {
	rc = _SRETUVAL(ret);
    }

    return rc;
}

/* virtual */ SysStatus
LinuxCharDev::destroy()
{    SysStatus rc;
    AutoLock<LockType> al(&_lock);
    // remove all ObjRefs to this object
    rc = closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if (_FAILURE(rc)) {
	return _SCLSCD(rc) == 1 ? 0: rc;
    }


    if (di) {
	LinuxEnv sc(SysCall);
	chardev_release(&di);
    }
    return destroyUnchecked();

}


/* virtual */ void
LinuxCharDev::calcAvailable(GenState& avail, StreamServer::ClientData* client)
{
    uval state;

    if (availMask == FileLinux::INVALID && client) {
	LinuxEnv sc; //Linux environment object
	availMask = chardev_poll(di);
    }
    state = availMask;
    if (availMask & (FileLinux::POLLHUP|FileLinux::ENDOFFILE)) {
	state |= FileLinux::ENDOFFILE;
    }

    if (availMask &
	(FileLinux::POLLOUT |
	 FileLinux::POLLWRNORM |
	 FileLinux::POLLWRBAND) ) {
	state |= FileLinux::WRITE_AVAIL;
    }

    if (availMask &
	(FileLinux::POLLIN |
	 FileLinux::POLLRDNORM |
	 FileLinux::POLLRDBAND)) {
	state |= FileLinux::READ_AVAIL;
    }

    avail.state = state;
}

/* virtual */ SysStatusUval
LinuxCharDev::recvfrom(struct iovec *vec, uval veclen, uval flags,
		       char *addr, uval &addrLen, GenState &moreAvail, 
		       void *controlData, uval &controlDataLen,
		       __XHANDLE xhandle)
{
    addrLen = 0;

    controlDataLen = 0; /* setting to zero, since no control data */

    AutoLock<LockType> al(&_lock);
    ClientData *cd = getClientData(xhandle);
    SysStatusUval rc = 0;


    LinuxEnv sc(SysCall);
    rc = 0;
    for (uval i = 0; i<veclen; ++i) {
	int nr = chardev_read(di, (char*)vec[i].iov_base,
			      vec[i].iov_len, &availMask);
	if (nr < 0) {
	    if (rc>0) break;
	    if (nr==-EWOULDBLOCK) break;
	    rc = _SERROR(2495, 0, -nr);
	    break;
	}
	rc += nr;
    }
    availMask = chardev_poll(di);
    calcAvailable(moreAvail);
    cd->setAvail(moreAvail);
    return rc;
}

/* virtual */ SysStatusUval
LinuxCharDev::sendto(struct iovec* vec, uval veclen, uval flags,
		     const char *addr, uval addrLen, GenState &moreAvail, 
		     void *controlData, uval controlDataLen,
		     __XHANDLE xhandle)
{
    tassertMsg((controlDataLen == 0), "oops\n");

    AutoLock<LockType> al(&_lock);
    ClientData *cd = getClientData(xhandle);
    SysStatusUval rc = 0;

    LinuxEnv sc(SysCall);
    rc = 0;
    for (uval i = 0; i<veclen; ++i) {
	int nr = chardev_write(di, (char*)vec[i].iov_base,
			       vec[i].iov_len, &availMask);
	if (nr < 0) {
	    if (rc>0) break;
	    if (nr==-EWOULDBLOCK) break;
	    rc = _SERROR(2367, 0, -nr);
	    break;
	}
	rc += nr;
    }
    availMask = chardev_poll(di);
    calcAvailable(moreAvail);
    cd->setAvail(moreAvail);
    return rc;

}

