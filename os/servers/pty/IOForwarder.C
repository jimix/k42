/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IOForwarder.C,v 1.8 2005/08/08 14:31:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Object to forward data from one IO object to another
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "IOForwarder.H"
#include <scheduler/Scheduler.H>

IOForwarder::IOForwarder():
    IONotif (FileLinux::READ_AVAIL|FileLinux::ENDOFFILE|FileLinux::DESTROYED)
{};


IOForwarder::~IOForwarder() {
    detach();
    if (src) {
	DREF(src)->detach();
	src = NULL;
    }
    if (target) {
	DREF(target)->detach();
	target = NULL;
    }
}

SysStatus
IOForwarder::init(FileLinuxRef from, FileLinuxRef to)
{
    SysStatus rc;
    target = to;
    src = from;
    lock.init();
    flags = IONotif::Persist;
    buf.init(4096);
    rc = DREF(src)->notify(this);
    return rc;
};

/* virtual */ void
IOForwarder::processData()
{
    SysStatus rc;
    GenState wState;
    sval len;
    char* space;

    do {
	if (!lock.tryAcquire()) {
	    return;
	}

	if (buf.spaceAvail() && (available.state & FileLinux::READ_AVAIL)) {
	    len = 2048;
	    space = buf.reserveSpace(len);

	    GenState rState;
	    rc = DREF(src)->read(space, len, NULL, rState);
	    (void) available.setIfNewer(rState);

	    if (_FAILURE(rc)) goto failure;

	    buf.commitSpace(_SGETUVAL(rc));
	}
	while (buf.bytesAvail()>0) {
	    len = 2048;
	    space = buf.peekBytes(len);
	    ThreadWait *tw=NULL;
	    do {
		while (tw && !tw->unBlocked()) {
		    Scheduler::Block();
		}
		if (tw) {
		    tw->destroy();
		    delete tw;
		    tw = NULL;
		}
		rc = DREF(target)->write(space, len, &tw, wState);
		tassertMsg(_SUCCESS(rc), "Failed fwd write: %lx\n",rc);
		if (_FAILURE(rc)) goto failure;
		if (wState.state &
			    (FileLinux::ENDOFFILE|FileLinux::DESTROYED)) {
		    goto finish;
		}
	    } while (tw);
	    buf.consumeBytes(_SGETUVAL(rc));
	}
	len = buf.bytesAvail();
	lock.release();
    } while (((available.state & FileLinux::READ_AVAIL) &&
		!(available.state & FileLinux::ENDOFFILE))
	     || (len != 0));

    if (!(available.state & (FileLinux::ENDOFFILE|FileLinux::DESTROYED))) {
	return;
    }

    if (!lock.tryAcquire()) return;

    while (buf.bytesAvail()>0) {
	len = 2048;
	space = buf.peekBytes(len);
	ThreadWait *tw=NULL;
	do {
	    while (tw && !tw->unBlocked()) {
		Scheduler::Block();
	    }
	    if (tw) {
		delete tw;
		tw = NULL;
	    }
	    rc = DREF(target)->write(space, len, &tw, wState);
	    tassertMsg(_SUCCESS(rc), "Failed fwd write: %lx\n",rc);
	    if (wState.state & FileLinux::ENDOFFILE) {
		goto finish;
	    }
	} while (tw);
	buf.consumeBytes(_SGETUVAL(rc));
    }
  finish:
    lock.release();
    return;
  failure:
    err_printf("IOForwarder failure: %lx\n",rc);
    goto finish;
}

/* static */ void
IOForwarder::ProcessData(uval iofUval)
{
    IOForwarder *const iof = (IOForwarder *) iofUval;
    iof->processData();
}

/* virtual */ void
IOForwarder::ready(FileLinuxRef fl, uval state)
{
    /*
     * Data-availability changes can result in potentially recursive read()
     * and write() calls on the src and target objects.  To avoid deadlocking
     * on the locks in those objects, we create a thread to process the data.
     */
    Scheduler::ScheduleFunction(ProcessData, uval(this));
}
