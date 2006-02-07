/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LocalConsole.C,v 1.10 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implements a simple version of line buffering
 * FIXME: there is really just one console supported for input now,
 * get rid of this in exception local.
 **************************************************************************/

#include <kernIncs.H>
#include "LocalConsole.H"
#include "defines/inout.H"
#include <sys/thinwire.H>
#include "exception/ExceptionLocal.H"
#include "libksup.H"
#include "bilge/ThinWireMgr.H"
#include <misc/baseStdio.H>
#include <sync/BlockedThreadQueues.H>
#include "scheduler/Scheduler.H"
#include "misc/hardware.H"

LocalConsole* SysConsole = NULL;

/*
 * If thinwire console exists, then we always want to use it,
 * for input so that we have a separate console for each window,
 * otherwise want to poll on reading, and schedule a yeild.
 * when got 0 data back.
 */
SysStatusUval
LocalConsole::read(char *ptr, uval len)
{
    SysStatus rc = 0;
    /* We assume console reads only happen from threads that have no active
     * clustered object references (at least ones that could potentially
     * be deleted.  We also assume the various calls made from here don't
     * access any potentially deleteable clustered objects.  We also also
     * assume that no one above us has already deactivated this thread
     * (if so, we assume Scheduler assertions will be triggered).
     */
    if (len==0) return rc;

    rdlock.acquire();
    Scheduler::DeactivateSelf();


    if (buffer.bytesAvail()==0) {
	rc = ioc->read(ptr, len, 0);
    } else {
	rc = buffer.getData(ptr, len);
    }

    Scheduler::ActivateSelf();
    rdlock.release();
    return rc;
}

SysStatusUval
LocalConsole::write(const char *buf, uval len)
{
    tassertSilent( hardwareInterruptsEnabled(), BREAKPOINT );
    wtlock.acquire();

    ioc->write(buf, len, 1);

    wtlock.release();

    return len;
}


void
LocalConsole::putChar(char c)
{
    write(&c, 1);
}


SysStatusUval
LocalConsole::getChar(char &c)
{
    SysStatusUval rc;
    rc = read(&c,1);
    return rc;
}

void
LocalConsole::setConsole(IOChan* chan)
{
    ioc = chan;
}

void
LocalConsole::init(MemoryMgrPrimitive* mem, IOChan* chan)
{
    uval ptr;

    mem->alloc(ptr, 256);

    // vtable pointer in "buffer" member must be initialized
    // Best done by taking a cleanly initialized object and copying it.
    IORingBuffer tmp;
    memcpy(&buffer, &tmp, sizeof(tmp));
    buffer.init(256, (char*)ptr);

    raw = chan;
    setConsole(chan);

    rdlock.init();
    wtlock.init();
}

void
LocalConsole::Init(VPNum vp, MemoryMgrPrimitive* mem, IOChan* chan)
{
    SysConsole = new(mem) LocalConsole;
    SysConsole->init(mem, chan);
}

char
LocalConsole::peekChar()
{
    AutoLock<LockType> al(&rdlock); // locks now, unlocks on return
    sval bytes = 1;
    char *c = NULL;
    char peekedChar = (char)-1;
    c = buffer.peekBytes(bytes);
    if (bytes) {
	return *c;
    }

    sval size = -1;
    c = buffer.reserveSpace(size);
    SysStatus rc = ioc->read(c, size, 0);
    if (rc > 0) {
	peekedChar = *c;
	buffer.commitSpace(rc);
    }
    return peekedChar;
}

sval
consolePeekChar(void)
{
    return SysConsole->peekChar();
}

void
consolePutChar(char c)
{
    return SysConsole->putChar(c);
}

void
consoleWrite(const char *str, uval len)
{
    SysConsole->write(str,len);
}
