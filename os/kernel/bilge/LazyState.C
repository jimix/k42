/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LazyState.C,v 1.9 2003/11/09 21:57:30 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Preserves state of object reopened on demand
 * **************************************************************************/

#include <kernIncs.H>
#include <defines/experimental.H>
#include "LazyState.H"
#include <stub/StubFileLinuxServer.H>
#include <io/FileLinux.H>

/*
 * Object on which we maintian lazy state.
 * Currently, maintain StubFileLinux, eventually have a generic object that
 * is parent of other objects also manage lazily
 */
class LazyState::StubFileLinuxServerHolder {
    sval closeChain;			// chain of objects to be closed
    uval refcount;
    uval type;				// the type of the object
    char *data;				// extra data associated with obj
    uval dataLen;
    AccessRights match;
    AccessRights nomatch;
    StubObj stub;
    typedef BLock LockType;
    LockType      lock;			// lock on object
public:
    StubFileLinuxServerHolder(
	ObjectHandle oh, uval t, AccessRights m, AccessRights nm,
	char *d, uval dl, sval c) : stub(StubObj::UNINITIALIZED) {
	refcount = 0; type = t; stub.setOH(oh);
	closeChain = c;
	// The following passert is part of preparation to get rid of
	// closeChain (we can't recall a reason for it!!)
	passertMsg(closeChain == -1, "?");
	match = m;
	nomatch = nm;
	data = (char*)allocGlobal(dl); memcpy(data, d, dl); dataLen = dl;
    }
    void incRefCount() { lock.acquire(); refcount++; lock.release(); }
    // returns refcount, if 0, means should close
    uval decRefCount() {
	lock.acquire();
	refcount--;
	lock.release();
	return refcount;
    }

    // synchronous calls, FIXME: move out of kernel
    ~StubFileLinuxServerHolder() { stub._releaseAccess(); }

    SysStatus lazyReOpen(ProcessID pid, uval &tp, ObjectHandle &oh,
			 char *d, uval &dl) {
	SysStatus rc;
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	tp = type;
	memcpy(d, data, dataLen);
	dl = dataLen;
	if (type == FileLinux_FILE || type == FileLinux_DIR) {
	    rc = stub._lazyReOpen(oh, pid, match, nomatch, d, dl);
	} else {
	    rc = stub._lazyReOpen(oh, pid, match, nomatch);
	}
	passertMsg(_SUCCESS(rc), "wops");
	return rc;
    }
    uval getCloseChain() { return closeChain; }

    DEFINE_GLOBAL_NEW(StubFileLinuxServerHolder);
};

SysStatus
LazyState::lazyReOpen(ProcessID pid, sval file, uval &type, ObjectHandle &oh,
		      char *data, uval &dataLen)
{
//    err_printf("%p - will reopen %ld\n", this, file);
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    StubFileLinuxServerHolder *sf;
    SysStatus rc;
    if (files.find(file, sf)==0) {
	// didn't find, return error
	tassertMsg(0, "didn't find file\n");
	return _SERROR(2252, 0, EINVAL);
    }
    // found the file, lets re-open it
    rc = sf->lazyReOpen(pid, type, oh, data, dataLen);
    tassertMsg(_SUCCESS(rc), "failed\n");

    rc = locked_lazyClose(file);
    tassertMsg(_SUCCESS(rc), "failed\n");

//    err_printf("%p - had done reopen %ld\n", this, file);
    return rc;
}

SysStatus
LazyState::locked_lazyClose(sval file)
{
    StubFileLinuxServerHolder *sf;
    while (file != -1) {
//	err_printf("%p - close %ld\n", this, file);
	if (files.remove(file, sf)==0) {
	    // didn't find, return error
	    tassertMsg(0, "didn't find file\n");
	    return _SERROR(2253, 0, EINVAL);
	}
	file = sf->getCloseChain();
	if (sf->decRefCount() == 0) { // should close
	    delete sf;
	}
    }
    return 0;
}

SysStatus
LazyState::lazyGiveAccess(sval file, StubFileLinuxServerHolder *s)
{
    s->incRefCount();
    files.add(file, s);
    return 0;
}


SysStatus
LazyState::lazyGiveAccess(sval file, uval type, ObjectHandle oh,
			  sval closeChain,
			  AccessRights match, AccessRights nomatch,
			  char *d, uval dl)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
//    err_printf("%p - give access %ld\n", this, file);
    StubFileLinuxServerHolder* sf = new StubFileLinuxServerHolder(
	oh, type, match, nomatch, d, dl, closeChain);
    return lazyGiveAccess(file, sf);
}

SysStatus
LazyState::copyState(LazyState *from)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    // loop, copy all
    StubFileLinuxServerHolder *sf;
    uval file;
    uval count=0;
    void *ptr = from->files.next(NULL, file, sf);
    while (ptr) {
	count++;
	lazyGiveAccess(file, sf);
	ptr = from->files.next(ptr, file, sf);
    }
//    err_printf("%p - copied %ld\n", this, count);
    return 0;
}

void
LazyState::detach()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    // loop, close all
    StubFileLinuxServerHolder *sf;
    uval file;
    while (files.removeHead(file, sf) != 0) {
	if (sf->decRefCount() == 0) { // should close
	    delete sf;
	}
    }
}
