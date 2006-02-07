/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IOForwarderConsole.C,v 1.4 2005/08/08 14:31:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Object to forward data from one IO object to another
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "IOForwarderConsole.H"
#include <scheduler/Scheduler.H>
#include <misc/utilities.H>

SysStatus
IOForwarderConsole::init(FileLinuxRef from)
{
    return IOForwarder::init(from, NULL);
};

/* virtual */ void
IOForwarderConsole::processData()
{
    SysStatus rc;
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
	    consoleWrite(space, len);
	    buf.consumeBytes(len);
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
	consoleWrite(space, len);
	buf.consumeBytes(len);
    }
  finish:
    lock.release();
    return;
  failure:
    err_printf("IOForwarderConsole failure: %lx\n",rc);
    goto finish;
}
