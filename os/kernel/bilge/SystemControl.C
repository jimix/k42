/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SystemControl.C,v 1.6 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Represent console input as a ring buffer, so that
		       data can be inserted as input from non-console sources
 ****************************************************************************/
#include "kernIncs.H"
#include <scheduler/Scheduler.H>
#include "exception/ExceptionLocal.H"
#include "SystemControl.H"
#include <io/IORingBuffer.H>
#include <bilge/KBootParms.H>
#include <bilge/LocalConsole.H>

SystemControl* SystemControl::systemControl= NULL;

/*
 * We need a separate thread to read from the console, rather than the
 * thread  requesting  the  data,  since  new  data  might  come  from
 * applicaiton insert while the requesting thread is blocked.
 */
void
SystemControl::readFromConsole()
{
    char buffer[512];
    char echo[3*sizeof(buffer)]; // erasing 1 character requires echoing 3
    uval e;
    uval x;
    uval r = 0;
    uval s = 0;
    uval cr = 0;

    while (1) {
	while (SysConsole->peekChar() == ((char)-1)) {
	    if (!attachedToConsole) return;
	    Scheduler::DeactivateSelf();
	    Scheduler::DelayMicrosecs(50000);
	    Scheduler::ActivateSelf();
	}

	lock.acquire();

	x = spaceAvail();
	if (x > sizeof(buffer)) x = sizeof(buffer);

	r += SysConsole->read(buffer+r, x-r);

	e = 0;
	for (uval i = s; i < r; i++) {
	    switch (buffer[i]) {
	    case '\177':
	    case '\b':
		if (s > cr) {
		    // We go back one and erase one character as well
		    echo[e++] = '\b';
		    echo[e++] = ' ';
		    echo[e++] = '\b';
		    s--;
		} else {
		    echo[e++] = '\007'; // ring the bell
		}
		break;
	    case '\r':
		buffer[i] = '\n';
	    case '\n':
		cr = s + 1;
	    default:
		echo[e++] = buffer[s++] = buffer[i];
		break;
	    }
	}
	SysConsole->write(echo, e);

	r = s;	// discard backspaces and the characters they've obliterated

	if (cr > 0) {
	    putData(buffer, cr);
	    sem.V(cr);

	    // Move the rest to the front of the buffer
	    if (cr < r) {
		memcpy(&buffer[0], &buffer[cr], r - cr);
	    }
	    r -= cr;
	    s = r;
	    cr = 0;
	}

	lock.release();
    }
}

/* static */ void
SystemControl::ReadFromConsole(uval t)
{
    ((SystemControl*)t)->readFromConsole();
}

void
SystemControl::insert(char* buf, uval len)
{
    uval x;
    for (uval i=0; i<len; ++i) {
	if (buf[i]=='|') {
	    buf[i]='\n';
	}
    }
    while (len) {
	lock.acquire();
	x = spaceAvail();
	if (x<len) {
	    len = x;
	}
	if (len>0) {
	    x = putData(buf, len);
	    buf += x;
	    sem.V(x);
	    len -= x;
	}
	lock.release();
    }
}

/* static */ void
SystemControl::ClassInit(VPNum vp)
{
    if (vp!=0) return;

    systemControl = new SystemControl;
    systemControl->init(PAGE_SIZE);
    systemControl->attachedToConsole = 1;
}

/* static */ void
SystemControl::DetachFromConsole()
{
    systemControl->attachedToConsole = 0;
}

/* static */ void
SystemControl::AttachToConsole()
{
    systemControl->attachedToConsole = 1;
    Scheduler::ScheduleFunction(SystemControl::ReadFromConsole,
				(uval)systemControl);
}

/* virtual */ void
SystemControl::init(uval bufSize)
{
    lock.init();
    sem.init(0);
    IORingBuffer::init(bufSize);

    int rc;
    char * const varname = "K42_LOGIN_CONSOLE";
    char buf[256];
    rc = KBootParms::_GetParameterValue(varname, buf, 256);
    /*
     * The K42_LOGIN_CONSOLE asks the kernel to start the shell
     * on the console always after booting, and to prevent the
     * test loop from eating the characters fed to that shell
     */
    if (_FAILURE(rc) || buf[0] == '\0') {
	AttachToConsole();
    } else {
	err_printf("Suppressing Console.\n");
    }
}

/* virtual */ uval
SystemControl::read(char* data, uval size)
{
    uval val = 0;
    /* We assume these reads only happen from threads that have no
     * active clustered object references (at least ones that could
     * potentially be deleted.  We also assume the various calls made
     * from here don't access any potentially deleteable clustered
     * objects.  We also also assume that no one above us has already
     * deactivated this thread (if so, we assume Scheduler assertions
     * will be triggered).
     */
    Scheduler::DeactivateSelf();
    while (val<size) {
	if (val==0) {
	    sem.P();
	} else if (!sem.tryP()) {
	    break;
	}
	lock.acquire();
	if (bytesAvail()!=0) {
	    val += IORingBuffer::getData(data+val,1);
	}
	lock.release();
	if (data[val-1]=='\n' || data[val-1]=='\r') {
	    break;
	}
    }
    Scheduler::ActivateSelf();
    return val;
}

/* static */ uval
SystemControl::Read(char *str, uval len)
{
    tassertMsg((systemControl != NULL), "woops\n");
    return SystemControl::systemControl->read(str, len);
}

/* static */ void
SystemControl::Insert(char* buf, uval len)
{
    tassertMsg((systemControl != NULL), "woops\n");
    SystemControl::systemControl->insert(buf,len);
}


