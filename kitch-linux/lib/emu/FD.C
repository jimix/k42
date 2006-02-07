/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FD.C,v 1.103 2005/07/15 17:14:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FD (file descriptor) implementation for linux
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/poll.h>
#include <io/FileLinux.H>
#include <io/IOForkManager.H>
#include <scheduler/Scheduler.H>
#include "FD.H"
#include "linuxEmul.H"
#include <trace/traceIO.h>
#include <misc/AutoList.I>
#include <io/FileLinuxStreamTTY.H>
#include <io/FileLinuxSocketInet.H>
#include <io/FileLinuxDir.H>
#include <io/FileLinuxFile.H>
#include <io/FileLinuxDevNull.H>
#include <io/FileLinuxDevZero.H>
#include <io/FileLinuxVirtFile.H>

/*static*/ _FD *_FD::FileTable;

struct _FD::PollBlockElem :public AutoListNode {
    volatile ThreadID waiter;
    // if == 0  -> not unblocked
    // if < 0   -> error code to return
    // if > 0   -> num fd's ready
    SysStatusUval retcode;
    uval32 max;
    uval32 waker;
    pollfd *pfd;
};
// Object that performs notifications of state changes for poll()
class _FD::PollNotif: public IONotif {
public:
    struct FDRecord *fdrec;
    int fd;
    DEFINE_GLOBAL_NEW(PollNotif);
    PollNotif (struct FDRecord *fdr, int f):
	IONotif (FileLinux::ALL & ~FileLinux::DESTROYED),
	fdrec(fdr), fd(f) {
	flags = IONotif::Persist|IONotif::DelMe;
    }

    virtual void ready(FileLinuxRef fl, uval state) {
	_FD::SetPollStatus(fd, fdrec, state);
    };
    virtual void close();
    virtual ~PollNotif () {};
};

#define SMALL_POLL_SIZE	2
struct SmallPollOp {
    DEFINE_GLOBAL_NEW(SmallPollOp);
    ThreadID thread;
    volatile SysStatusUval* result;
    volatile uval refCount;
    void release() {
	//Decrement ref count
	uval count = FetchAndAdd(&refCount, uval(-1LL));
	if (count == 1) {
	    //We were last decrementer
	    delete this;
	}
    }
};
struct SmallPollNotif: public KeyedNotif {
    DEFINE_GLOBAL_NEW(SmallPollNotif);

    SmallPollOp *spo;
    struct pollfd *pfd;
    SmallPollNotif (SmallPollOp *op, struct pollfd *pf):
	KeyedNotif (FileLinux::DESTROYED|FileLinux::ENDOFFILE|pf->events,
		    uval(Scheduler::GetCurThread())),
	spo(op), pfd(pf) {
	flags |= IONotif::DelMe;
    }
    ~SmallPollNotif () {
	release();
    }
    void release() {
	SmallPollOp *x = (SmallPollOp*)Swap((uval*)&spo,0ULL);
	//Decrement ref count
	if (x) x->release();
    }
    virtual void ready(FileLinuxRef fl, uval state) {

	// Atomically clear spo->result, if it is not NULL.
	volatile SysStatusUval *addr = NULL;
	do {
	    addr = spo->result;
	    if (!addr) break;
	} while (!CompareAndStoreVolatile((uval*)&spo->result,
					  uval(addr), 0ULL));

	if (addr && spo->thread != Scheduler::NullThreadID) {
	    ThreadID thr = spo->thread;
	    spo->thread = Scheduler::NullThreadID;

	    // File is being close, file descriptor not valid
	    if (unlikely(state&FileLinux::DESTROYED)) {
		*addr = _SERROR(2671, 0, EINVAL);
	    } else {
		// Change *addr last; thread may be waiting for it
		// to change
		pfd->revents = state;
		*addr = _SRETUVAL(1);
	    }
	    Scheduler::Unblock(thr);
	}

	release();
    }
};


/* static */ SysStatusUval
_FD::Poll(struct pollfd *fds, uval numfds, sval &timeout) {
    return FileTable->poll(fds, numfds, timeout);
//    return FileTable->smallPoll(fds, numfds, timeout);
}

SysStatusUval
_FD::smallPoll(struct pollfd *fds, uval numfds, sval &timeout)
{
    volatile SysStatusUval rc = 0;
    SysStatus err = 0;
    uval ready = 0;
    SmallPollOp *spo = NULL;
    uval unmatched = 0;

    // There will be at most numfds+1 references to this object
    // 1 per fd being queried plus this function
    // We have to count ourselves as a reference because we need
    // to clear spo->result when we exit, so that if any SmallPollNotif
    // objects are left behind, they can tell that we exited.
    // (So they won't write to memory that used to be in this stack frame.)
    spo = new SmallPollOp;
    spo->refCount = numfds+1;
    spo->result = &rc;
    spo->thread = Scheduler::GetCurThread();

    lock.acquire();
    ready=0;
    for (uval idx = 0 ; idx < numfds; ++idx) {
	uval fd = fds[idx].fd;

	// invalid fd's get marked as INVALID and don't generate an
	// error on the entire call
	if (fd > FD_MAX || fd < 0 || !getFD(fd)) {
	    err = _SERROR(2673, 0, EINVAL);
	    break;
	}

	fds[idx].revents = 0;
	FileLinuxRef fileRef = fdrec[fd].ref;

	// If no good bits set, no need to look at IO object
	// Otherwise we must assume PollNotif object state is stale
	GenState avail;
	avail.state = FileLinux::INVALID;
	if (fdrec[fd].poll) {
	    avail = fdrec[fd].poll->available;
	}
	if (avail.state & fds[idx].events) {
	    DREF(fileRef)->getState(avail);
	    fdrec[fd].poll->available.setIfNewer(avail);
	}

	fds[idx].revents = avail.state;
	// Convert to Linux semantics (EOF-> readable)
	if (fds[idx].revents & FileLinux::ENDOFFILE) {
	    fds[idx].revents |= FileLinux::READ_AVAIL;
	}
	if (fds[idx].revents & fds[idx].events) {
	    ++ready;
	} else if (ready==0) {
	    // Allocate these things from the spnBuf
	    KeyedNotif *spn = new SmallPollNotif (spo, &fds[idx]);
	    KeyedNotif *previous;
	    SysStatus rc2 = DREF(fileRef)->notifyKeyed(spn, previous);
	    tassertMsg(_SUCCESS(rc2), "handle this\n");
	    // There was a SmallPollNotif attached from a previous operation
	    // delete it now.
	    // FIXME: We could re-use the existing SmallPollNotif object
	    //        (requiring a change of semantics for notifyKeyed),
	    //        but this requires handling the races that may occur
	    //        as we convert the object to refer to the local "spo".
	    if (_SGETUVAL(rc2) == 1) {
		delete previous;
	    }
	    unmatched++;

	}
    }

    lock.release();
    // Adjust refCount for spo struct.  We know how many we refs we created
    // by creating SmallPollNotif objects (unmatched), and thus
    // we know that the original refCount was too big (numfds-unmatched).
    //
    if (numfds - unmatched) {
	AtomicAdd(&spo->refCount, ~(numfds - unmatched - 1));
    }

    // We're going to return early, clear spo->result to orphan spo
    if (err || ready) {
	spo->result = NULL;
	// Release the local ref
	spo->release();
    }


    if (err) return err;
    if (ready) return ready;

    // all callbacks set, status of all fds is known
    // but no fd is ready so block

    const SysTime start = Scheduler::SysTimeNow();
    const SysTime tpus = Scheduler::TicksPerSecond() / 1000000;

    timeout = timeout * tpus;
    uval timeToWait = (uval)timeout;
    uval totalWaited = 0;

    while (rc == 0 &&	// if already unblocked
	   (timeToWait!=0) &&
	   !SYSCALL_SIGNALS_PENDING()) {

	SysTime when = ((uval)timeout - totalWaited);

	TraceOSIOSelBlockTo((timeout - totalWaited)/tpus,
		 (uval)start, Scheduler::GetCurThread());

	SYSCALL_DEACTIVATE();

	Scheduler::BlockWithTimeout(when, TimerEvent::relative);

	SYSCALL_ACTIVATE();
	SysTime now = Scheduler::SysTimeNow();

	totalWaited = now - start;

	TraceOSIOSelWoke(
		 Scheduler::GetCurThread(),
		 totalWaited/tpus);

	timeToWait = (uval)timeout - totalWaited;

	if ((sval)timeToWait<0 && timeout>0) {
	    timeToWait = 0;
	} else if (timeout==-1) {
	    timeToWait = ~0ULL;
	}
	tassertWrn(timeout == -1 || totalWaited < (2*((uval)timeout)),
		   "Excessive wait detected: %ld < %ld\n",
		   totalWaited, 2*(uval)timeout);
    }

    // convert back to micro seconds
    timeout = (sval)(timeToWait/tpus);

    // Eliminate reference to stack, release the local ref
    spo->result = NULL;
    spo->release();

    if (rc) return rc;

    if (timeout!=0) {
	// Not timed out, no fd's ready --> signal interruption
	return _SERROR(2922, 0, EINTR);
    }

    return 0;
}


void
_FD::PollNotif::close() {
    _FD::SetPollStatus(fd, fdrec, FileLinux::ENDOFFILE|FileLinux::DESTROYED);
}

void
_FD::FDSet::Sanity()
{
    tassert((BitsPerWord == (sizeof(uval)*8)),
	    err_printf("code assumes can walk through by word\n"));
}

void
_FD::FDSet::setAll()
{
    unsigned int i;
    fd_set *arr = (this);
    for (i = 0; i < sizeof (fd_set) / sizeof (fd_mask); ++i) {
      __FDS_BITS (arr)[i] = ~((fd_mask)0);
    }
}

/*
 * perform fd initialization here
 */

/*
 * called just post exec, state transfered is readyForLazy bit vector
 */
/*static*/ void
_FD::ClassInit(FDSet *fromParent)
{
    // One instance
    FileTable = new _FD;
    /*
     * no need to initialize the fd_list except for cleaner
     * errors.
     */
    FDSet::Sanity();
    FileTable->lock.init();
    FileTable->pollsLock.init();
    FileTable->blockedPolls.init();

    if (fromParent == NULL) {
	FileTable->active.zero();
	FileTable->readyForLazy.zero();
    } else {
	uval fd;
	uval word;
	uval mask;

	fromParent->copyTo(&FileTable->active);
	fromParent->copyTo(&FileTable->readyForLazy);
	// NULL all list elements marked as active to allow check
	// on NULL in getFD

	// FIXME: this can be done faster/better
	for (word=0;word<FDSet::Words;word++) {
	    if ((mask=__FDS_BITS(&(FileTable->active))[word])) {
		fd = word*FDSet::BitsPerWord;
		while (mask) {
		    if (mask&1) {
			FileTable->fdrec[fd].ref = NULL;
			FileTable->fdrec[fd].poll= NULL;
		    }
		    fd++;
		    mask>>=1;
		}
	    }
	}
    }
    FileTable->coe.zero();
}

FileLinuxRef
_FD::locked_lazyGetFD(uval fd)
{
//    err_printf("In _FD::lazyGetFD\n");
    SysStatus rc;
    uval type;
    ObjectHandle oh;
    FileLinuxRef fileRef;
    char buf[512];
    uval dataLen;

    // try again
    if (fdrec[fd].ref) return fdrec[fd].ref;
    if (!active.isSet(fd)) return NULL;

    readyForLazy.clr(fd);

    // go to server
    rc = DREFGOBJ(TheProcessRef)->lazyReOpen(fd, type, oh, buf, dataLen);
    tassertMsg(_SUCCESS(rc), "woops, re-open failed\n");
    switch (type) {
    case FileLinux_STREAM:
	rc = FileLinuxStream::LazyReOpen(fileRef, oh, buf, dataLen);
	break;
    case FileLinux_TTY:
	rc = FileLinuxStreamTTY::LazyReOpen(fileRef, oh, buf, dataLen);
	break;
    case FileLinux_FILE:
	rc = FileLinuxFile::LazyReOpen(fileRef, oh, buf, dataLen);
	break;
    case FileLinux_CHR_NULL:
	rc = FileLinuxDevNull::LazyReOpen(fileRef, oh, buf, dataLen);
	break;
    case FileLinux_CHR_ZERO:
	rc = FileLinuxDevZero::LazyReOpen(fileRef, oh, buf, dataLen);
	break;
    case FileLinux_CHR_TTY:
    case FileLinux_SOCKET:
	rc = FileLinuxSocketInet::LazyReOpen(fileRef, oh, buf, dataLen);
	break;
#if 0
    case FileLinux_VIRT_FILE:
	rc = FileLinuxVirtFile::LazyReOpen(fileRef, oh, buf, dataLen);
	break;
    case FileLinux_DIR:
	rc = FileLinuxDir::LazyReOpen(fileRef, oh, buf, dataLen);
	break;
#endif
    default:
	rc = _SERROR(1472, 0, ENOSYS);
	passertMsg(0, "FIXME object not yet done\n");
	err_printf("no support for this file type\n");
	break;
    }

    passertMsg(_SUCCESS(rc), "?");
    if (_FAILURE(rc)) return NULL;

    /*
     * Current strategy is we put it in table, but keep lazy state
     * in kernel, so that we don't have to do anything on next fork
     * We may in some cases want to do extra work on fork, so
     * in future may need to get rid of kernel state here...
     */

    // actually insert in table
    fdrec[fd].ref = fileRef;
    coe.clr(fd);

//    err_printf("out _FD::lazyGetFD\n");
    return fileRef;
}

/*
 * Remove an FileLinuxRef from the fd_list and return the
 * Ref.
 */
FileLinuxRef
_FD::freeFD(uval fd)
{
    AutoLock<LockType> al(&lock);
    FileLinuxRef closedRef;

    closedRef = locked_getFD(fd);

    // Let poll object actually delete this record, when it's ready
    if (fdrec[fd].poll) {
	fdrec[fd].poll->close();
	//poll object will take care of itself
	fdrec[fd].poll = NULL;
    }
    coe.clr(fd);
    active.clr(fd);

    SysStatus rc;
    if (readyForLazy.isSet(fd)) {
	readyForLazy.clr(fd);
	rc = DREFGOBJ(TheProcessRef)->lazyClose(fd);
	tassertMsg(_SUCCESS(rc), "woops\n");
    }

    return closedRef;
}

SysStatus
_FD::locked_closeFD(int fd)
{
    FileLinuxRef theRef = NULL;

    if (active.isSet(fd) && fdrec[fd].ref) {
	theRef = fdrec[fd].ref;

	if (fdrec[fd].poll) {
	    fdrec[fd].poll->close();
	    // poll object will take care of itself
	    fdrec[fd].poll = NULL;
	}
    }

    active.clr(fd);
    coe.clr(fd);

    SysStatus rc=0;

    if ((!theRef) && (!readyForLazy.isSet(fd))) {
	return _SERROR(1695, 0, EBADF);
    }

    if (readyForLazy.isSet(fd)) {
	readyForLazy.clr(fd);
	rc = DREFGOBJ(TheProcessRef)->lazyClose(fd);
	tassertMsg(_SUCCESS(rc), "woops\n");
    }
//    err_printf("FD::locked_closeFD\n");
    if (theRef) {
//	err_printf("doing detach %lx\n",DREFGOBJ(TheProcessRef)->getPID());
	rc = DREF(theRef)->detach();
//	err_printf("done detach\n");
    }
    return rc;
}

SysStatusUval
_FD::replaceFD(FileLinuxRef& fileLinuxRef, uval fd)
{
    AutoLock<LockType> al(&lock);
    FileLinuxRef oldFileLinuxRef;

    // This checks for dup2() and returns the right errno
    if (fd > FD_MAX) {
	return _SERROR(1695, 0, EMFILE);
    }

    if (active.isSet(fd)) {
	oldFileLinuxRef = locked_getFD(fd);
    } else {
	oldFileLinuxRef = NULL;
	active.set(fd);
    }

    fdrec[fd].ref = fileLinuxRef;
    fileLinuxRef = oldFileLinuxRef;
    if (readyForLazy.isSet(fd)) {
	readyForLazy.clr(fd);
	DREFGOBJ(TheProcessRef)->lazyClose(fd);
    }
    coe.clr(fd);
    return fd;
}

/* does a binary search to find first zero */
static uval
findFirstZeroInWord(uval mask)
{
    uval bit = 0;
#if (_SIZEUVAL == 8)
    if ((mask & 0xffffffffUL) == 0xffffffffUL) {
	// first zero bit in left half
	mask = mask >> 32;
	bit = 32;
    };
#endif /* #if (_SIZEUVAL == 8) */
    if ((mask&0xffffU) == 0xffffU) {
	// first zero bit in left 16 bits
	mask = mask >> 16;
	bit += 16;
    }
    if ((mask&0xffU) == 0xffU) {
	// first zero bit in left 8 bits
	mask = mask >> 8;
	bit += 8;
    }
    if ((mask&0xfU) == 0xfU) {
	// first zero bit in left 4 bits
	mask = mask >> 4;
	bit += 4;
    }
    // mask MUST contain a zero bit
    // so this loop terminates.
    while (mask&1) {
	mask = mask>>1;
	bit += 1;
    }
    return bit;
}

void
_FD::FDSet::copyTo( FDSet *to)
{
    memcpy(to, this, sizeof(FDSet));
}

SysStatusUval
_FD::FDSet::findZero(uval from)
{
    uval word,bit;
    uval mask;

    // ignore all free fds less than from
    word = from / BitsPerWord;
    bit = from % BitsPerWord;
    // mask has one bits for all values in this word less than from
    mask = ((uval(1)<<bit)-1);

    for (;word<Words;word++) {
	if ((uval)(__FDS_BITS(this)[word]) != (uval)(-1)) {
	    // for first word, mask may have some bits set - see above
	    mask |= __FDS_BITS(this)[word];
	    bit = findFirstZeroInWord(mask);
	    return _SRETUVAL(word*BitsPerWord+bit);
	}
	mask = 0;
    } // end of for loop over words
    return _SERROR(1603, 0, EMFILE);
}

SysStatusUval
_FD::FDSet::findMatchBit(FDSet *vec, uval from, uval max)
{
    uval word,bit;
    uval mask, maxWord;

    // ignore all free fds less than from
    word = from/BitsPerWord;
    maxWord = max/BitsPerWord;
    if (maxWord > Words) { maxWord = Words; } // go no farther than last word

    if (from == 0) {
	mask = (uval)(-1);
    } else {
	bit = from % BitsPerWord;
	// mask has one bits for all values in this word >= from
	mask = ~((uval(1)<<bit)-1);
    }

    // make sure not past last word
    maxWord = MIN(FDSet::Words,maxWord);

    for (;word<=maxWord;word++) {
	mask = mask & __FDS_BITS(this)[word] & __FDS_BITS(vec)[word];
	if (mask != (uval)(0)) {
	    // since I already have a binary search for zero, invert mask
	    bit = findFirstZeroInWord(~mask) + word*BitsPerWord;
	    // could be past max
	    if (bit < max) return _SRETUVAL(bit);
	}
	mask = (uval)(-1);
    } // end of for loop over words
    return _SERROR(1604, 0, EMFILE);
}

SysStatusUval
_FD::allocFD(FileLinuxRef fileLinuxRef, uval lowestfd)
{
    AutoLock<LockType> al(&lock);
    SysStatusUval rc;
    uval fd;

    // this is tested here for the F_DUPFD case
    if (lowestfd >= FD_MAX) {
	return _SERROR(1694, 0, EINVAL);
    }

    rc = active.findZero(lowestfd);
    if (_FAILURE(rc)) return rc;	//  why not a SysStatus??

    fd = _SGETUVAL(rc);
    active.set(fd);
    fdrec[fd].ref = fileLinuxRef;
    coe.clr(fd);			// Always set to zero

    return fd;
}

/* For a "fast exec", close all COE fd's */
void
_FD::closeOnExec(void)
{
    AutoLock<LockType> al(&lock);
    uval fd;
    uval word;
    uval mask;
    for (word=0;word<FDSet::Words;word++) {
	if ((mask=__FDS_BITS(&coe)[word])) {
	    fd = word*FDSet::BitsPerWord;
	    while (mask) {
		if (mask&1) {
		    FileLinuxRef fileLinuxRef = fdrec[fd].ref;
		    SysStatus rc;
		    if (readyForLazy.isSet(fd)) {
			readyForLazy.clr(fd);
			rc = DREFGOBJ(TheProcessRef)->lazyClose(fd);
			tassertMsg(_SUCCESS(rc), "woops\n");
		    }


		    if (fileLinuxRef) DREF(fileLinuxRef)->detach();
		    // There can't be any poll's in progress
		    if (fdrec[fd].poll) {
			// poll object will take care of itself
			fdrec[fd].poll = NULL;
		    }
		    fdrec[fd].ref = NULL;
		    active.clr(fd);
		}
		fd++;
		mask>>=1;
	    }
	    __FDS_BITS(&coe)[word]=0;
	}
    }
}

/* no lock needed since the value is either zero or one and
 * holding the lock does not materially change the consequences
 * of a race betweed getCOE and open/close/setCOE
 */
int
_FD::getCOE(uval fd)
{
    return coe.isSet(fd);
}

/*
 * hold the lock to preserve the invarient that an unallocated
 * FD has zero coe and fd
 */
void
_FD::setCOE(uval fd, uval flag)
{
    AutoLock<LockType> al(&lock);
    if (active.isSet(fd)) {
	if (flag) {
	    coe.set(fd);
	} else {
	    coe.clr(fd);
	}
    }
    return;
}

/*
 * this call must be made when external considerations guarantee
 * that all is done. FIXME: called by pty/rlogind for now.
 */
void
_FD::closeAll(void)
{
    AutoLock<LockType> al(&lock);
    uval fd;
    uval word;
    uval mask;
    for (word=0;word<FDSet::Words;word++) {
	if ((mask=__FDS_BITS(&active)[word])) {
	    fd = word*FDSet::BitsPerWord;
	    while (mask) {
		if (mask&1) {
		    locked_closeFD(fd);
		}
		fd++;
		mask>>=1;
	    }
	}
    }
}

SysStatus
_FD::copyFDs(uval coe)
{
    // FIXME: this is not true
    // all other threads have been killed so no locking required
    SysStatus rc;
    uval idx;  		// word index

    for (idx = 0; idx < FDSet::Words; idx++) {
	if (__FDS_BITS(&active)[idx] == 0) {
	    // no open files in this section
	    continue;
	}
	uval bit = 1;
	uval scan = 0;
	uval fd = (idx * FDSet::BitsPerWord);
	uval word = __FDS_BITS(&active)[idx];
	while ((word & ~(scan))) {
	    // bits are set in this word
	    // FIXME optimize this to do a byte at a time
	    if (word & bit) {
		if (coe && getCOE(fd)) { // close this file
		    closeFD(fd);
		}
		if (fdrec[fd].ref && (!readyForLazy.isSet(fd))) {
		    readyForLazy.set(fd);
		    // something to do that have not told process about
		    FileLinuxRef flr = getFD(fd);
		    // tell the file to pass control down to kernel
		    rc = DREF(flr)->lazyGiveAccess(fd);

		    // FIXME: there can't be poll's in progress when we
		    // fork/exec
		    if (fdrec[fd].poll) {
			// poll object will take care of itself
			fdrec[fd].poll = NULL;
		    }
		    fdrec[fd].ref = NULL;
		    tassertMsg(_SUCCESS(rc), "woopsn\n");
		}
	    }
	    scan |= bit;
	    bit = bit << 1;
	    ++fd;
	}
    }
    return 0;
}

SysStatus
_FD::setPollStatus(uval fd, FDRecord* fdr, uval state)
{
    ThreadID id = Scheduler::NullThreadID;

    TraceOSIOSelNotify(fd, state);

    // Convert to Linux semantics (EOF-> readable)
    if (state & FileLinux::ENDOFFILE) {
	state |= FileLinux::READ_AVAIL;
    }

    pollsLock.acquire();
    PollBlockElem *be, *be_next;
    for (be = (PollBlockElem*)blockedPolls.next(); be; be = be_next) {
	be_next = (PollBlockElem*)be->next();

	// locate pollfd for this fd for this particular poll
	uval i = 0;
	while ((uval)be->pfd[i].fd != fd && i<be->max) ++i;
	if (i>=be->max) continue;

	// forced wakeup if state has DESTROYED bit on
	if (state & (be->pfd[i].events|FileLinux::DESTROYED)) {
	    be->pfd[i].revents = state;

	    /*
	     * This unblock is one iteration behind, i.e., we are waking now
	     * the id which we recorded in the previous loop iteration.
	     */
	    if (id!=Scheduler::NullThreadID) {
		TraceOSIOSelWaker(id);
		Scheduler::Unblock(id);
	    }
	    id = be->waiter;

	    if (state & FileLinux::DESTROYED) {
		be->retcode = _SERROR(2670, 0, EINVAL);
	    } else {
		be->waker  = fd;
		be->retcode = 1;
	    }

	    //README!!!!!!
	    //README!!!!!!

	    // Because we remove each "be" that contains a reference
	    // to this file-descriptor, once we're done there will be
	    // no blocked poll()'s that reference "fd" on the list.
	    // Since close() uses this code to kill blocked poll()'s,
	    // there will be no remaining reference to this fd
	    // once we're done  --- thus we'll tolerate any notifications
	    // that may arrive late from the I/O object.
	    be->lockedDetach();
	    be->waiter = Scheduler::NullThreadID;
	}
    }
    pollsLock.release();

    // Finally wake the last id found (typically it's the only one)
    if (id!=Scheduler::NullThreadID) {
	TraceOSIOSelWaker(id);
	Scheduler::Unblock(id);
    }

    return _SRETUVAL(0);
}

SysStatusUval
_FD::poll(struct pollfd *fds, uval numfds, sval &timeout)
{
    SysStatus rc;
    uval ready;

    lock.acquire();

    /*
     * Make one pass through the list to validate the fds and see if any
     * are ready immediately.
     */
    ready = 0;
    for (uval idx = 0 ; idx < numfds; ++idx) {
	uval fd = fds[idx].fd;

	if (fd > FD_MAX || fd < 0 || !locked_getFD(fd)) {
	    lock.release();
	    return _SERROR(2672, 0, EINVAL);
	}

	// If no good bits set, no need to look at IO object
	// Otherwise we must assume PollNotif object state is stale
	GenState avail;
	avail.state = FileLinux::INVALID;
	if (fdrec[fd].poll) {
	    avail = fdrec[fd].poll->available;
	    if (avail.state & fds[idx].events) {
		DREF(fdrec[fd].ref)->getState(avail);
		// Guarantee un-staleness
		fdrec[fd].poll->available.setIfNewer(avail);
	    }
	} else {
	    DREF(fdrec[fd].ref)->getState(avail);
	}

	fds[idx].revents = avail.state;

	// Convert to Linux semantics (EOF -> readable)
	if (fds[idx].revents & FileLinux::ENDOFFILE) {
	    fds[idx].revents |= FileLinux::READ_AVAIL;
	}
	if (fds[idx].revents & fds[idx].events) {
	    ++ready;
	}
    }

    /*
     * We're done if there are ready fds or if the caller doesn't want to wait.
     */
    if ((ready > 0) || (timeout == 0)) {
	lock.release();
	return _SRETUVAL(ready);
    }

    /*
     * Register a PollBlockElem so that we can be awoken.
     */
    PollBlockElem pbe;
    pbe.init();
    pbe.retcode = 0;
    pbe.max = numfds;
    pbe.pfd = fds;
    pbe.waker = (uval32)-1;
    pbe.waiter = Scheduler::GetCurThread();
    pollsLock.acquire();
    blockedPolls.lockedAppend(&pbe);
    pollsLock.release();

    /*
     * Scan the list again, registering PollNotif objects if necessary and
     * checking again for fds that might have become ready.
     */
    ready = 0;
    for (uval idx = 0 ; idx < numfds; ++idx) {
	uval fd = fds[idx].fd;

	if (fdrec[fd].poll == NULL) {
	    PollNotif *pn = new PollNotif (&fdrec[fd],fd);
	    fdrec[fd].poll = pn;
	    rc = DREF(fdrec[fd].ref)->notify(pn);
	    tassertMsg(_SUCCESS(rc), "handle this\n");
	}
	// We updated this state manually up above, so we know
	// it can't have stale bits set.
	fds[idx].revents = fdrec[fd].poll->available.state;

	// Convert to Linux semantics (EOF-> readable)
	if (fds[idx].revents & FileLinux::ENDOFFILE) {
	    fds[idx].revents |= FileLinux::READ_AVAIL;
	}
	if (fds[idx].revents & fds[idx].events) {
	    ++ready;
	}
    }

    lock.release();

    /*
     * If ready fds were detected on the second pass, detach the PollBlockElem
     * and return.  It's okay if the PBE has already been notified.
     */
    if (ready > 0) {
	pollsLock.acquire();
	pbe.lockedDetach();
	pollsLock.release();
	return _SRETUVAL(ready);
    }

    /*
     * All callbacks have been set and status of all fds is known.  It's time
     * to block.
     */

    const SysTime tps = Scheduler::TicksPerSecond();
    const SysTime start = Scheduler::SysTimeNow();
    const SysTime end = (timeout < 0) ? SysTime(-1) :
					    start + (timeout * tps / 1000000);

    SysTime now = start;

    while ((pbe.waiter != Scheduler::NullThreadID) &&
	   (now < end) &&
	   !SYSCALL_SIGNALS_PENDING())
    {
	SYSCALL_DEACTIVATE();

	if (end == SysTime(-1)) {
	    Scheduler::Block();
	} else {
	    Scheduler::BlockWithTimeout(end, TimerEvent::absolute);
	}

	SYSCALL_ACTIVATE();
	now = Scheduler::SysTimeNow();

	TraceOSIOSelWoke(Scheduler::GetCurThread(),
					    (now - start) * 1000000 / tps);

	tassertWrn((now - start) < (end - start) + tps,
		   "Excessive wait detected: timeout %lld, actual wait %lld\n",
		   end - start, now - start);
    }

    if (timeout > 0) {
	// report time remaining
	timeout = (now >= end) ? 0 : (end - now) * 1000000 / tps;
    }

    /*
     * If we've been unblocked, the PBE will have been removed from the list
     * of blocked polls and the retcode field set up for us to return it.
     */
    if (pbe.waiter == Scheduler::NullThreadID) {
	if (_SUCCESS(pbe.retcode)) {
	    for (uval idx = 0; idx < numfds; ++idx) {
		uval fd = fds[idx].fd;
		if (pbe.waker == fd) {
		    fds[idx].revents = fdrec[fd].poll->available.state;
		    if (fds[idx].revents & FileLinux::ENDOFFILE)
			fds[idx].revents |= FileLinux::READ_AVAIL;
		    break;
		}
	    }
	}
	return pbe.retcode;
    }

    /*
     * Detach the PollBlockElem.  It's okay even if the PBE has now been
     * notified.
     */
    pollsLock.acquire();
    pbe.lockedDetach();
    pollsLock.release();

    if (SYSCALL_SIGNALS_PENDING()) {
	return _SERROR(2923, 0, EINTR);	// interrupted
    }

#ifdef DEBUG_POLL
    for (uval idx = 0 ; idx < numfds; ++idx) {
	uval fd = fds[idx].fd;
	GenState raw;
	GenState avail;
	rc = DREF(fdrec[fd].ref)->debugAvail(raw,avail);
	if (_FAILURE(rc)) continue;

	tassertMsg(raw == avail,
		   "State mismatch: %ld %llx %llx\n",
		   fd, raw.fullVal, avail.fullVal);
    }
#endif


    return _SRETUVAL(0);	// timed out
}
