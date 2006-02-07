/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SyncService.H,v 1.2 2005/06/15 06:16:32 jk Exp $
 *****************************************************************************/
/*****************************************************************************
 * @class SyncService
 * FCMFile register with this class to have themselves called periodically 
 * for syncing.
 * **************************************************************************/

#ifndef __SYNCSERVICE_H_
#define __SYNCSERVICE_H_

#include <cobj/CObjRootMultiRep.H>
#include <misc/HashSimple.H>
#include "mem/FCMFile.H"

class SyncService : public Obj {
    typedef BLock LockType;
    class MyRoot;
    class TimerEventSS;
    friend class MyRoot;
    
    MyRoot* root() {return (MyRoot*)(myRoot);}

    /** protects the FCMHash */
    LockType fcmHashLock;

    /** hash of the FCMs that are attached to this SyncService */
    HashSimple<FCMFileRef, uval, AllocPinnedGlobal, 4> fcmHash;

    /** count of attached FCMs */
    uval numFCM;

    /** Called when the timer has expired */
    static void HandleTimer(uval p);
    void handleTimer();

    /**
     * A implementation of TimerEvent to trigger the regular fsync
     */
    class TimerEventSS : protected TimerEvent {
	/** number of seconds between syncs */
	static const uval SYNC_INTERVAL = 30;
    public:
	DEFINE_LOCALSTRICT_NEW(TimerEventSS);
	/**
	 * Called when the timer expires
	 */
	virtual void handleEvent() {
	    SysStatus rc = 
		Scheduler::DisabledScheduleFunction(SyncService::HandleTimer, 
			(uval)0);
	    tassert(_SUCCESS(rc), err_printf("oops\n"));
	}
	/**
	 * Start the timer to expire in SYNC_INTERVAL seconds
	 */
	void start() {
	    scheduleEvent(SYNC_INTERVAL * Scheduler::TicksPerSecond(),
		    TimerEvent::relative);
	}
    };

    /**
     * The TimerEvent that will call us every 30s
     */
    TimerEventSS timer;

    /**
     * Construct a SyncService representative on the specified vp
     * @param vp the vp number of this representative
     */
    SyncService(VPNum vp);

public:
    DEFINE_PINNEDGLOBALPADDED_NEW(SyncService);

    static void ClassInit(VPNum vp);
    
    /**
     * null function called to instantiate the rep
     */
    virtual SysStatus establishRep() { return 0; }

    /**
     * Attach a FCM (FCMFile) to the SyncService. This will find an appropriate
     * SyncService rep and call it to do the attaching
     * @param fcm The FCMFile to attach
     */
    virtual SysStatus attachFCM(FCMFileRef fcm);
    
    /**
     * Attach a FCM (FCMFile) to the SyncService
     * @param fcm The FCMFile to attach
     */
    virtual SysStatus detachFCM(FCMFileRef fcm);

protected:
    /**
     * Do the actual attachment. The fcm must exist on the same VP as the
     * SyncService
     * @param fcm The FCMFile to attach
     */
    virtual SysStatus repAttachFCM(FCMFileRef fcm);

    /**
     * Do the actual detach. The fcm must exist on the same VP as the
     * SyncService
     * @param fcm The FCMFile to attach
     */
    virtual SysStatus repDetachFCM(FCMFileRef fcm);

    /**
     * Call fsync on each of the attached FCMs
     */
    virtual void syncFCMs();
};

#endif /* #ifndef __SYNCSERVICE_H_ */
