/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxPTY.C,v 1.18 2005/07/15 17:14:35 rosnbrg Exp $
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

#include "LinuxPTY.H"
#include <stub/StubFileSystemDev.H>
#include <stub/StubDevFSRemote.H>
#include <io/FileLinux.H>

//#include <lk/Utils.H>
#include <asm/ioctls.h>
#include <meta/MetaDevOpener.H>
#include <xobj/XDevOpener.H>
#include <linux/k42devfs.h>
#include <lk/InitCalls.H>

extern void LinuxEnvSuspend();
extern void LinuxEnvResume();

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

struct TTYEntry {
    DEFINE_GLOBAL_NEW(TTYEntry);
    ObjectHandle oh;
    void*	token;
};

HashSimpleLocked<int, TTYEntry*, AllocGlobal, 0> *ptsEntries;

void LinuxPTYServer_ClassInit()
{
    ptsEntries = new typeof(*ptsEntries);

    LinuxPTYServer::ClassInit(0);
}

extern "C" {
    void remove_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
    struct OpenLineInfo;
    int ttydev_open(struct OpenLineInfo** poli, dev_t dev,
		    wait_queue_head_t **readQ, wait_queue_head_t **writeQ,
		    unsigned long oflag);
    int ttydev_ioctl(struct OpenLineInfo *oli,
		     unsigned int cmd, unsigned long *size, char* arg);
    int ttydev_release(struct OpenLineInfo** poli);
    int ttydev_getPtyToken(int line, unsigned long *token);
    int ttydev_poll(struct OpenLineInfo* oli);
    int ttydev_read(struct OpenLineInfo* oli,
		    char* buf, size_t count, unsigned long *avail);
    int ttydev_write(struct OpenLineInfo* oli,
		     const char* buf, size_t count,
		     unsigned long *avail);
}

devfs_handle_t ptyDir;
devfs_handle_t ptsDir;
extern void getDevFSOH(devfs_handle_t node, ObjectHandle &oh);



// Define some functions to complete the Linux Kernel emulation layer
extern "C" unsigned long __k42_pa(unsigned long v) { return v; };
extern "C" unsigned long __k42_va(unsigned long p) { return p; };

extern void setMemoryAllocator(PageAllocatorRef allocRef);

static char kernel_lock_buf[sizeof(FairBLock)]={0,};
FairBLock *kernel_lock=(FairBLock*)kernel_lock_buf;

struct tty_struct;

extern "C" void k42devfsInit();

extern "C"
struct task_struct*
getTaskStruct(char* mem, unsigned long mem_size, //memory to create task
	      pid_t _pid, pid_t _pgrp,
	      pid_t _session, struct tty_struct* _tty, int _leader);

struct PTYTaskInfo {
    struct task_struct *t;
    char *mem;
    PTYTaskInfo(pid_t _pid, pid_t _pgrp, pid_t _session,
		uval _tty, uval _leader) {
	// guess that 2048 is big enough for task struct
	mem = (char*)allocGlobal(2048);
	memset(mem, 0, 2048);

	t = getTaskStruct(mem, 2048, _pid, _pgrp, _session,
			  (struct tty_struct*)_tty, _leader);
    }
    ~PTYTaskInfo() {
	freeGlobal(mem, 2048);
    }
};

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
devfs_handle_t k42devfs_mk_dir (const char *name);

extern void ConfigureLinuxEnv(VPNum vp,PageAllocatorRef pa);
extern void ConfigureLinuxGeneric(VPNum vp);
extern void ConfigureLinuxFinal(VPNum vp);
extern void LinuxStartVP(VPNum vp, SysStatus (*initfn)(uval arg));

typedef int (*initcall_t)(void);
typedef void (*exitcall_t)(void);
extern initcall_t __initcall_tty_init;
extern initcall_t __initcall_pty_init;
extern initcall_t __initcall_tty_class_init;
extern "C" void vfs_caches_init(unsigned long numpages);
extern "C" void driver_init(void);
extern "C" void console_init(void);

/* static */ SysStatus
LinuxPTYServer::ClassInit(VPNum vp)
{

    if (vp==0) {
	CharDevOpener::charDevs = new CharDevOpener::DeviceHash;
	MetaStreamServer::init();
	MetaDevOpener::init();
    }

    ConfigureLinuxEnv(vp, GOBJ(ThePageAllocatorRef));
    ConfigureLinuxGeneric(vp);

    if (vp==0) {
	LinuxEnv le(SysCall);
	vfs_caches_init(0x1<<8);
	driver_init();

	console_init();

	ptyDir = k42devfs_mk_dir("pty");
	ptsDir = k42devfs_mk_dir("pts");

	RegisterInitCall(new InitCall(&__initcall_tty_init));
	RegisterInitCall(new InitCall(&__initcall_pty_init));
	RegisterInitCall(new InitCall(&__initcall_tty_class_init));

	LinuxEnvSuspend();
	VPNum vpCnt = _SGETUVAL(DREFGOBJ(TheProcessRef)->vpCount());
	for (VPNum i = 1; i < vpCnt; i++) {
	    LinuxStartVP(i, LinuxPTYServer::ClassInit);
	}

	LinuxEnvResume();
	ConfigureLinuxFinal(0);
    }

    return 0;
}

SysStatus
DestroyDevice(devfs_handle_t dev, unsigned int major,
	      unsigned int minor, void* devData)
{
    err_printf("devfs remove for %x %x\n", major, minor);
    return 0;
}

SysStatus
CreateDevice(ObjectHandle dirOH,
	     const char *name, mode_t mode, unsigned int major,
	     unsigned int minor, ObjectHandle &device,
	     void* &devData)
{
    SysStatus rc = 0;
    const mode_t perm= S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
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

    rc = devStub._CreateNode((char*)name, perm, dirOH, serverOH,
			     new_encode_dev(MKDEV(major,minor)), device);

    tassertRC(rc,"device node creation\n");
    return 0;
}


/* virtual */ SysStatus
CharDevOpener::_open(__out ObjectHandle &oh, __out TypeID &type,
		     __in ProcessID pid, __in uval oflag, __in uval token)
{
    return LinuxPTYServer::Create(oh, type, devNum, pid, oflag);
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
LinuxPTYServer::giveAccessSetClientData(ObjectHandle &oh, ProcessID toProcID,
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
LinuxPTYServer::notify(unsigned long arg) {
    SysStatus rc;
    AutoLock<LockType> al(&_lock);

    if (readQ && list_empty(&readCB.task_list)) {
	add_wait_queue(readQ, &readCB);
    }

    if (writeQ && list_empty(&writeCB.task_list)) {
	add_wait_queue(writeQ, &writeCB);
    }

    availMask = FileLinux::INVALID;
    rc = locked_signalDataAvailable();
    tassertMsg(_SUCCESS(rc),
	       "PTY locked_signalDataAvailable failed: %lx\n", rc);

}

/* static */ SysStatus
LinuxPTYServer::Create(ObjectHandle &oh, TypeID &type,
		       dev_t num, ProcessID pid, uval oflag)
{

    SysStatus rc;
    int ret;
    LinuxPTYServer *pty = new LinuxPTYServer();
    LinuxPTYServerRef ref;

    pty->devNum = num;
    pty->availMask = FileLinux::INVALID;
    CObjRootSingleRep::Create(pty);
    ref = (LinuxPTYServerRef)pty->getRef();
    pty->readCB.obj = ref;
    pty->writeCB.obj = ref;

    rc = DREF(ref)->giveAccessByServer(oh, pid);
    if (_FAILURE(rc)) {
	DREF(ref)->destroy();
	return rc;
    }

    ProcessLinux::LinuxInfo info;
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoNativePid(pid, info);
    tassertMsg(_SUCCESS(rc),
	       "Couldn't get info about k42 process 0x%lx\n", pid);

    PTYTaskInfo ptyTI(info.pid, info.pgrp, info.session, info.tty,
		      info.pid == info.session);
    LinuxEnv sc(SysCall, ptyTI.t);
    ret = ttydev_open(&pty->oli, num, &pty->readQ, &pty->writeQ,
		      oflag|O_NONBLOCK);

    if (ret<0) {
	rc = _SERROR(2365, 0, -ret);
	// releaseAccess will trigger destroy
	DREF(ref)->releaseAccess(oh.xhandle());
    } else {
	pty->devNum = ret;

	//Put ourselves on wait queue with TTYEvents object that will
	// generate callbacks instead of waking threads
	if (pty->readQ && list_empty(&pty->readCB.task_list)) {
	    add_wait_queue(pty->readQ, &pty->readCB);
	}

	if (pty->writeQ && list_empty(&pty->writeCB.task_list)) {
	    add_wait_queue(pty->writeQ, &pty->writeCB);
	}

	type = (TypeID)new_encode_dev(pty->devNum);
    }
    return rc;
}


/* virtual */ SysStatusUval
LinuxPTYServer::_ioctl(__in uval request, __inout uval &size,
		       __inoutbuf(size:size:size) char* arg,
		       __XHANDLE xhandle) {
    SysStatusUval rc;
    ProcessID pid = XHandleTrans::GetOwnerProcessID(xhandle);

    ProcessLinux::LinuxInfo info;
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoNativePid(pid, info);
    tassertMsg(_SUCCESS(rc),
	       "Couldn't get info about k42 process 0x%lx\n", pid);

    PTYTaskInfo ti(info.pid, info.pgrp, info.session,
		   info.tty, info.session == info.pid);
    LinuxEnv sc(SysCall, ti.t);
    switch (request) {
    case TIOCSCTTY:
	//Arg is literal value,  passed over as buffer contents by PPC
	arg = (char*)(uval)(((unsigned int*)arg)[0]);

	break;
    }

    int ret = ttydev_ioctl(oli, request, &size, arg);
    if (ret<0) {
	rc = _SERROR(2366, 0, -ret);
    } else {
	rc = _SRETUVAL(ret);
	switch (request) {
	case TIOCSCTTY:
	{
	    rc = DREFGOBJ(TheProcessLinuxRef)->setCtrlTTY(
		devNum - MKDEV(UNIX98_PTY_SLAVE_MAJOR,0), pid);
	    tassertMsg(_SUCCESS(rc), "setCtrlTTY failed: %lx\n",rc);
	    break;
	}
	default:
	    ;
	}
    }

    return rc;
}

/* virtual */ SysStatus
LinuxPTYServer::destroy()
{    SysStatus rc;
    AutoLock<LockType> al(&_lock);
    // remove all ObjRefs to this object
    rc = closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if (_FAILURE(rc)) {
	return _SCLSCD(rc) == 1 ? 0: rc;
    }

    LinuxEnv sc(SysCall);
    if (readQ)  {
	remove_wait_queue(readQ, &readCB);
	readQ = NULL;
	INIT_LIST_HEAD(&readCB.task_list);
    }
    if (writeQ) {
	remove_wait_queue(writeQ, &writeCB);
	writeQ = NULL;
	INIT_LIST_HEAD(&writeCB.task_list);
    }
    if (oli) {
	ttydev_release(&oli);
    }
    return destroyUnchecked();

}


/* virtual */ void
LinuxPTYServer::calcAvailable(GenState& avail,
			      StreamServer::ClientData* client)
{
    uval state;
    if (availMask == FileLinux::INVALID && client) {
	LinuxEnv sc; //Linux environment object
	availMask = ttydev_poll(oli);
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
LinuxPTYServer::recvfrom(struct iovec *vec, uval veclen, uval flags,
			 char *addr, uval &addrLen, GenState &moreAvail, 
			 void *controlData, uval &controlDataLen,
			 __XHANDLE xhandle)
{
    addrLen = 0;

    controlDataLen = 0; /* setting to zero, since no control data */

    AutoLock<LockType> al(&_lock);
    ClientData *cd = getClientData(xhandle);
    SysStatusUval rc = 0;


    ProcessLinux::LinuxInfo info;
    ProcessID pid = XHandleTrans::GetOwnerProcessID(xhandle);
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoNativePid(pid, info);
    tassertMsg(_SUCCESS(rc),
	       "Couldn't get info about k42 process 0x%lx\n", pid);

    PTYTaskInfo ptyTI(info.pid, info.pgrp, info.session, info.tty,
		      info.pid == info.session);

    LinuxEnv sc(SysCall, ptyTI.t);
    rc = 0;
    for (uval i = 0; i<veclen; ++i) {
	int nr = ttydev_read(oli, (char*)vec[i].iov_base,
			     vec[i].iov_len, &availMask);
	if (nr < 0) {
	    if (rc>0) break;
	    if (nr==-EWOULDBLOCK) break;
	    rc = _SERROR(2495, 0, -nr);
	    break;
	}
	rc += nr;
    }
    availMask = ttydev_poll(oli);
    calcAvailable(moreAvail);
    cd->setAvail(moreAvail);
    return rc;
}

/* virtual */ SysStatusUval
LinuxPTYServer::sendto(struct iovec* vec, uval veclen, uval flags,
		       const char *addr, uval addrLen, GenState &moreAvail, 
		       void *controlData, uval controlDataLen,
		       __XHANDLE xhandle)
{
    tassertMsg((controlDataLen == 0), "oops\n");
    AutoLock<LockType> al(&_lock);
    ClientData *cd = getClientData(xhandle);
    SysStatusUval rc = 0;
    ProcessLinux::LinuxInfo info;
    ProcessID pid = XHandleTrans::GetOwnerProcessID(xhandle);
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoNativePid(pid, info);
    tassertMsg(_SUCCESS(rc),
	       "Couldn't get info about k42 process 0x%lx\n", pid);

    PTYTaskInfo ptyTI(info.pid, info.pgrp, info.session, info.tty,
		      info.pid == info.session);
    LinuxEnv sc(SysCall, ptyTI.t);
    rc = 0;
    for (uval i = 0; i<veclen; ++i) {
	int nr = ttydev_write(oli, (char*)vec[i].iov_base,
			      vec[i].iov_len, &availMask);
	if (nr < 0) {
	    if (rc>0) break;
	    if (nr==-EWOULDBLOCK) break;
	    rc = _SERROR(2367, 0, -nr);
	    break;
	}
	rc += nr;
    }
    availMask = ttydev_poll(oli);
    calcAvailable(moreAvail);
    cd->setAvail(moreAvail);
    return rc;

}

/* virtual */ SysStatus
LinuxPTYServer::_getStatus(struct stat &status)
{
    status.st_dev	= MINOR(devNum);	// FIXME unix major
    status.st_ino	= 0;
    status.st_mode	= S_IFCHR+0666;
    status.st_nlink	= 1;
    status.st_uid	= 0;
    status.st_gid	= 0;
    status.st_rdev	= (MAJOR(devNum) << 8) | MINOR(devNum);	
    status.st_size	= 0;
    status.st_blksize	= 0x1000;
    status.st_blocks	= 0;	// FIXME
    status.st_atime	= 0;	// FIXME
    status.st_ctime	= 0;	// FIXME
    status.st_mtime	= 0;	// FIXME
    return 0;
}


extern "C" int k42devpts_pty_new(int line, dev_t device, void *token);
extern "C" void devpts_pty_kill(int number);
extern int sprintf(char *buf, const char *fmt, ...);

int
k42devpts_pty_new(int line, dev_t device, void *token)
{
    SysStatus rc = 0;
    // Opening a pty requires us to register it --
    // it may just have come into existence and it may become a
    // controlling tty for some process. "token" is in fact
    // the pointer to the tty_struct object.
    if (MAJOR(device) >= UNIX98_PTY_SLAVE_MAJOR  &&
	MAJOR(device) < UNIX98_PTY_SLAVE_MAJOR
	+ UNIX98_PTY_MAJOR_COUNT) {
	ptsEntries->acquireLock();

	ObjectHandle oh;
	TTYEntry *te;
	if (ptsEntries->locked_find(line, te)) {
	    tassertMsg(0,"Multiple add of same tty?\n");
	    ptsEntries->releaseLock();
	    return 0;
	}

	//tassertMsg(ret==0,"couldn't get pty token\n");
	rc = DREFGOBJ(TheProcessLinuxRef)->addTTY(line, (uval)token);
	tassertMsg(_SUCCESS(rc), "addTTY failed: %lx\n",rc);

	char buf[16];
	sprintf(buf, "%d", line);
	getDevFSOH(ptsDir, oh);
	ObjectHandle devOH;
	void *devData;
	LinuxEnvSuspend();
	rc = CreateDevice(oh, buf, S_IFCHR | 0555,
			  UNIX98_PTY_SLAVE_MAJOR,
			  line, devOH, devData);
	LinuxEnvResume();
	if (_SUCCESS(rc)) {
	    te = new TTYEntry;
	    te->oh = devOH;
	    te->token = token;
	    ptsEntries->locked_add(line, te);
	}

	ptsEntries->releaseLock();
    }
    return 0;
}

void
devpts_pty_kill(int number)
{
    TTYEntry *te;
    if (ptsEntries->remove(number, te)) {
	Obj::AsyncReleaseAccess(te->oh);
	delete te;
    }
}

extern "C" struct tty_struct *devpts_get_tty(int number);
struct tty_struct *devpts_get_tty(int number)
{
    struct tty_struct *t = NULL;
    ptsEntries->acquireLock();

    TTYEntry *te;
    if (ptsEntries->locked_find(number, te)) {
	t = (struct tty_struct*)te->token;
    }
    ptsEntries->releaseLock();
    return t;
}
