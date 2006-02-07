#ifndef __THIN_WIRE_MGR_H_
#define __THIN_WIRE_MGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ThinWireMgr.H,v 1.26 2005/02/09 18:45:42 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A on-thread interface to ThinWire facility
 * **************************************************************************/
#include <sys/thinwire.H>
#include <sync/FairBLock.H>


struct TWMgrChan: public IOChan {
    DEFINE_GLOBAL_NEW(TWMgrChan);
    TWMgrChan(uval num): IOChan(num) {};
    virtual uval isReadable();
    virtual uval read(char* buf, uval length, uval block = 1);
    virtual uval write(const char* buf, uval length, uval block = 1);
};




class ThinWireMgr {
    static ThinWireMgr *thinWireMgr;
    friend struct TWConChan;
    friend struct TWMgrChan;

    typedef FairBLock LockType;
    LockType lock;

    uval32 readAvailable;
    uval32 readPending;

    // delay for polling thinwire
    uval minDelay, maxDelay, currDelay;

    struct PendingRequest {
	ThreadID id;			// not NullThreadID if thread blocked
    };

    PendingRequest pending[NUM_CHANNELS];

    ThreadID daemon;			// polls for data for reads
    uval     suspendDaemon;		// flag to tell daemon to suspend
    uval     runDaemon;			// flag to tell daemon to terminate

    // operations that actually go out
    inline sval locked_updateAvailable();
    inline sval locked_read(uval channel, char *buf, uval length);
    inline sval locked_write(uval channel, const char *buf, uval length);

    sval blockingRead(uval channel, char *buf, uval length);
    sval blockingWrite(uval channel, const char *buf, uval length);

    sval nonBlockingRead(uval channel, char *buf, uval length);
    sval nonBlockingWrite(uval channel, const char *buf, uval length);

    uval testAvailable(uval channel);

    inline uval locked_testAvailable(uval channel);
    inline void locked_clearAvailable(uval channel);
    inline uval locked_testPending(uval channel);
    inline void locked_clearPending(uval channel);
    inline void locked_setPending(uval channel);

    static void BeAsynchronous(uval arg);

    DEFINE_PINNEDGLOBAL_NEW(ThinWireMgr);

private:
    static uval KillDaemon();

public:
    static uval TestAvailable(uval channel);
    static void SetPollDelay(uval minDelay, uval maxDelay);

    static sval BlockingRead(uval channel, char *buf, uval length);

    static sval BlockingWrite(uval channel, const char *buf, uval length);

    static uval NonBlockingRead(uval channel, char *buf, uval length);

    static void NonBlockingWrite(uval channel, const char *buf, uval length);

    static uval SuspendDaemon();	// returns 1 if previously running
    static uval RestartDaemon();	// returns 1 if previously running

    static void ClassInit(VPNum vp);

    // returns != 0 if got something that woke a thread
    static sval DoPolling();
};

#endif /* #ifndef __THIN_WIRE_MGR_H_ */
