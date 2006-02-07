/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServerConsole.C,v 1.20 2005/07/15 17:14:28 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports interface to console to external programs.
 * **************************************************************************/
#include <kernIncs.H>
#include <io/IO.H>
#include "StreamServerConsole.H"
#include <cobj/CObjRootSingleRep.H>
#include <sys/ProcessSet.H>
#include <meta/MetaStreamServer.H>
#include <linux/termios.h>
#include <linux/ioctl.h>
#include "SystemControl.H"
#include "libksup.H"
#include "bilge/LocalConsole.H"
StreamServerConsole *StreamServerConsole::con = NULL;

/* virtual */ void
StreamServerConsole::calcAvailable(GenState& avail,
				   StreamServer::ClientData* client)
{
    if (buffer.bytesAvail() > 0) {
        avail.state = FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL;
    } else {
        avail.state = FileLinux::WRITE_AVAIL;
    }
}


/* virtual */ SysStatusUval
StreamServerConsole::recvfrom(struct iovec *vec, uval veclen, uval flags,
			      char *addr, uval &addrLen, GenState &moreAvail, 
			      void *controlData, uval &controlDataLen,
			      __XHANDLE xhandle)
{
    addrLen = 0;
    uval copied = 0;

    controlDataLen = 0; /* setting to zero, since no control data */

    lock();
    if (buffer.bytesAvail()) {
	char data[buffer.bufSize()];
	copied = buffer.getData(data, buffer.bufSize());
	memcpy_toiovec(vec, data, veclen, copied);
    }
    calcAvailable(moreAvail);
    unlock();
    clnt(xhandle)->setAvail(moreAvail);
    return copied;
}


/* virtual */ SysStatusUval
StreamServerConsole::sendto(struct iovec* vec, uval veclen, uval flags,
			    const char *addr, uval addrLen, 
			    GenState &moreAvail, 
			    void *controlData, uval controlDataLen,
			    __XHANDLE xhandle)
{
    uval len = vecLength(vec, veclen);
    char* buf = (char*)allocLocalStrict(len);

    tassertMsg((controlDataLen == 0), "oops\n");
    memcpy_fromiovec(buf, vec, veclen, len);

    SysConsole->write(buf, len);

    freeLocalStrict(buf, len);
    calcAvailable(moreAvail);

    if (XHANDLE_IDX(xhandle) != CObjGlobalsKern::ConsoleIndex) {
	clnt(xhandle)->setAvail(moreAvail);
    }

    return len;
}


void
StreamServerConsole::Init1(VPNum vp)
{
    if (vp==0) {
	MetaStreamServer::init();
	con = new StreamServerConsole();
	CObjRootSingleRepPinned::Create(con, (RepRef)GOBJK(TheConsoleRef));
	con->buffer.init(64);
    }
}

void
StreamServerConsole::Init2(VPNum vp)
{
    if (vp==0) {
	// Need to provide some sort of clientData object, to make it look
	// like a normal xhandle
	StreamServer::NullClientData *ncd = new StreamServer::NullClientData();
	MetaStreamServer::
	    createXHandle((ObjRef)GOBJK(TheConsoleRef), _KERNEL_PID,
                          MetaObj::globalHandle, MetaObj::write, (uval)ncd);
    }
}

/* virtual */ SysStatus
StreamServerConsole::handleXObjFree(XHandle xhandle)
{
    if (xhandle == login) {
	login = 0; //Force termination of ReadFromConsole thread
	lock();
	unlock();
	// Now we know ReadFromConsole won't read again

	SystemControl::AttachToConsole();
    }
    return StreamServer::handleXObjFree(xhandle);
};

/* virtual */ void
StreamServerConsole::readFromConsole()
{
    sval x;
    uval time = 10;
    lock();
    while (login!=0) {
	while (buffer.spaceAvail()==0) {
	    unlock();
	    time += (50000 - time) / 2;
	    Scheduler::DelayMicrosecs(time);
	    lock();
	}
	sval size = buffer.bufSize();
	char* buf = buffer.reserveSpace(size);
	x = SysConsole->read(buf, size);
	if (x==0) {
	    unlock();
	    time += (50000 - time) / 2;
	    Scheduler::DelayMicrosecs(time);
	    lock();
	} else {
	    time = 10;
	    buffer.commitSpace(x);

	    // If there were no bytes there before...
	    if (size==(sval)buffer.bufSize()) {
		locked_signalDataAvailable();
	    }
	}
    }
    unlock();
}

/* static */ void
StreamServerConsole::ReadFromConsole(uval t)
{
    ((StreamServerConsole*)t)->readFromConsole();
}

/* static */ void
StreamServerConsole::StartLoginShell(ObjectHandle &oh, ProcessID initPID)
{
    con->startLoginShell(oh, initPID);
}
/* virtual */ void
StreamServerConsole::startLoginShell(ObjectHandle &oh, ProcessID initPID)
{
    SysStatus rc;
    lock();
    rc = giveAccessByClient(oh, initPID);
    tassertMsg(_SUCCESS(rc), "woops %lx\n", rc);

    // Mark login handle
    login = oh.xhandle();

    // now start the polling read
    Scheduler::ScheduleFunction(StreamServerConsole::ReadFromConsole,
				(uval)this);
    unlock();
}


/* virtual */ SysStatus
StreamServerConsole::kosherxlist()
{
    XHandle xhandle;

    if (_FAILURE(lockIfNotClosingExportedXObjectList())) return 0;
    err_printf("<");
    xhandle = getHeadExportedXObjectList();
    while (xhandle != XHANDLE_NONE) {
	err_printf("X");
	GenState available;
	calcAvailable(available, clnt(xhandle));
	clnt(xhandle)->signalDataAvailable(available);
	xhandle = getNextExportedXObjectList(xhandle);
    }

    err_printf(">");
    unlockExportedXObjectList();

    return 0;
}

