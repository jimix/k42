/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxFile.C,v 1.141 2005/08/29 14:13:55 dilma Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinuxFile.H"

#include <cobj/CObjRootSingleRep.H>
#include <stub/StubNameTreeLinux.H>
#include <io/FileLinuxDir.H>

#include <asm/ioctl.h>
#include <sys/ProcessLinuxClient.H>

#include <cobj/XHandleTrans.H>

// needed for checking for ENABLE_SYNCSERVICE
#include <defines/paging.H>

#include <linux/hdreg.h>
#define BLKROSET   _IO(0x12,93)	/* set device read-only (0 = read-write) */
#define BLKROGET   _IO(0x12,94)	/* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#define BLKGETSIZE _IO(0x12,96)	/* return device size */
#define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#define BLKRASET   _IO(0x12,98)	/* Set read ahead for block device */
#define BLKRAGET   _IO(0x12,99)	/* get current read ahead setting */
#define BLKFRASET  _IO(0x12,100)/* set filesystem (mm/filemap.c) read-ahead */
#define BLKFRAGET  _IO(0x12,101)/* get filesystem (mm/filemap.c) read-ahead */
#define BLKSECTSET _IO(0x12,102)/* set max sectors per request (ll_rw_blk.c) */
#define BLKSECTGET _IO(0x12,103)/* get max sectors per request (ll_rw_blk.c) */
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */

FileLinux::IoctlDesc FileLinux::ioctl_table[] = {
    {HDIO_GETGEO, sizeof(struct hd_geometry)},
    {BLKGETSIZE, sizeof(long)},
    {BLKRRPART, 0},
    {BLKSSZGET, sizeof(int)},
    {0,0}
};

// for debugging
//#define DILMA_DEBUG_TRACE_FLF
//#define DILMA_DEBUG_FLF
#ifdef DILMA_DEBUG_TRACE_FLF
#define FLFDEBUG(method) traceFS_ref1_str1(TRACE_FS_DEBUG_1UVAL_1STR, \
                                           (uval) this, method);
#else
#ifdef DILMA_DEBUG_FLF
#define FLFDEBUG(method) err_printf("FLF this %p meth %s\n", this, method);
#else
#define FLFDEBUG(method)
#endif // #ifdef  DILMA_DEBUG_FLF
#endif // #ifdef DILMA_DEBUG_TRACE_FLF

SysStatus
FileLinuxFile::Create(FileLinuxRef &newFile, ObjectHandle toh, uval oflag,
		      uval useType)
{
    FileLinuxFile *newp;
    SysStatus rc;

    tassertMsg((useType == NON_SHARED || useType == SHARED
		|| useType == FIXED_SHARED || useType == LAZY_INIT),
	       "invalid useType\n");

#ifndef LAZY_SHARING_SETUP
    passertMsg(useType != LAZY_INIT, "?");
#endif

    newp = new FileLinuxFile;

    rc = newp->init(toh, oflag, (UseType) useType);

    if (_SUCCESS(rc)) {
	newFile = (FileLinuxRef)CObjRootSingleRep::Create(newp);
    }

#ifndef LAZY_SHARING_SETUP
    switch (useType) {
    case SHARED:
	// FIXME: we should also register it so we get ready to change
	// behavior from SHARED to NON_SHARED
	break;
    case NON_SHARED: {
	newp->lock();
	SysStatus rc = newp->locked_registerCallBackUseType();
	tassertMsg(_SUCCESS(rc), "ops");
	newp->unLock();
	break;
    }
    case FIXED_SHARED:
	passertMsg(0, "can't get here");
	break;
    case LAZY_INIT:
	passertMsg(0, "?");
	break;
    default:
	passertMsg(0, "?");
    }
#endif // #ifndef LAZY_SHARING_SETUP

    //FIXME - cleanup on failure paths?
    return rc;
}

/*virtual*/ SysStatus
FileLinuxFile::dup(FileLinuxRef& newfile)
{
    FLFDEBUG("dup");

    ObjectHandle newoh;
    SysStatus rc;
    uval flength, offset;

    streamLock.acquire();

    if (useType == NON_SHARED && buffer) {
	tassertMsg(callBackUseTypeRegistered == 1, "?");
	// file has been actually used locally; need to update server
	rc = buffer->getLengthOffset(flength, offset);
	tassertMsg(_SUCCESS(rc), "?");
	rc = stub._setLengthOffset(flength, offset);
	tassertMsg(_SUCCESS(rc), "?");
	(void) buffer->afterServerUpdate();
    } else { // offset information irrelevant to server
	offset = uval(~0);
    }

    tassertMsg((useType == SHARED || useType == NON_SHARED ||
		useType == FIXED_SHARED || useType == LAZY_INIT),
	       "how come?\n useType is %ld", (uval) useType);
    uval ut, newUseType;
    ut = (uval) useType;

#ifndef LAZY_SHARING_SETUP
    passertMsg(useType != LAZY_INIT, "?");
    streamLock.release();
#endif

#ifndef LAZY_SHARING_SETUP
    // FIXME: if we don't have the lazy thing in, we don't actually need to
    // send our useType ...
#endif // #ifndef LAZY_SHARING_SETUP

    rc = stub._dup(_SGETPID(DREFGOBJ(TheProcessRef)->getPID()), ut,
		   newUseType, flength, offset, newoh);

#ifdef DEBUG_USE_TYPE
    char name[255];
#ifdef HACK_FOR_FR_FILENAMES
    SysStatusUval rclen = stub._getFileName(name, sizeof(name));
    tassertMsg(_SUCCESS(rclen), "?");
#else
    name[0] = '\0';
#endif // #ifdef HACK_FOR_FR_FILENAMES
    err_printf("FileLinuxFile.C::dup, file %s, pid 0x%lx, gotback newUseType %ld,"
	       " rc 0x%lx\n", name,
	       (uval) _SGETPID(DREFGOBJ(TheProcessRef)->getPID()),
	       newUseType, rc);
#endif // #ifdef DEBUG_USE_TYPE

    _IF_FAILURE_RET(rc);

#ifdef LAZY_SHARING_SETUP
    if (ut == LAZY_INIT) {
	tassertMsg(buffer == NULL, "?");
    }
#endif // #ifdef LAZY_SHARING_SETUP

#ifdef LAZY_SHARING_SETUP
    tassertMsg((newUseType == SHARED || newUseType == NON_SHARED ||
		newUseType == FIXED_SHARED || newUseType == LAZY_INIT),
	       "how come?\n newUseType after _dup is %ld", newUseType);
#else
    tassertMsg(newUseType == SHARED || newUseType == NON_SHARED
	       || newUseType == FIXED_SHARED,
	       "how come?\n newUseType after _dup is %ld (without LAZY_SHARING)",
	       newUseType);
#endif // #ifdef LAZY_SHARING_SETUP

#ifdef LAZY_SHARING_SETUP
    streamLock.release();
#endif // #ifdef LAZY_SHARING_SETUP

    return Create(newfile, newoh, openFlags, newUseType);
}

SysStatus
FileLinuxFile::init(ObjectHandle oh, uval oflag, UseType ut)
{
    FLFDEBUG("init");

    //We don't call the FileLinux::init -- it creates a ref, which
    //this object wants to do for itself.  However, we still have to
    //initialize "waiters" here.
    waiters.init();

    stub.setOH(oh);

    streamLock.init();

    openFlags = oflag;
    useType = ut;

    tassertMsg(callBackUseTypeRegistered==0, "?");

    destroyAckSync = 0;

    return 0;
}

/* virtual */ SysStatus
FileLinuxFile::getType(TypeID &type) {
    type = FileLinux_FILE;
    return 0;
}

/* virtual */ SysStatus
FileLinuxFile::lock()
{
    streamLock.acquire();
    return 0;
}

/* virtual */ SysStatus
FileLinuxFile::unLock()
{
    streamLock.release();
    return 0;
}

/* virtual */ SysStatusUval
FileLinuxFile::locked_readAlloc(uval len, char * &buf, ThreadWait **tw)
{
    FLFDEBUG("locked_readAlloc");

    _ASSERT_HELD(streamLock);

    if (buffer == NULL) locked_initBuffer(len);
    return buffer->readAlloc(len, buf, tw);
}

/* virtual */ SysStatusUval
FileLinuxFile::readAlloc(uval len, char * &buf, ThreadWait **tw)
{
    FLFDEBUG("readAlloc");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    return locked_readAlloc(len, buf, tw);
}

/* virtual */ SysStatusUval
FileLinuxFile::readAllocAt(uval len, uval off, FileLinux::At at,
			   char * &bf, ThreadWait **tw)
{
    FLFDEBUG("readAllocAt");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    return locked_readAllocAt(len, off, at, bf, tw);
}

/* virtual */ SysStatusUval
FileLinuxFile::locked_readFree(char *ptr)
{
    FLFDEBUG("locked_readFree");

    _ASSERT_HELD(streamLock);
    tassertMsg(buffer != NULL, "?");
    return buffer->readFree(ptr);
}

/* virtual */ SysStatus
FileLinuxFile::readFree(char *ptr)
{
    FLFDEBUG("readFree");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    return locked_readFree(ptr);
}

/* virtual */ SysStatusUval
FileLinuxFile::locked_writeAlloc(uval len, char * &buf, ThreadWait **tw)
{
    FLFDEBUG("locked_writeAlloc");

    _ASSERT_HELD(streamLock);

    if (buffer == NULL) locked_initBuffer(len);
    return buffer->writeAlloc(len, buf, tw);
}

/* virtual */ SysStatusUval
FileLinuxFile::writeAlloc(uval len, char * &buf, ThreadWait **tw)
{
    FLFDEBUG("writeAlloc");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    return locked_writeAlloc(len, buf, tw);
}

/* virtual */ SysStatusUval
FileLinuxFile::writeAllocAt(uval len, uval off, FileLinux::At at,
		       char * &bf, ThreadWait **tw)
{
    FLFDEBUG("writeAllocAt");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    return locked_writeAllocAt(len, off, at, bf, tw);
}

/* virtual */ SysStatus
FileLinuxFile::locked_writeFree(char *ptr)
{
    FLFDEBUG("locked_writeFree");

    _ASSERT_HELD(streamLock);

    tassertMsg(buffer != NULL, "?");
    return buffer->writeFree(ptr);
}

/* virtual */ SysStatus
FileLinuxFile::writeFree(char *ptr)
{
    FLFDEBUG("writeFree");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    return locked_writeFree(ptr);
}

/* virtual */ SysStatus
FileLinuxFile::locked_flush(uval release)
{
    FLFDEBUG("locked_flush");

    //err_printf("In locked_flush flength is %ld\n", flength);
    if (useType == NON_SHARED) {
	if (buffer) {
	    tassertMsg(callBackUseTypeRegistered == 1, "?");
	    // flength, offset may have changed
	    // FIXME: should we make sure we only send this if we've really
	    // changed it?
	    uval flength, offset;
	    SysStatus rc = buffer->getLengthOffset(flength, offset);
	    tassertMsg(_SUCCESS(rc), "?");

	    rc = stub._setLengthOffset(flength, offset);
	    tassertMsg(_SUCCESS(rc), "?");
	    (void) buffer->afterServerUpdate();

	} else {
	    // nothing to do
	}
    } else {
#ifdef LAZY_SHARING_SETUP
	tassertMsg((useType == SHARED
		    || useType == FIXED_SHARED || useType == LAZY_INIT),
		   "useType is %ld\n", (uval)useType);
#else
	tassertMsg((useType == SHARED || useType == FIXED_SHARED),
		   "useType is %ld\n", (uval)useType);
#endif // #ifdef LAZY_SHARING_SETUP
    }

    if ((O_ACCMODE & openFlags) != O_RDONLY) {
	if (buffer != NULL) {
	    buffer->flush();
	}
    }

    return 0;
}

/* virtual */ SysStatus
FileLinuxFile::flush()
{
    FLFDEBUG("flush");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    return locked_flush();
}

/* virtual */ SysStatusUval
FileLinuxFile::writev(const struct iovec *vec, uval vecCount,
		      ThreadWait **tw, GenState &moreAvail)
{
    FLFDEBUG("writev");

    SysStatusUval rc;
    uval length_write;
    char* buf_write;
    uval length = vecLength(vec, vecCount);

    lock();
    moreAvail.state = FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL;

    rc = locked_writeAlloc(length, buf_write, tw);
    if (_FAILURE(rc)) {
	unLock();
	return rc;
    }
    length_write = _SGETUVAL(rc);
    if (length_write) {
	memcpy_fromiovec(buf_write, vec, vecCount, length_write);
	locked_writeFree(buf_write);
    }
    unLock();
    return rc;
}

/* virtual */ SysStatusUval
FileLinuxFile::readv(struct iovec* vec, uval vecCount,
		     ThreadWait **tw, GenState &moreAvail)
{
    FLFDEBUG("readv");

    SysStatusUval rc;
    uval length_read;
    char* buf_read;
    uval length = vecLength(vec,vecCount);
    lock();
    moreAvail.state = FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL;
    rc = locked_readAlloc(length, buf_read, tw);
    if (_FAILURE(rc)) {
	unLock();
	//semantics shift from EOF error to 0 length on EOF
	if (_SCLSCD(rc) == FileLinux::EndOfFile) {
	    moreAvail.state = FileLinux::ENDOFFILE;
	    return 0;
	}
	return rc;
    }
    length_read = _SGETUVAL(rc);
    if (length_read) {
	memcpy_toiovec(vec, buf_read, vecCount, length_read);
	locked_readFree(buf_read);
    }
    unLock();
    return rc;
}

/* virtual */ SysStatusUval
FileLinuxFile::pread(const char *buf, uval nbytes, uval offset)
{
    FLFDEBUG("readv");

    SysStatusUval rc, rcret;
    char* buf_read;

    lock();

    // retrieve and save current position
    if (buffer == NULL) locked_initBuffer(uval(~0));
    rc = buffer->setFilePosition(0, FileLinux::RELATIVE);
    _IF_FAILURE_RET(rc);
    uval savedOffset = _SGETUVAL(rc);
    rc = buffer->setFilePosition(offset, FileLinux::ABSOLUTE);
    // tassertMsg only because it didn't fail on the first call, it should
    // work now
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    rcret = locked_readAlloc(nbytes, buf_read, NULL);

    // restore previous file offset
    rc = buffer->setFilePosition(savedOffset, FileLinux::ABSOLUTE);
    // tassertMsg only because it didn't fail on the first 2 calls, it should
    // work now
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    // return value from locked_readAlloc
    if (_FAILURE(rcret)) {
	//semantics shift from EOF error to 0 length on EOF
	if (_SCLSCD(rc) == FileLinux::EndOfFile) {
	    rcret = _SRETUVAL(0);
	}
    } else {
	tassertMsg(_SGETUVAL(rcret) > 0, "?");
	memcpy((void*)buf, buf_read, _SGETUVAL(rcret));
	locked_readFree(buf_read);
    }

    unLock();
    return rcret;
}

/* virtual */ SysStatusUval
FileLinuxFile::pwrite(const char *buf, uval nbytes, uval offset)
{
    FLFDEBUG("readv");

    SysStatusUval rc, rcret;
    char* buf_write;

    lock();

    // retrieve and save current position
    if (buffer == NULL) locked_initBuffer(uval(~0));
    rc = buffer->setFilePosition(0, FileLinux::RELATIVE);
    _IF_FAILURE_RET(rc);
    uval savedOffset = _SGETUVAL(rc);
    rc = buffer->setFilePosition(offset, FileLinux::ABSOLUTE);
    // tassertMsg only because it didn't fail on the first call, it should
    // work now
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    rcret = locked_writeAlloc(nbytes, buf_write, NULL);

    // restore previous file offset
    rc = buffer->setFilePosition(savedOffset, FileLinux::ABSOLUTE);
    // tassertMsg only because it didn't fail on the first 2 calls, it should
    // work now
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    if (_SUCCESS(rcret)) {
	uval length_write = _SGETUVAL(rcret);
	if (length_write) {
	    memcpy(buf_write, (void*)buf, length_write);
	    locked_writeFree(buf_write);
	}
    }

    unLock();
    return rcret;
}

/* virtual */ SysStatusUval
FileLinuxFile::setFilePosition(sval position, At at)
{
    FLFDEBUG("setFilePosition");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return

    if (buffer == NULL) locked_initBuffer(uval(~0));
    return buffer->setFilePosition(position, at);
}

/* virtual */ SysStatus
FileLinuxFile::ftruncate(off_t length)
{
    FLFDEBUG("ftruncate");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return

    if (buffer == NULL) locked_initBuffer(uval(~0));
    return buffer->ftruncate((uval) length);
}

/* virtual */SysStatus
FileLinuxFile::getStatus(FileLinux::Stat *status)
{
    FLFDEBUG("getStatus");

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    SysStatus rc;

#ifdef LAZY_SHARING_SETUP
    if (useType == LAZY_INIT) {
	tassertMsg(callBackUseTypeRegistered == 0, "ops");
	rc = locked_registerCallBackUseType();
	tassertMsg(_SUCCESS(rc), "ops");
	tassertMsg(bufferInitData.fileLength != uval(~0), "?");
	buffer = new Buffer(bufferInitData.fileLength,
			    bufferInitData.initialOffset,
			    openFlags, stub.getOH(), useType, uval(~0));
    }
#else
    passertMsg(useType != LAZY_INIT, "?");
#endif // #ifdef LAZY_SHARING_SETUP

    rc = stub._getStatus(*status);
    _IF_FAILURE_RET(rc);

    if (useType == NON_SHARED) {
#ifndef LAZY_SHARING_SETUP
	if (buffer == NULL) locked_initBuffer(uval(~0));
#endif // #ifndef LAZY_SHARING_SETUP
	uval flength, dummy;
	rc = buffer->getLengthOffset(flength, dummy);
	status->st_size = flength;
    }

    return rc;
}

/* virtual */ SysStatus
FileLinuxFile::detach()
{
    FLFDEBUG("detach");
    return FileLinux::detach();
}

/* virtual */ SysStatus
FileLinuxFile::destroy()
{
    SysStatus rc;

    FLFDEBUG("destroy");
    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return
    //err_printf("Started destroy() ref %p xhandle %lx\n",
    //	       getRef(), (uval) stub.getOH().xhandle());
    uval64 oldDestroyAck = FetchAndOr64(&destroyAckSync, 2);
    if (oldDestroyAck & 1) {
	uval interval = 2000;		// do this in cycles, not time
	// FIXME: this is an ugly stolen code ... get from other place
	while ((destroyAckSync & 1) && interval < 20000000UL) {
	    Scheduler::DelayUntil(interval, TimerEvent::relative);
	    interval = interval*10;	// back off to avoid livelock
	}
	tassertMsg((destroyAckSync & 1)==0, "waiting was not enough?!\n");
    }


    {   // remove all ObjRefs to this object
	SysStatus rc = closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return (_SCLSCD(rc) == 1) ? 0 : rc;
    }

#ifndef ENABLE_SYNCSERVICE
    rc = locked_flush();
    tassertMsg(_SUCCESS(rc), "?");
#endif

    delete buffer;

#ifndef NDEBUG
    /* We assert that no locks are held here because the below
     * _releaseAccess() call is going to result in the locks being
     * irretrievably lost.  Remove this assertion as soon as the lock
     * lossage is fixed.
     */
    struct flock currLock;
    rc = getLock(currLock);
    if (rc == 0) {
	tassertWrn(currLock.l_type == F_UNLCK,
		   "We are about to lose our advisory file locks\n");
    }
#endif

    rc = stub._releaseAccess();
    tassertMsg(_SUCCESS(rc), "not dealing with this error\n");

    if (callBackUseTypeRegistered == 1) {
	rc = DREF(callBackUseTypeObjRef)->destroy();
	tassertMsg(_SUCCESS(rc), "destroy of callBackUseTypeObjRef failed\n");
    }

    // FIXME: potentially leaking callBackLockObjRef

    destroyUnchecked();
    return 0;
}

/* virtual */ SysStatus
FileLinuxFile::ioctl(uval request, va_list args)
{
    FLFDEBUG("ioctl");

    uval i = 0;
    while (ioctl_table[i].number && ioctl_table[i].number!= request) {
	++i;
    }

    if (!ioctl_table[i].number) {
	return FileLinux::ioctl(request, args);
    }

    uval size = ioctl_table[i].arg_size;

    char* x = va_arg(args, char*);
    SysStatus rc = stub._ioctl(request, x, size);
    return rc;
}

/* virtual */ SysStatus
FileLinuxFile::lazyGiveAccess(sval fd)
{
    SysStatus rc;

    FLFDEBUG("lazyGiveAccess");

    streamLock.acquire();

    LazyReOpenData data;
    data.openFlags = openFlags;
    data.useType = useType;
    //err_printf("in lazyGiveAccess useType is %ld\n", useType);
    // call server to transfer to my process
    rc = stub._lazyGiveAccess(fd, FileLinux_FILE, -1,
			      (char *)&data, sizeof(LazyReOpenData));
    tassertMsg(_SUCCESS(rc), "?");
    streamLock.release();

    // Detach should destroy --- return 1
    rc = detach();
    tassertMsg(_SUCCESS(rc) && _SGETUVAL(rc)==0, "detach failure %lx\n",rc);

    return rc;
}

/* static */ SysStatus
FileLinuxFile::LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh, char *buf,
			  uval bufLen)
{
    SysStatus rc;
    LazyReOpenData *d = (LazyReOpenData *)buf;
    tassertMsg((bufLen == sizeof(LazyReOpenData)), "got back bad len\n");

#ifdef LAZY_SHARING_SETUP
    if (d->useType != FIXED_SHARED) {
	d->useType = LAZY_INIT;
    }
#else
    passertMsg(d->useType != LAZY_INIT, "?");
#endif // #ifdef LAZY_SHARING_SETUP

    rc = Create(flRef, oh, d->openFlags, d->useType);
    return rc;
}

/* virtual */ void
FileLinuxFile::getStateTransferData(char *transferData)
{
    LazyReOpenData *data = (LazyReOpenData *)transferData;
    data->openFlags = openFlags;
    data->useType = useType;
}

/* virtual */ uval
FileLinuxFile::getStateTransferDataSize() const 
{
    return sizeof(LazyReOpenData);
}

SysStatus
FileLinuxFile::setLockWait(struct flock &fileLock)
{
    FLFDEBUG("setLockWait");

    SysStatusUval rc;
    uval key;
    BlockedThreadQueues::Element qe;

    if (CompareAndStore(&callBackLockRegistered, 0, 1)) {
	CallBackLock::Create(callBackLockObjRef);
	registerCallBackLock();
    }

    rc = stub._setLockWait(fileLock, key);

    if (_SUCCESS(rc) &&	_SGETUVAL(rc) == EAGAIN) {
	tassertMsg(key > 0, "Bad Key\n");

	ThreadID tid = Scheduler::GetCurThread();

	DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(
	    &qe, (void *)&fileLock);

	DREF(callBackLockObjRef)->add(key, tid);

	// lock is held and we are to waiting
	Scheduler::Block();

	if (ProcessLinuxClient::SyscallSignalsPending()) {
	    // FIXME: pssible unblock and signal is pending
	    rc = _SERROR(2519, 0, EINTR);
	    tassertWrn(0, "FileLock was interrupted by signal.\n");
	}
	DREF(callBackLockObjRef)->remove(key, tid);

	DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(
	    &qe, (void *)&fileLock);
    }

    return rc;
}

SysStatusUval /* __async */
FileLinuxFile::CallBackLock::_callBack(__in uval arg, __XHANDLE xhandle)
{
    ThreadID tid;

    // FIXME: is there a find and remove or do I have to lock this myself?
    if (callBackList.find(arg, tid)) {
	Scheduler::Unblock(tid);
    }
    return 0;
}

SysStatus
FileLinuxFile::registerCallBackLock(void)
{
    FLFDEBUG("registerCallBackLock");

    SysStatus rc;
    ObjectHandle callbackOH;

    rc = DREF(callBackLockObjRef)->giveAccessByServer(callbackOH,
						      stub.getPid());
    tassertMsg(_SUCCESS(rc), "giveAccess Failed\n");

    if (_SUCCESS(rc)) {
	uval foo1, foo2, foo3;
	rc = stub._registerCallback(callbackOH, FileLinux::LOCK_CALL_BACK,
				    foo1, foo2, foo3);
	tassertMsg(_SUCCESS(rc),
		   "error register callback rc=(%ld,%ld,%ld)\n",
		   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
    }
    return rc;
}

/* static */ SysStatus
FileLinuxFile::CallBackLock::Create(CallBackLockRef &ref)
{
    CallBackLock *rep = new CallBackLock;
    if (rep == NULL) {
	return _SERROR(2066, 0, ENOMEM);
    }

    ref = (CallBackLockRef)CObjRootSingleRep::Create(rep);
    if (ref == NULL) {
	delete rep;
	return _SERROR(2067, 0, ENOSPC);
    }
    return 0;
}

/* static */ SysStatus
FileLinuxFile::CallBackUseType::Create(CallBackUseTypeRef &ref,
				       FileLinuxFileRef f,
				       volatile uval64 *addr)
{
    CallBackUseType *rep = new CallBackUseType(f, addr);
    if (rep == NULL) {
	return _SERROR(2211, 0, ENOMEM);
    }

    ref = (CallBackUseTypeRef)CObjRootSingleRep::Create(rep);
    if (ref == NULL) {
	delete rep;
	return _SERROR(2212, 0, ENOSPC);
    }
    return 0;
}

SysStatus
FileLinuxFile::locked_registerCallBackUseType()
{
    FLFDEBUG("locked_registerCallBackUseType");

     _ASSERT_HELD(streamLock);

    tassertMsg(callBackUseTypeRegistered==0, "registered already?\n");

#ifdef LAZY_SHARING_SETUP
    tassertMsg((useType == LAZY_INIT),
	       "invalid useType %ld\n", (uval) useType);
#else
    // FIXME: in the near future we should have SHARED objects registering
    // themselves also!
    tassertMsg(useType == NON_SHARED, "invalid useType %ld\n", (uval) useType);
#endif // #ifdef LAZY_SHARING_SETUP

    SysStatus rc;
    ObjectHandle callbackOH;

    rc = CallBackUseType::Create(callBackUseTypeObjRef,
				 (FileLinuxFileRef) getRef(), &destroyAckSync);
    tassertMsg(_SUCCESS(rc), "Create failed\n");

    rc = DREF(callBackUseTypeObjRef)->giveAccessByServer(callbackOH,
						      stub.getPid());
    tassertMsg(_SUCCESS(rc), "giveAccess Failed\n");

    if (_SUCCESS(rc)) {
	uval flen, offset;
	uval ut = useType;
	tassertMsg(stub.getOH().valid(), "stub not valid\n");
	rc = stub._registerCallback(callbackOH, FileLinux::USETYPE_CALL_BACK,
				    ut, flen, offset);
	tassertMsg(_SUCCESS(rc),
		   "error register callback rc=(%ld,%ld,%ld)\n",
		   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	passertMsg((ut == NON_SHARED || ut == SHARED), "ut %ld", (uval) ut);
	tassertMsg(flen != uval(~0), "flen %ld\n", flen);

	if (ut == SHARED) {
	    tassertMsg(buffer == NULL, "?");
	    bufferInitData.fileLength = flen;
	    // not setting up offset because it'll get from the server anyway?
	} else {
#ifdef LAZY_SHARING_SETUP
	    if (useType == NON_SHARED) {
		// actually this is not possible ...
		passertMsg(0, "impossible\n");
	    } else {
		tassertMsg(useType == LAZY_INIT, "was %ld\n",
			   (uval) useType);
		tassertMsg(buffer == NULL, "?");
		bufferInitData.initialOffset = offset;
		bufferInitData.fileLength = flen;
	    }
#else
	    tassertMsg(buffer == NULL, "?");
	    bufferInitData.initialOffset = offset;
	    bufferInitData.fileLength = flen;
#endif // #ifdef LAZY_SHARING_SETUP
	}
	useType = (UseType) ut;
    } else {
	tassertMsg(0, "????");
    }

    callBackUseTypeRegistered = 1;

    return rc;

}

SysStatusUval /* __async */
FileLinuxFile::CallBackUseType::_callBack(__in uval arg, __XHANDLE xh)
{
    FLFDEBUG("_callBack for UseType");

    /* FIXME: so far only accepting call backs to go from exclusive to shared.
     * We have only one argument in the interface and we need to deal with
     * two types of requests (CALLBACK_REQUEST_INFO and
     * CALLBACK_REQUEST_SWITCH). Once we use the hot swapping infrastructure
     * we'll eliminate this problem */
    tassertMsg((arg == CALLBACK_REQUEST_INFO ||arg == CALLBACK_REQUEST_SWITCH),
	       "unexpected arg (%ld)\n", arg);

    /* check if destruction is going on and indicate that _callBack ack is
     * needed: set last bit */
    uval64 oldDestroyAck = FetchAndOr64(destroyAckSyncAddr, 1);
    if (oldDestroyAck > 1) {
	// destruction is already going on; nothing to do
#ifdef DILMA_DEBUG_SWITCH
	err_printf("_callBack can't proceed: object being destroyed "
		   "value %lld\n flref %p", oldDestroyAck, flref);
#endif //#ifdef DILMA_DEBUG_SWITCH
	return 0;
    }

    SysStatus rc = DREF(flref)->processCallBack(arg);
    tassertMsg(_SUCCESS(rc), "ops");

    // restore last bit in destroyAckSync to 0
    AtomicAnd64(destroyAckSyncAddr, uval(~1));

    return 0;
}

/* virtual */ SysStatus
FileLinuxFile::CallBackUseType::destroy()
{
    // remove all ObjRefs to this object
    SysStatus rc = closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if (_FAILURE(rc)) return (_SCLSCD(rc) == 1) ? 0 : rc;

    destroyUnchecked();
    return 0;
}

/* virtual */ SysStatus
FileLinuxFile::processCallBack(uval request)
{
    FLFDEBUG("processCallBack");

    SysStatus rc;

    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return

    uval flen, off;
#ifndef LAZY_SHARING_SETUP
    flen = uval(~0);
#endif // #ifndef LAZY_SHARING_SETUP

    if (request == CALLBACK_REQUEST_SWITCH) {
	useType = SHARED;
#ifndef LAZY_SHARING_SETUP
	if (buffer) {
	    // FIXME: for now only from exclusive to shared
	    buffer->switchToShared(flen, off);
	}
#else
	// FIXME: for now only from exclusive to shared
	buffer->switchToShared(flen, off);
#endif // #ifndef LAZY_SHARING_SETUP
    } else {
#ifndef LAZY_SHARING_SETUP
	if (buffer) {
	    tassertMsg(request == CALLBACK_REQUEST_INFO, "req is %ld\n",
		       (uval) request);
	    rc = buffer->getLengthOffset(flen, off);
	    tassertMsg(_SUCCESS(rc), "?");
	}
#else
	tassertMsg(request == CALLBACK_REQUEST_INFO, "req is %ld\n",
		   (uval) request);
	rc = buffer->getLengthOffset(flen, off);
	tassertMsg(_SUCCESS(rc), "?");
#endif // #ifndef LAZY_SHARING_SETUP
    }

    tassertMsg(callBackUseTypeRegistered == 1, "how come? it is %ld\n",
	       callBackUseTypeRegistered);
    tassertMsg(stub.getOH().valid(), "stub not valid\n");

#ifdef DEBUG_USE_TYPE
#ifdef HACK_FOR_FR_FILENAMES
    char name[255];
    SysStatusUval rclen = stub._getFileName(name, sizeof(name));
    tassertMsg(_SUCCESS(rclen), "?");
    err_printf("processCallBack(%s) for request %ld returning flen %ld off %ld\n",
	       name, request, flen, off);
#endif // #ifdef HACK_FOR_FR_FILENAMES
#endif // #ifdef DEBUG_USE_TYPE

    rc = stub._ackUseTypeCallBack(request, flen, off);
    tassertMsg(_SUCCESS(rc), "how? ref %p\n", getRef());

    return 0;
}

