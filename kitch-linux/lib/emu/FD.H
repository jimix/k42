#ifndef __FD_H_
#define __FD_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FD.H,v 1.52 2005/07/15 17:14:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FD (file descriptor) implementation for linux
 * **************************************************************************/

#include <defines/experimental.H>
#include <cobj/ObjectRefs.H>
#include <sync/atomic.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <misc/ListSimpleLocked.H>
#include <io/FileLinux.H>
#include <io/GenState.H>
#include <defines/experimental.H>

struct __k42_timeval {
    sval  tv_sec;		/* Seconds.  */
    sval  tv_usec;		/* Microseconds.  */
  };

struct pollfd;
class PollNotif;

class _FD {
    friend class IOForkManagerLinuc;
public:
    // only one instance
    static _FD *FileTable;
    enum {
	FD_MAX=__FD_SETSIZE,		// total number of bits
    };

  // private:   _FD::FDSet is used in lib/emu/select.C
    class FDSet : public fd_set {
	friend class _FD;
	enum {
	    Words = (__FD_SETSIZE / __NFDBITS), // number of words
	    BitsPerWord = __NFDBITS
	};
    public:
	void clr(uval i) { FD_CLR(i, this); }
	void set(uval i) { FD_SET(i, this); }
	uval isSet(uval i) { return FD_ISSET(i, this); }
	void zero() { FD_ZERO(this); }
	void zero(uval i) {
	    /*
	     * zero "words" containing i bits
	     */
	    uval words = ((i+(BitsPerWord-1)) / BitsPerWord); // "words"
	    words = words * (BitsPerWord/8); // bytes
	    memset(this, 0, words);
	}

	void setAll();

	static void Sanity();

	// find first zero bit >= from
	SysStatusUval findZero(uval from=0);

	// find first bit including from that matches up to (not including) max
	SysStatusUval findMatchBit(FDSet *vec, uval from, uval max);

	void copyTo(FDSet *to);
    };

    // list of selects currently blocked, have seperate lock so don't have to
    // re-acquire FD lock when done select
    struct PollBlockElem;
    AutoListHead blockedPolls;
    FairBLock pollsLock;

    /*
     * we tried to do the FD non blocking, but its impossible to get
     * a sensible answer for replaceFD that way, and all the other calls
     * are questionable in the face of multithreading.
     *
     * Note that getFD does NOT aquire the lock, so only open, close,
     * etc. scale poorly.
     */
    typedef BLock LockType;
    LockType lock;
    Thread* owner;

    /*
     * active represents in-use fd's by 1 bits.
     * To make reasoning easier, we maintain the invarient
     * that if a active bit is zero, the corresponding coe bit,
     * list entry, and readyForLazy must be null.
     */
    FDSet active;
    struct PollNotif;
    struct FDRecord{
	DEFINE_NOOP_NEW(FDRecord);
	FileLinuxRef ref;
	PollNotif*   poll;
	FDRecord(): ref(NULL), poll(NULL) {};
    };
    FDRecord fdrec[FD_MAX];
    FDSet readyForLazy;			// bit for ready for Lazy, orthog
    FDSet coe;				// close on exec

    // coe != 0 if close on exec
    SysStatus copyFDs(uval coe);

    FileLinuxRef lazyGetFD(uval fd) {
	AutoLock<LockType> al(&lock);
	return locked_lazyGetFD(fd);
    }
    FileLinuxRef locked_lazyGetFD(uval fd);

    FileLinuxRef getFD(uval fd) {
	if (fd >= FD_MAX) { // unsigned comparison will catch "negative" fds
	    return NULL;
	}
	if (!active.isSet(fd)) {
	    return NULL;
	}
	if (fdrec[fd].ref) {
	    return fdrec[fd].ref;
	}
	return lazyGetFD(fd);
    }
    FileLinuxRef locked_getFD(uval fd) {
	if (!active.isSet(fd)) {
	    return NULL;
	}
	if (fdrec[fd].ref) {
	    return fdrec[fd].ref;
	}
	return locked_lazyGetFD(fd);
    }

    int getCOE(uval fd);
    void setCOE(uval fd, uval flag);
    FileLinuxRef freeFD(uval fd);

    SysStatus closeFD(int fd) {
	AutoLock<LockType> al(&lock);
	return locked_closeFD(fd);
    }
    SysStatus locked_closeFD(int fd);

    SysStatusUval allocFD(FileLinuxRef fileLinuxRef, uval lowestfd=0);
    // needed by dup2.  Must replace the current value of fd
    // with new value.  If fd is not in use, allocates it, sets
    // it and replaces fileLinuxRef with NULL
    SysStatusUval replaceFD(FileLinuxRef& fileLinuxRef, uval fd);

    void closeAll();
    void closeOnExec();  //for "fast", in-process exec
    SysStatus forkAll(void) { return copyFDs(0); }
    SysStatus execAll(void) { return copyFDs(1); }

    // poll() timeout assumed to be in micro-seconds.
    SysStatusUval smallPoll(struct pollfd *fds, uval numfds, sval &timeout);
    SysStatusUval poll(struct pollfd *fds, uval numfds, sval &timeout);
    SysStatus setPollStatus(uval fd, FDRecord *fdr, uval state);
    static void CopyReadyForLazy(FDSet *target) {
	FileTable->readyForLazy.copyTo(target);
    }

public:
    DEFINE_GLOBALPADDED_NEW(_FD);
    static void ClassInit(FDSet *fromParent);

    // do NOT aquire lock here, its not necessary
    static FileLinuxRef GetFD(uval fd) { return FileTable->getFD(fd);}
    static int GetCOE(uval fd) {return FileTable->getCOE(fd);}
    static void SetCOE(uval fd, uval flag) {
	return FileTable->setCOE(fd, flag);
    }
    static FileLinuxRef FreeFD(uval fd) { return FileTable->freeFD(fd); }

    // free fd and detaches from underlying object
    static SysStatus CloseFD(int fd) { return FileTable->closeFD(fd);}

    static SysStatusUval AllocFD(FileLinuxRef fLR, uval lowestfd = 0) {
	return FileTable->allocFD(fLR, lowestfd);
    }
    // needed by dup2.  Must replace the current value of fd
    // with new value.  If fd is not in use, allocates it, sets
    // it and replaces fileLinuxRef with NULL
    static SysStatusUval ReplaceFD(FileLinuxRef& fLR, uval fd) {
	return FileTable->replaceFD(fLR, fd);
    }

    static void CloseAll() { FileTable->closeAll();}
    static SysStatus ForkAll(void) { return FileTable->copyFDs(0);}
    static SysStatus ExecAll(void) { return FileTable->copyFDs(1);}

    // poll() timeout assumed to be in micro-seconds.
    static SysStatusUval Poll(struct pollfd *fds, uval numfds, sval &timeout);
    static SysStatus SetPollStatus(uval fd, FDRecord* fdr, uval state) {
	return FileTable->setPollStatus(fd, fdr, state);
    }
};

#endif /* #ifndef __FD_H_ */
