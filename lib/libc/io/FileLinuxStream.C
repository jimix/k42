/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxStream.C,v 1.120 2005/07/15 17:14:22 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinux.H"
#include "FileLinuxStream.H"
#include "FileLinuxStreamTTY.H"
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubStreamServerPipe.H>
#include <stub/StubStreamServerSocket.H>
#include <sync/Lock.H>
#include <sys/time.h>
#include "io/StreamServer.H"
#include <stub/StubStreamServer.H>
#include <trace/traceIO.h>
#include <sys/ProcessLinuxClient.H>
#include <misc/AutoList.I>
#include <linux/ioctl.h>
#include <linux/termios.h>
#include <stdio.h>
#include "emu/FD.H"


/* virtual */ SysStatusUval
TransVirtStream::recvmsg(struct msghdr &msg, uval flags, GenState &moreAvail,
			 void *controlData, uval &controlDataLen)
{
    uval addrLen = msg.msg_namelen;
    SysStatusUval rc;
    rc = stub._recvfromVirt(msg.msg_iov, msg.msg_iovlen, flags,
			    (char*)msg.msg_name,
			    addrLen, moreAvail);
    controlDataLen = 0; /* setting to zero, since no control data */
    if (_SUCCESS(rc)) {
	msg.msg_namelen = addrLen;
    }
    return rc;
}

/* virtual */ SysStatusUval
TransVirtStream::sendmsg(struct msghdr &msg, uval flags, uval startOffset,
			 GenState &moreAvail, void *controlData,
			 uval controlDataLen)
{

    uval addrLen = msg.msg_namelen;
    SysStatusUval rc;
    rc = stub._sendtoVirt(msg.msg_iov, msg.msg_iovlen, flags,
			  (char*)msg.msg_name, addrLen, moreAvail);
    return rc;
}

/* virtual */ SysStatusUval
TransPPCStream::recvmsg(struct msghdr &msg, uval flags, GenState &moreAvail, 
			void *controlData, uval &controlDataLen)
{
    uval addrLen = msg.msg_namelen;
    SysStatusUval rc;
    uval bytesLeft = vecLength(msg.msg_iov, msg.msg_iovlen);
    uval totalLen = bytesLeft;
    uval offset = 0;
    uval currVec = 0;
    uval received = 0;
    uval loopiter = 0;

    do {
	char* addr = ((char*)uval(msg.msg_iov[currVec].iov_base) + offset);
	uval len = MIN(PPCPAGE_LENGTH_MAX-1024,
		       msg.msg_iov[currVec].iov_len-offset);

	bytesLeft = totalLen;
	if (loopiter == 0) {
	    // get any control data on first iteration through loop
	    rc = stub._recvfrom(addr, len, flags, (char*)msg.msg_name,
				addrLen, bytesLeft, moreAvail, 
				(char *)controlData,
				controlDataLen);
	} else {
	    char *tmpcontrolData = NULL;
	    uval tmpcontrolDataLen = 0;
	    rc = stub._recvfrom(addr, len, flags, (char*)msg.msg_name,
				addrLen, bytesLeft, moreAvail, 
				(char *)tmpcontrolData,
				tmpcontrolDataLen);

	}
	loopiter++;

	offset+= _SGETUVAL(rc);

	if (offset == msg.msg_iov[currVec].iov_len) {
	    offset = 0;
	    ++currVec;
	}
	if (_SUCCESS(rc) ) {
	    received += _SGETUVAL(rc);
	    if (addrLen) {
		msg.msg_namelen = addrLen;
		addrLen = 0;
	    }
	}
    } while (_SUCCESS(rc) && bytesLeft>0 && currVec<msg.msg_iovlen);
    if (_SUCCESS(rc)) {
	rc = _SRETUVAL(received);
    }
    return rc;
}

/* virtual */ SysStatusUval
TransPPCStream::sendmsg(struct msghdr &msg, uval flags, uval startOffset,
			GenState &moreAvail, void *controlData,
			uval controlDataLen)
{
    uval bytesLeft = vecLength(msg.msg_iov, msg.msg_iovlen);
    uval totalLen = bytesLeft;
    SysStatusUval rc;
    uval currVec = 0;
    uval offset = 0;
    do {
	char* addr = (char*)(uval(msg.msg_iov[currVec].iov_base) + offset);
	uval len = MIN(PPCPAGE_LENGTH_MAX-1024,
		       msg.msg_iov[currVec].iov_len-offset);
	// Real I/O happens only when bytesLeft == len,
	// until then we're just copying data over
	if (bytesLeft == len) {
	    rc = stub._sendto(addr, len, flags, (char*)msg.msg_name,
			      msg.msg_namelen, bytesLeft, moreAvail,
			      (char*)controlData, controlDataLen);
	} else {
	    rc = stub._sendto(addr, len, flags, (char*)msg.msg_name,
			      0, bytesLeft, moreAvail,
			      NULL/*controlData*/, 0/*controlDataLen*/);
	}

//	tassertMsg(_FAILURE(rc) || _SGETUVAL(rc)==len||_SGETUVAL(rc)==totalLen,
//		   "write only partial success: %lx %lx\n",rc, len);
	if (bytesLeft == len) {
	    offset = totalLen;
	    bytesLeft = 0;
	} else {
	    bytesLeft -= _SGETUVAL(rc);
	    offset+= _SGETUVAL(rc);
	}
	if (offset == msg.msg_iov[currVec].iov_len) {
	    offset = 0;
	    ++currVec;
	}
    } while (_SUCCESS(rc) && bytesLeft && currVec < msg.msg_iovlen);

    tassertMsg(bytesLeft==0 || _FAILURE(rc), "bytesLeft not 0: %lx %lx\n",
	       bytesLeft, rc);

    return rc;
}

FileLinuxStream::FDTransferData::FDTransferData()
    : entryOffsetIndexSize(0),
      entryOffsetIndex(NULL)
{
    dataBlockSize = 50;
    dataBlock = (char *)allocGlobal(dataBlockSize);
    numEntries = (uval *)dataBlock;
    *numEntries = 0;
    dataBlockUsed = sizeof(uval);
}

FileLinuxStream::FDTransferData::FDTransferData(char *data)
    : entryOffsetIndexSize(0), entryOffsetIndex(NULL)
{
    numEntries = (uval *)data;

    entryOffsetIndex = (uval *)allocGlobal(sizeof(uval)* *numEntries);
    passertMsg(entryOffsetIndex!=NULL, "allocGlobal failed\n");
    uval offset = sizeof(uval);
    EntryLayout *el;
    for (uval i=0; i<*numEntries; i++) {
	entryOffsetIndex[i] = offset;
	el = (EntryLayout *) (data+offset);
	offset += el->entrySize;
    }
    entryOffsetIndexSize = *numEntries;
    dataBlockSize = dataBlockUsed = offset;

    // Take a copy of the data as we don't know how long it will
    // hang around
    dataBlock = (char *)allocGlobal(dataBlockSize);
    passertMsg(dataBlock!=NULL, "allocGlobal failed\n");
    memcpy(dataBlock, data, dataBlockSize);
    numEntries = (uval *)dataBlock;
}

FileLinuxStream::FDTransferData::~FDTransferData()
{
    freeGlobal(dataBlock, dataBlockSize);
    if (entryOffsetIndex) {
	freeGlobal(entryOffsetIndex, entryOffsetIndexSize);
    }
}

uval
FileLinuxStream::FDTransferData::getNumEntries() const
{
    return *numEntries;
}

FileLinuxStream::FDTransferData::EntryLayout *
FileLinuxStream::FDTransferData::getEntry(uval entryNum)
{
    passertMsg(entryNum<*numEntries, "Invalid entry requested");

    if (entryOffsetIndexSize==0) {
	passertMsg(entryOffsetIndex==NULL, "entryOffsetIndex should be NULL");

	// Need to build the entryOffsetIndex
	entryOffsetIndex = (uval *)allocGlobal(sizeof(uval)* *numEntries);
	passertMsg(entryOffsetIndex!=NULL, "allocGlobal failed\n");
	uval offset = sizeof(uval);
	EntryLayout *el;
	for (uval i=0; i<*numEntries; i++) {
	    entryOffsetIndex[i] = offset;
	    el = (EntryLayout *) (dataBlock+offset);
	    offset += el->entrySize;
	}
	entryOffsetIndexSize = *numEntries;
    }

    return (EntryLayout *)(dataBlock + entryOffsetIndex[entryNum]);
}

void
FileLinuxStream::FDTransferData::addEntry(char *entryDataBlock,
					  uval entryDataBlockSize,
					  ObjectHandle &oh,
					  TypeID dataForType)
{
    while (dataBlockUsed+sizeof(EntryLayout)+entryDataBlockSize
	   > dataBlockSize) {
	uval newBlockSize = dataBlockSize*2;
	char *newDataBlock = (char *)allocGlobal(newBlockSize);
	passertMsg(newDataBlock!=NULL, "Could not allocate %li bytes\n",
		   newBlockSize);
	memcpy(newDataBlock, dataBlock, dataBlockUsed);
	freeGlobal(dataBlock, dataBlockSize);
	dataBlock = newDataBlock;
	dataBlockSize = newBlockSize;
	numEntries = (uval *) dataBlock;
    }

    EntryLayout *el = (EntryLayout *)(dataBlock + dataBlockUsed);
    el->entrySize = sizeof(EntryLayout) + entryDataBlockSize;
    el->dataForType = dataForType;
    el->oh = oh;

    memcpy(el->rawData, entryDataBlock, entryDataBlockSize);


    dataBlockUsed += el->entrySize;
    (*numEntries)++;

    // Invalidate entryOffsetIndex if it exists
    if (entryOffsetIndex) {
	freeGlobal(entryOffsetIndex, sizeof(uval)*entryOffsetIndexSize);
	entryOffsetIndex = NULL;
	entryOffsetIndexSize = 0;
    }
}

SysStatus
FileLinuxStream::FDTransferData::getDataBlock(char **data,
					      uval *blockSize)
{
    *data = dataBlock;
    *blockSize = dataBlockUsed;
    return 0;
}

uval
FileLinuxStream::FDTransferData::getDataBlockSize() const
{
    return dataBlockUsed;
}

/* virtual */ SysStatus
FileLinuxStream::lock()
{
    objLock.acquire();
    return 0;
}

/* virtual */ SysStatus
FileLinuxStream::unLock()
{
    objLock.release();
    return 0;
}

/* virtual */ SysStatus
FileLinuxStream::locked_flush(uval release)
{
    IOBufReg *be = wBufList.getHead();
    int rc;
    GenState moreAvail;

    while (be && !be->getRefs()) {
	RegBufInfo *bi = be->getInfo();

	struct iovec vec = {bi->getBase(), bi->getOff()};
	struct msghdr msg = {NULL, 0, &vec, 1, 0, 0, 0};
	char *controlData = NULL;
	uval controlDataLen = 0;
	rc = locked_sendmsg(msg, 0, NULL, moreAvail, controlData, 
			    controlDataLen);

	wBufList.remove(be);
	deleteIOBufReg(be);
	be = wBufList.getHead();
	if (_FAILURE(rc)) {
	    return rc;
	}
    }

    return 0;
}

SysStatus
FileLinuxStream::flush()
{
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    return locked_flush();
}

void
FileLinuxStream::init(ObjectHandle stubOH, FileLinuxRef useThis)
{
    /*
     * initially set avalable to invalid, since data may have
     * arrived in the interval between creating the socket connection
     * to the server and the _registerCallback.  If so, we missed
     * the first callback.
     */
    FileLinux::init(useThis);
    objLock.init();
    statusUpdater = NULL;
    available.state = FileLinux::INVALID;
    mth = NULL;

    // Only create a stubholder for valid OH's
    if (stubOH.valid()) {
	stubHolder = new TransPPCStream(stubOH);
    }
}

/*static*/ SysStatus
FileLinux::Pipe(FileLinuxRef& newPipeR, FileLinuxRef& newPipeW)
{
    return FileLinuxStream::Pipe(newPipeR, newPipeW);
}

/*static*/ SysStatus
FileLinuxStream::Pipe(FileLinuxRef& newPipeR, FileLinuxRef& newPipeW)
{
    ObjectHandle stubOHR, stubOHW;
    SysStatus rc;

    // get server read and write object handles
    rc = StubStreamServerPipe::_Create(stubOHR, stubOHW);
    if (_FAILURE(rc)) return rc;

    rc = Create(newPipeR, stubOHR, O_RDONLY);
    if (_FAILURE(rc)) return rc;

    rc = Create(newPipeW, stubOHW, O_WRONLY);
    return rc;
}

/* static */ SysStatus
FileLinuxStream::SocketPair(FileLinuxRef &newSocket1,
			    FileLinuxRef &newSocket2,
			    uval domain, uval type, uval protocol)
{
    ObjectHandle stubOH1, stubOH2;
    SysStatus rc;

    rc = StubStreamServerSocket::_CreateSocketPair(stubOH1, stubOH2);
    if (_FAILURE(rc)) return rc;

    rc = Create(newSocket1, stubOH1, O_RDONLY|O_WRONLY);
    if (_FAILURE(rc)) return rc;

    rc = Create(newSocket2, stubOH2, O_RDONLY|O_WRONLY);
    return rc;
}

/*virtual*/ SysStatus
FileLinuxStream::dup(FileLinuxRef& newfile)
{
    SysStatus rc;
    ObjectHandle copyOH;

    rc = getStub(StubStreamServer*,stubHolder)
	->_dup(copyOH,
	       DREFGOBJ(TheProcessRef)->getPID());
    if (_FAILURE(rc)) return rc;

    rc = Create(newfile, copyOH, openFlags);
    return rc;
}

/*virtual*/ SysStatus
FileLinuxStreamTTY::dup(FileLinuxRef& newfile)
{
    SysStatus rc;
    ObjectHandle copyOH;
    rc = giveAccessByClient(copyOH, DREFGOBJ(TheProcessRef)->getPID());
    if (_FAILURE(rc)) return rc;

    rc = FileLinuxStreamTTY::Create(newfile, copyOH, openFlags);
    return rc;
}

SysStatusUval
FileLinuxStream::locked_readAlloc(uval len, char * &buf, ThreadWait **tw)
{
    GenState moreAvail;
    tassert((len > 0), err_printf("bad length to salloc\n"));
    // first check if there is data returned by previous realloc
    if (rBufList.getHead()) {
	if (rBufList.getHead()->getCnt()) {
	    IOBufReg *be = rBufList.getHead();
	    len = MIN(len,be->getCnt());
	    be->adjPtrCnt(len);
	    be->incRefs();

	    buf = be->getPtr();
	    return len;
	}
    }

    IOBufReg *be = createIOBufReg(len);
    struct iovec vec = {be->getBase(), len};
    struct msghdr msg = {NULL, 0, &vec, 1, 0, 0, 0};
    char *controlData = NULL;
    uval controlDataLen = 0;
    SysStatusUval rc = locked_recvmsg(msg, 0, NULL, moreAvail, controlData,
				      controlDataLen);
    if (_SUCCESS(rc) && (_SGETUVAL(rc)>0)) {
	len = MIN(_SGETUVAL(rc),len);

	RegBufInfo *bi = be->getInfo();
	bi->setOff(len);
	bi->setRefs(1);
	bi->setCnt(0);

	rBufList.insert(be);

	buf = be->getBase();
	return len;
    }
    // read failed
    deleteIOBufReg(be);
    return rc;
}

SysStatusUval
FileLinuxStream::locked_readRealloc(char *prev, uval oldlen, uval newlen,
				    char * &buf, ThreadWait **tw)
{
    IOBufReg *be = rBufList.search(prev);
    if (!be) {
	return _SERROR(1040, 0, EINVAL);
    }

    // pushing data back into stream ?
    if (newlen < oldlen) {
	tassert((prev+oldlen==be->getPtr()),
		err_printf("only support realloc at end, cp=%p, oend=%p\n",
			   prev+oldlen, be->getPtr()));

	// note, change is negative
	be->adjPtrCnt((sval)newlen-(sval)oldlen);
	buf=prev;
	return newlen;
    }
    tassertWrn(0, "NYI, call Karina\n");
    return _SERROR(1045, 0, ENOSYS);
}

SysStatus
FileLinuxStream::locked_readFree(char *ptr)
{
    IOBufReg *be = rBufList.search(ptr);
    if (!be) return _SERROR(1042, 0, EINVAL);

    // only destroy if no refs and no cnt of valid data
    if ((!be->decRefs() && !(be->getCnt()))) {
	rBufList.remove(be);
	deleteIOBufReg(be);
    }

    return 0;
}

SysStatusUval
FileLinuxStream::readAlloc(uval len, char * &buf, ThreadWait **tw)
{
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    return locked_readAlloc(len, buf, tw);
}

SysStatus
FileLinuxStream::readFree(char *ptr)
{
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    return locked_readFree(ptr);
}

SysStatusUval
FileLinuxStream::locked_readAllocAt(uval, uval, FileLinux::At, char *&,
				    ThreadWait **tw)
{
    return _SERROR(1043, 0, EPERM);
}

SysStatusUval
FileLinuxStream::readAllocAt(uval,  uval, FileLinux::At, char * &,
			     ThreadWait **tw)
{
    return _SERROR(1044, 0, EPERM);
}

SysStatusUval
FileLinuxStream::locked_writeAlloc(uval len, char * &buf, ThreadWait **tw)
{
    tassert((len > 0), err_printf("bad length to salloc\n"));
    IOBufReg *be = createIOBufReg(len);

    // initialization
    RegBufInfo *bi = be->getInfo();
    bi->setCnt((be->getInfo())->getLen());
    bi->setOff(len);
    bi->setRefs(1);

    wBufList.append(be);
    buf = be->getBase();
    return len;
}

SysStatusUval
FileLinuxStream::locked_writeRealloc(char *prev, uval oldlen, uval newlen,
			      char * &buf, ThreadWait **tw)
{
    IOBufReg *be = wBufList.search(prev);
    if (!be) return _SERROR(1500, 0, EINVAL);

    // can only realloc if last request in buffer
    if (be->getPtr() != (((char *)prev)+oldlen)) {
	return _SERROR(1501, 0, EINVAL);
    };

    if (newlen>oldlen) {
	// the case checked in assertion should be handled,
	// increase length of buffer or allocate larger buffer and copy
	tassert((be->getCnt()<=(newlen-oldlen)), err_printf("woops\n"));
	be->adjPtrCnt(newlen-oldlen);
	buf=prev;
	return newlen;
    }

    // am reducing length of allocated buffer, can be negative
    be->adjPtrCnt((sval)newlen-(sval)oldlen);
    buf=prev;
    return newlen;
}

SysStatus
FileLinuxStream::locked_writeFree(char *ptr)
{
    SysStatus rc = 0;
    IOBufReg *be = wBufList.search(ptr);
    if (!be)
	return _SERROR(1035, 0, EINVAL);

    if ((!be->decRefs()) && (be == wBufList.getHead())) {
	// else, wait for all prev refcnt to 0 to be flushed
	rc = locked_flush();
    }
    return rc;
}

SysStatusUval
FileLinuxStream::locked_writeAllocAt(uval, uval, FileLinux::At,
				     char *&, ThreadWait **tw)
{
    return _SERROR(1038, 0, EPERM);
}

SysStatusUval
FileLinuxStream::writeAlloc(uval len, char * &buf, ThreadWait **tw)
{
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    return locked_writeAlloc(len, buf, tw);
}

SysStatus
FileLinuxStream::writeFree(char *ptr)
{
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    return locked_writeFree(ptr);
}

SysStatusUval
FileLinuxStream::writeAllocAt(uval,  uval, FileLinux::At, char *&,ThreadWait**)
{
    return _SERROR(1502, 0, EPERM);
}

SysStatus
FileLinuxStream::getStatus(FileLinux::Stat *status)
{
    SysStatus rc;
    
    rc = getStub(StubStreamServer*,stubHolder)->_getStatus(*status);
    return rc;
}

// FIXME: once original postFork is out of the code base, we can leave
// this one as postFork. It's only used on crtInit.C (for objects
// TheConsole and TheTTY)
/* virtual */ SysStatusUval
FileLinuxStream::FIXMEpostFork(ObjectHandle oh)
{
    SysStatus rc;
    stubHolder->setOH(oh);

    if (mth && mth->mtr) {
	DREF(mth->mtr)->postFork();
	DREF(mth->mtr)->detach();
	mth = NULL;
    }

    available.state = FileLinux::INVALID;

    rc = registerCallback();
    tassertMsg(_SUCCESS(rc), "failed _registerCallback() %lx\n", rc);

    return 0;
}

/* virtual */ SysStatus
FileLinuxStream::lazyGiveAccess(sval fd)
{
    LazyReOpenData data;
    getStateTransferData((char *)&data);
    // call server to transfer to my process
    SysStatus rc= getStub(StubStreamServer*,stubHolder)->_lazyGiveAccess(fd,
					     FileLinux_STREAM, -1,
					     (char *)&data,
					     sizeof(LazyReOpenData));
    tassertMsg(_SUCCESS(rc), "?");

    // Detach should destroy --- return 1
    rc = detach();
    tassertMsg(_SUCCESS(rc) && _SGETUVAL(rc)==0, "detach failure %lx\n",rc);

    return rc;
}
/* static */ SysStatus
FileLinuxStream::LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh, char *buf,
			    uval bufLen)
{
    SysStatus rc;
    LazyReOpenData *d = (LazyReOpenData *)buf;
    tassertMsg((bufLen == sizeof(LazyReOpenData)), "got back bad len\n");
    rc = Create(flRef, oh, d->openFlags);
    return rc;
}

/* virtual */ void
FileLinuxStream::getStateTransferData(char *transferData)
{
    LazyReOpenData *data = (LazyReOpenData *)transferData;
    data->transType = stubHolder->transType;
    data->openFlags = openFlags;
}

/* virtual */ uval
FileLinuxStream::getStateTransferDataSize() const
{
    return sizeof(LazyReOpenData);
}

/* virtual */ SysStatus
FileLinuxStreamTTY::lazyGiveAccess(sval fd)
{
    LazyReOpenData data;
    data.openFlags = openFlags;
    data.transType = stubHolder->transType;
    // call server to transfer to my process
    return getStub(StubStreamServer*,stubHolder)->_lazyGiveAccess(fd,
				      FileLinux_TTY, -1, (char *)&data,
				      sizeof(LazyReOpenData));
}

/* static */ SysStatus
FileLinuxStreamTTY::LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh, char *buf,
			       uval bufLen)
{
    //err_printf("In FileLinuxStreamTTY::LazyReOpen\n");
    SysStatus rc;
    LazyReOpenData *d = (LazyReOpenData *)buf;
    tassertMsg((bufLen == sizeof(LazyReOpenData)), "got back bad len\n");
    rc = FileLinuxStreamTTY::Create(flRef, oh, d->openFlags);
    //err_printf("Out FileLinuxStreamTTY::LazyReOpen\n");
    return rc;
}

/* virtual */ SysStatus
FileLinuxStreamTTY::getStatus(FileLinux::Stat *status)
{
   SysStatus rc;
   rc = getStub(StubStreamServer*,stubHolder)->_getStatus(*status);
   
   char name[100];
   memset(name, 0, 100);
   sprintf(name, "/dev/pts/%lu", status->st_dev);
   rc = FileLinux::GetStatus(name, status);
   return rc;
}

/* virtual */ SysStatus
FileLinuxStream::debugAvail(GenState &raw, GenState &avail) {
    AutoLock<LockType> al(&objLock);
    avail = available;
    return getStub(StubStreamServer*,stubHolder)->_getAvailability(raw);
}

SysStatus
FileLinuxStream::checkAvail(ThreadWait **tw, uval condition)
{
    SysStatus rc;
    ThreadNotif *tn = NULL;
    _ASSERT_HELD(objLock);
    if (available.state & FileLinux::INVALID) {
	GenState av;
        // in initialization
        rc = getStub(StubStreamServer*,stubHolder)->_getAvailability(av);
        tassert(_SUCCESS(rc), err_printf("woops %016lx\n",rc));
        setAvailable(av);
        tassert ((!(available.state & FileLinux::INVALID)),
		 err_printf("still invalid\n"));
    }

    if (!(available.state & condition) && (tw != NULL)) {
	waiters.lock();
	if (!(available.state & (condition|FileLinux::DESTROYED))) {
	    tn = new ThreadNotif(condition);
	    waiters.lockedPrepend(tn);
	    *tw = tn;
	}
	waiters.unlock();
    }
    if (available.state & condition) {
	return 1;
    }
    return 0;
}

SysStatus
FileLinuxStream::registerCallback()
{
    SysStatus rc;
    ObjectHandle tmpOH;

    rc = giveAccessByServer(tmpOH, stubHolder->getPid());
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    return getStub(StubStreamServer*,stubHolder)->_registerCallback(tmpOH);
}


SysStatusUval
FileLinuxStream::locked_recvmsg(struct msghdr &msg, uval flags,
				ThreadWait **tw,
				GenState &moreAvail,
				void *controlData, uval &controlDataLen)
{
    SysStatus rc=0;
    uval nbytes = vecLength(msg.msg_iov, msg.msg_iovlen);
  retry:
    rc = checkAvail(tw, READ_AVAIL|ENDOFFILE);

    if (rc != 1) { //Error or condition not satisfied
	moreAvail = available;
	return rc;
    }

    if (!(available.state & FileLinux::READ_AVAIL)) {
	moreAvail = available;
	return 0;
    }

    rc = stubHolder->recvmsg(msg, flags, moreAvail, controlData, 
			     controlDataLen);

    if (_SUCCESS(rc)) {
	setAvailable(moreAvail);
	if (_SGETUVAL(rc)==0 && tw && nbytes) { goto retry; }
    } else {
	//Check for MemTrans error and consider it back-pressure
	if (_SCLSCD(rc)==MemTrans::MemTransErr && mth && mth->mtr) {
	    tassertMsg(0,"Yielding for pages...\n");
	    //This needs to hook up to its own ThreadWait pointer and return
	    //to allow the thread to wait for pages
	    goto retry;
	}
    }
    return rc;
}

SysStatusUval
FileLinuxStream::locked_sendmsg(struct msghdr &msg, uval flags,
				ThreadWait **tw, GenState &moreAvail,
				void *controlData, uval controlDataLen)
{
    SysStatus rc;
    moreAvail = available;
    uval nbytes = vecLength(msg.msg_iov, msg.msg_iovlen);

  retry:
    rc = checkAvail(tw, WRITE_AVAIL);

    if (rc != 1) { //Error or condition not satisfied
        moreAvail = available;
	return rc;
     }

    rc = stubHolder->sendmsg(msg, flags, 0, moreAvail, controlData,
			     controlDataLen);

    if (_SUCCESS(rc)) {
	setAvailable(moreAvail);
	if (_SGETUVAL(rc)==0 && tw && nbytes) { goto retry; }
    } else {
	//Check for MemTrans error and consider it back-pressure
	if (_SCLSCD(rc)==MemTrans::MemTransErr && mth && mth->mtr) {
	    mth->pokeRing(mth->mtr,mth->smtXH);
	    goto retry;
	}
    }
    return rc;
}

struct cmsghdr *
__cmsg_nxthdr_foo (struct msghdr *__mhdr, struct cmsghdr *__cmsg)
{
  if ((size_t) __cmsg->cmsg_len < sizeof (struct cmsghdr))
    /* The kernel header does this so there may be a reason.  */
    return 0;

  __cmsg = (struct cmsghdr *) ((unsigned char *) __cmsg
			       + CMSG_ALIGN (__cmsg->cmsg_len));
  if ((unsigned char *) (__cmsg + 1) > ((unsigned char *) __mhdr->msg_control
					+ __mhdr->msg_controllen)
      || ((unsigned char *) __cmsg + CMSG_ALIGN (__cmsg->cmsg_len)
	  > ((unsigned char *) __mhdr->msg_control + __mhdr->msg_controllen)))
    /* No more entries.  */
    return 0;
  return __cmsg;
}


/* virtual */ SysStatusUval
FileLinuxStream::sendmsg(struct msghdr &msg, uval flags,
			 ThreadWait **tw, GenState &moreAvail)
{
    SysStatusUval rc=0;

    AutoLock<LockType> al(&objLock); // locks now, unlocks on return

    if (msg.msg_control && msg.msg_controllen>0) {
	struct cmsghdr *cmsg;
	sval32 fd;
	FileLinuxRef fileRef;
	ObjectHandle ohToSend;
	TypeID ohToSendType;

	/* We have control data */

	cmsg = CMSG_FIRSTHDR(&msg);
	passertMsg(cmsg!=NULL, "Should have at least one ancillary message\n");


	FDTransferData transferData;

	while (cmsg) {
	    if (cmsg->cmsg_level != SOL_SOCKET
		|| cmsg->cmsg_type != SCM_RIGHTS) {
		err_printf("Unknown ancillary message: level %i, type: %i\n",
			   cmsg->cmsg_level, cmsg->cmsg_type);
	    } else {
		/* We are sending a file descriptor */
		const uval numFDsToSend =
		    (cmsg->cmsg_len - CMSG_ALIGN(sizeof(struct cmsghdr)))/sizeof(int);
		for (uval i=0; i<numFDsToSend; i++) {

		    fd = *(((sval32 *)CMSG_DATA(cmsg))+i);

		    fileRef = _FD::GetFD(fd);
		    tassertMsg(fileRef!=NULL,
			       "Attempt to send invalid fd %i\n", fd);

		    /* Give access to the StreamSocketServer */
		    rc = DREF(fileRef)->giveAccessByClient(ohToSend,
							   stubHolder->getPid());
		    tassertRC(rc,
			    "Failed to giveAccess to StubStreamServer for FD\n");

		    rc = DREF(fileRef)->getType(ohToSendType);
		    passertRC(rc, "Failed to get type of fd to send\n");

		    uval stateDataSize =
			DREF(fileRef)->getStateTransferDataSize();
		    char *stateData = (char *)allocGlobal(stateDataSize);
		    passertMsg(stateData!=NULL, "stateData alloc failed\n");

		    DREF(fileRef)->getStateTransferData(stateData);
		    transferData.addEntry(stateData, stateDataSize,
					  ohToSend, ohToSendType);
		    freeGlobal(stateData, stateDataSize);
		}
	    }
	    cmsg = __cmsg_nxthdr_foo(&msg, cmsg);
	}

	if (transferData.getNumEntries() > 0) {
	    char *data;
	    uval dataBlockSize;
	    rc = transferData.getDataBlock(&data, &dataBlockSize);
	    tassertMsg(_SUCCESS(rc),
		       "Failed to generate transfer data block\n");

	    rc = locked_sendmsg(msg, flags, tw, moreAvail, data, dataBlockSize);
	    tassertMsg(_SUCCESS(rc),
		       "Failed to send OH to StubStreamServer\n");
	} else {
	    err_printf("Odd, no entries\n");
	}
    } else {
	char *controlData = NULL;
	uval controlDataLen = 0;
	rc = locked_sendmsg(msg, flags, tw, moreAvail, controlData,
			    controlDataLen);
    }
    // Really need  to restore msg control data
    return rc;
}

/* virtual */ SysStatusUval
FileLinuxStream::recvmsg(struct msghdr &msg, uval flags,
			 ThreadWait **tw, GenState &moreAvail)
{
    SysStatusUval rc, bytesReceived;
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return

    char controlData[128];
    uval controlDataLen = 128;
    
    bytesReceived = locked_recvmsg(msg, flags, tw, moreAvail, controlData, 
				   controlDataLen);

    if (_SUCCESS(bytesReceived) && (_SGETUVAL(bytesReceived) > 0) && 
	controlDataLen) {
	// got control data
	FDTransferData transferData(controlData);
	FDTransferData::EntryLayout *el;
	struct cmsghdr *cmsg;
	int newFD;
	FileLinuxRef newFile;
	
	cmsg = CMSG_FIRSTHDR(&msg);
	passertMsg(cmsg!=NULL, "Not enough room to put FD in\n");
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	uval i;
	
	for (i=0; i<transferData.getNumEntries(); i++) {
	    el = transferData.getEntry(i);
	    rc = FileLinux::Create(newFile,
				   el->dataForType,
				   el->oh,
				   el->rawData);
	    passertMsg(_SUCCESS(rc), "Failed to create new file\n");
	    
	    newFD = _FD::AllocFD(newFile);
	    cmsg->cmsg_len = CMSG_LEN(sizeof(newFD));
	    *(((int*)CMSG_DATA(cmsg))+i) = newFD;
	    
	}
	cmsg->cmsg_len = CMSG_LEN(sizeof(int)*i);
	msg.msg_controllen = CMSG_SPACE(sizeof(newFD)*i);
    }
	
    return bytesReceived;
}


/* virtual */ SysStatusUval
FileLinuxStream::readv(struct iovec *vec, uval vecCount,
		       ThreadWait **tw, GenState &moreAvail)
{
    AutoLock<LockType> al(&objLock);
    char *controlData = NULL;
    uval controlDataLen = 0;
    struct msghdr msg = {NULL, 0, vec, vecCount, 0, 0, 0};
    return locked_recvmsg(msg, 0, tw, moreAvail, controlData, controlDataLen);
}


/* virtual */ SysStatusUval
FileLinuxStream::writev(const struct iovec* vec, uval vecCount,
			ThreadWait **tw, GenState &moreAvail)
{
    AutoLock<LockType> al(&objLock);
    char *controlData = NULL;
    uval controlDataLen = 0;
    struct msghdr msg = {NULL, 0, (struct iovec*)vec, vecCount, 0, 0, 0};
    return locked_sendmsg(msg, 0, tw, moreAvail, controlData, controlDataLen);
}

/* virtual */ SysStatus
FileLinuxStream::_signalDataAvailable(GenState bits)
{
//    err_printf("FL Notification: %p %04x %04x\n",this,available,bits);
    if (isClosedExportedXObjectList()) {
	// object already destroyed, this async call bypassed destruction
	return 0;
    }
    TraceOSIORcvUpcall(stubHolder->getOH().xhandle(), bits.fullVal);
    setAvailable(bits);
    return 0;
}

/* virtual */ void
FileLinuxStream::setAvailable(GenState avail)
{
    GenState current;

    TraceOSIOUpdate(0, avail.fullVal);

    if (!available.setIfNewer(avail)) {
	return;
    }

    do {
	if (!CompareAndStore((volatile uval*)&statusUpdater,
			    (uval)NULL, (uval)Scheduler::GetCurThreadPtr())) {
	    // Some thread is already doing this.
	    return;
	}
	SyncAfterAcquire();

	waiters.lock();

	current = available;

	IONotif *node = (IONotif*) waiters.next();
	FileLinuxRef ref = (FileLinuxRef)getRef();

	while (node) {
	    IONotif *next = (IONotif*) node->next();

	    node->available = current;

	    if (node->condition & current.state) {
		node->ready(ref, current.state);

		if (!(node->flags & IONotif::Persist)) {
		    node->lockedDetach();
		    if (node->flags & IONotif::DelMe) {
			delete node;
		    }
		}
	    }

	    node = next;
	}

	waiters.unlock();

	SyncBeforeRelease();
	statusUpdater = NULL;

    } while (available.isNewer(current));
}

/* virtual */ SysStatus
FileLinuxStream::notify(IONotif *comp)
{
    KeyedNotif *kn = NULL;
    return _notifyKeyed(comp, kn);
}

/* virtual */ SysStatus
FileLinuxStream::notifyKeyed(KeyedNotif* kn, KeyedNotif* &existing)
{
    return _notifyKeyed(kn, existing);
}

/* virtual */ SysStatus
FileLinuxStream::_notifyKeyed(IONotif *ion, KeyedNotif* &existing)
{
    SysStatus rc = 0;

    if (available.state & FileLinux::INVALID) {
	GenState av;
        rc = getStub(StubStreamServer*,stubHolder)->_getAvailability(av);
	tassertMsg(_SUCCESS(rc), "woops: %lx\n",rc);
        setAvailable(av);
        tassertMsg(!(available.state & FileLinux::INVALID), "still invalid\n");
    }

    waiters.lock();

    existing = NULL;
    IONotif *node;
    IONotif *next = (IONotif*) waiters.next();
    if (ion->flags & IONotif::Keyed) {
	KeyedNotif *kn = (KeyedNotif*)ion;
	while ((node=next)) {
	    next = (IONotif*) node->next();
	    if (node->flags & IONotif::Keyed) {
		KeyedNotif *x = (KeyedNotif*)node;
		if (x->key == kn->key) {
		    existing = x;
		    existing->lockedDetach();
		    break;
		}
	    }
	}
    }

    GenState current;
    current = available;
    ion->available = current;

    if (current.state & ion->condition) {
	ion->ready((FileLinuxRef) getRef(), current.state);
    }

    if (((current.state & ion->condition) &&
				!(ion->flags & IONotif::Persist)) ||
	    (current.state & FileLinux::DESTROYED)) {
	if (ion->flags & IONotif::DelMe) {
	    delete ion;
	}
    } else {
	waiters.lockedPrepend(ion);
    }

    waiters.unlock();

    return (existing ? 1 : 0);
}



void
FileLinuxStream::locked_completeDestruction()
{
    delete stubHolder;
    destroyUnchecked();
}

/* virtual */
FileLinuxStream::~FileLinuxStream()
{
    waiters.lock();
    IONotif *node;
    while ((node = (IONotif*) waiters.next())) {
	node->available = available;
	if (node->condition & FileLinux::DESTROYED) {
	    node->ready((FileLinuxRef) NULL, available.state);
	}
	node->lockedDetach();
	if (node->flags & IONotif::DelMe) {
	    delete node;
	}
    }
    waiters.unlock();
}

// FIXME: modify destroy so invokations after/concurrent with
// destruction will return error, this is wrong now
/* virtual */ SysStatus
FileLinuxStream::destroy()
{
    available.setInFuture(FileLinux::DESTROYED);

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    lock();
    if (mth && mth->mtr) {
	SysStatus rc = DREF(mth->mtr)->detach();
	if (_SUCCESS(rc)) {
	    mth = NULL;
	}
    }

    locked_flush();

    // clear out all the buffers
    IOBufReg *be = rBufList.getHead();
    while (be) {
	rBufList.remove(be);
	deleteIOBufReg(be);
	be = rBufList.getHead();
    }

    getStub(StubStreamServer*,stubHolder)->_releaseAccess();
    locked_completeDestruction();

    unLock();
    return 0;
}

SysStatus
FileLinuxStream::Create(FileLinuxRef &ref, ObjectHandle oh,
			uval oflags, FileLinuxRef useR)
{
    FileLinuxStream *newp;
    SysStatus rc;

    newp = new FileLinuxStream;
    newp->init(oh, useR);
    newp->setFlags(oflags);

    ref = (FileLinuxRef)newp->getRef();

    // create an object handle for upcall from server
    rc = newp->registerCallback();
    tassert(_SUCCESS(rc), err_printf("do cleanup code rc:%016lx\n",rc));

    return rc;
}

void
FileLinuxStream::initSMT()
{

    SysStatus rc;
    MemTransRef mtr;
    XHandle smtXH;
    tassertMsg(objLock.isLocked(),"Called initSMT without lock held\n");

    ProcessID partner = stubHolder->getPid();

 retry:
    rc = MemTrans::GetMemTrans(mtr, smtXH, partner, 1234);

    // if the MemTrans already exists...
    if (_SUCCESS(rc)) {
	    mth = (MTEvents*)DREF(mtr)->getMTEvents();
	    //We're in a race with the thread that actually created the mth,
	    //wait for them to complete the allocRing call (see below).
	    while (!mth->ring) {
		Scheduler::Yield();
	    }
	    return;
    } else if (_SGENCD(rc)==EBUSY) {
	// Object is being deleted, retry
	goto retry;
    }

    // Return on succes, or any "real" errors
    if (_SGENCD(rc)!=ENOENT) {
	return;
    }

    mth = new MTEvents;
    rc = MemTrans::Create(mtr, partner, 64*PAGE_SIZE,
			  mth->smtXH, mth, 1234);

    if (_FAILURE(rc) && _SGENCD(rc)==EALREADY) {
	delete mth;
	mth = NULL;
	goto retry;
    } else {
	mth->mtr = mtr;
    }

    uval size = 0x32;
    rc = DREF(mth->mtr)->allocRing(size, 2, size-2, mth->smtXH);
    tassertMsg(_SUCCESS(rc),"MemTrans::AllocRing: %016lx\n",rc);
}

SysStatus
FileLinuxStream::setsockopt(uval level, uval optname,
			    const void *optval, uval optlen)
{
    SysStatus rc;

    rc = getStub(StubStreamServer*,stubHolder)
	->_setsockopt(level, optname, (char *)optval, optlen);
    return rc;
}

SysStatus
FileLinuxStream::getsockopt(uval level, uval optname,
			    const void *optval, uval *optlen)
{
    SysStatus rc;

    *optlen = 0;
    rc = getStub(StubStreamServer*,stubHolder)
	->_getsockopt(level, optname, (char *)optval, optlen);
    return rc;
}

SysStatus
FileLinuxStreamTTY::init(FileLinuxRef &ref, ObjectHandle oh,
			    uval oflags, FileLinuxRef useR)
{
    SysStatus rc;
    FileLinuxStream::init(oh, useR);
    setFlags(oflags);
    ref = (FileLinuxRef)getRef();

    // create an object handle for upcall from server
    rc = registerCallback();
    tassert(_SUCCESS(rc), err_printf("do cleanup code\n"));
    return rc;
}

SysStatus
FileLinuxStreamTTY::Create(FileLinuxRef &ref, ObjectHandle oh,
			    uval oflags, FileLinuxRef useR)
{
    FileLinuxStreamTTY *newp;
    newp = new FileLinuxStreamTTY;
    return newp->init(ref, oh, oflags, useR);
}

/* virtual */ SysStatus
FileLinuxStream::ioctl(uval request, va_list args)
{
    SysStatus rc;
    switch (request) {
    default:;
	rc = FileLinux::ioctl(request, args);	// no error for now
    }
    return rc;
}

#include <defines/bugs.H>

/* virtual */ SysStatus
FileLinuxStreamTTY::ioctl(uval request, va_list args)
{
    SysStatus rc;
    switch (request) {
    case TIOCSCTTY:
    {
	unsigned int data = va_arg(args, unsigned int);
	uval size = sizeof(unsigned int);
	rc = getStub(StubStreamServer*,stubHolder)->_ioctl(request, size,
							   (char*)&data);
	break;
    }

    case TIOCGPTN:
    {
	unsigned int *data = va_arg(args, unsigned int*);
	uval size = sizeof(unsigned int);
	rc = getStub(StubStreamServer*,stubHolder)->_ioctl(request, size,
							   (char*)data);
	break;
    }
    case TIOCSPTLCK:
    {
	int *data = va_arg(args, int*);
	uval size = sizeof(int);
	rc = getStub(StubStreamServer*,stubHolder)->_ioctl(request, size,
							   (char*)data);
	break;
    }

#define GET(x) x
#define __STRUCTP_IOCTL(request, type, debug)				\
case request: {								\
    if (debug) err_printf("Asking for " #request ": %lx\n", GET(request));\
    type *data = va_arg(args, type*);					\
    uval size = sizeof(type);						\
    rc = getStub(StubStreamServer*,stubHolder)->_ioctl(request, size,	\
						       (char*)data);	\
    break;								\
}
#define STRUCTP_IOCTLDBG(request, type) __STRUCTP_IOCTL(request,type,1)
#define STRUCTP_IOCTL(request, type) __STRUCTP_IOCTL(request,type,0)

    STRUCTP_IOCTL(TCGETS, struct termios);
    STRUCTP_IOCTL(TCSETS, struct termios);
    STRUCTP_IOCTL(TCSETSW, struct termios);
    STRUCTP_IOCTL(TCSETSF, struct termios);
    STRUCTP_IOCTL(TIOCGWINSZ, struct winsize);
    STRUCTP_IOCTL(TIOCSWINSZ, struct winsize);
    STRUCTP_IOCTL(TIOCGPGRP, int);
    STRUCTP_IOCTL(TIOCSPGRP, int);
    default:;
	rc = FileLinuxStream::ioctl(request, args);
    }
    return rc;
}
