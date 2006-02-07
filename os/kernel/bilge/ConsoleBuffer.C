/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ConsoleBuffer.C,v 1.3 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Represent console input as a ring buffer, so that
		       data can be inserted as input from non-console sources
 ****************************************************************************/
#include "kernIncs.H"
#include <scheduler/Scheduler.H>
#include "exception/ExceptionLocal.H"
#include "ConsoleBuffer.H"
#include <io/IORingBuffer.H>

ConsoleBuffer* ConsoleBuffer::console= NULL;

void
ConsoleBuffer::read()
{
    char ptr[1024];
    sval x;
    lock.acquire();
    while (1) {
	x = spaceAvail();
	if (x>1023) {
	    x = 1023;
	}
	lock.release();
	x = TheConsole->read(ptr,x);
	lock.acquire();
	putData(ptr,x);
	sem.V(x);
    }
    lock.release();
}

/* static */ void
ConsoleBuffer::Read(uval t)
{
    ((ConsoleBuffer*)t)->read();
}

void
ConsoleBuffer::insert(char* buf, uval len)
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
ConsoleBuffer::ClassInit(VPNum vp)
{
    if (vp!=0) return;

    console = new ConsoleBuffer;
    console->init(PAGE_SIZE);
}

/* virtual */ void
ConsoleBuffer::init(uval bufSize)
{
    lock.init();
    sem.init(0);
    IORingBuffer::init(bufSize);
	Scheduler::ScheduleFunction(ConsoleBuffer::Read,(uval)console);
}

/* virtual */ uval
ConsoleBuffer::getData(char* data, uval size)
{
    uval val = 0;
    /* We assume console reads only happen from threads that have no
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


void insertToConsole(char* buf, uval len)
{
    ConsoleBuffer::console->insert(buf,len);
}

uval
consoleRead(char *str, uval len)
{
    if (ConsoleBuffer::console) {
	return ConsoleBuffer::console->getData(str, len);
    }
    return TheConsole->read(str, len);
}

