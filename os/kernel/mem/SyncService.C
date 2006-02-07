/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SyncService.C,v 1.1 2005/06/15 04:49:13 jk Exp $
 *****************************************************************************/
#include "kernIncs.H"
#include "SyncService.H"
#include "cobj/sys/COSMgrObject.H"

/**
 * Root class for distributed SyncService objects
 */
class SyncService::MyRoot : public CObjRootMultiRepPinned {
protected:
    SyncService *reps[Scheduler::VPLimit];
public:
    virtual CObjRep * createRep(VPNum vp) {
	reps[vp] = new SyncService(vp);
	tassert(reps[vp],
		err_printf("No memory for SyncSerivce rep on vp %ld", vp)); 
	return reps[vp];
    }

    virtual SyncService *locateRep(FCMFileRef fcm);

    DEFINE_PINNEDGLOBALPADDED_NEW(SyncService::MyRoot);
    MyRoot(RepRef rep);
};

SyncService::MyRoot::MyRoot(RepRef rep)
 : CObjRootMultiRepPinned(rep)
{
    memset(reps, 0, sizeof(*reps));
}

SyncService *
SyncService::MyRoot::locateRep(FCMFileRef fcm)
{
    VPNum vp = COSMgrObject::refToVP((CORef)fcm);
    return reps[vp];
}

SyncService::SyncService(VPNum vp)
{
    fcmHashLock.init();
    timer.start();
}


/****************************************************************************
* Given that this is a kernel system wide object we instantiate all
* reps when the system is started this allows us to not worry about
* the rep set changing during the life time of the object (= system)
* Further we do not need to worry about destruction issues as it should
* never go away.  Note there are things that could break these assumptions:
*    Hot swapping of Hardware
*    Rep migration / restructuring
*****************************************************************************/
/* static */ void
SyncService::ClassInit(VPNum vp)
{
    if (vp == 0) {
        MyRoot *myRoot;

        myRoot = new MyRoot((RepRef)(GOBJK(TheSyncServiceRef)));
        passert(myRoot!=NULL, err_printf("No mem for SyncService\n"));
    } 
    /* To make life easier we instantiate all reps on all vp at startup
     * by making an external call to the single instance
     */
    DREF(((SyncService **)GOBJK(TheSyncServiceRef)))->establishRep();
}

SysStatus
SyncService::attachFCM(FCMFileRef fcm)
{
    SyncService *rep = COGLOBAL(locateRep(fcm));
    tassert(rep, err_printf("unable to locate a rep for fcm=%p\n", fcm));
    return rep->repAttachFCM(fcm);
}

SysStatus
SyncService::detachFCM(FCMFileRef fcm)
{
    SyncService *rep = COGLOBAL(locateRep(fcm));
    tassert(rep, err_printf("Unable to locate a rep for fcm=%p\n", fcm));
    return rep->repDetachFCM(fcm);
}

SysStatus
SyncService::repAttachFCM(FCMFileRef fcm) 
{
    uval tmp = 0;
    AutoLock<LockType> al(&fcmHashLock); // locks now, unlocks on return
    tassert(!fcmHash.find(fcm, tmp), err_printf("re-attach\n"));
    fcmHash.add(fcm, tmp);
    numFCM++;
    return 0;
}

SysStatus
SyncService::repDetachFCM(FCMFileRef fcm) 
{
    uval tmp;
    AutoLock<LockType> al(&fcmHashLock); // locks now, unlocks on return
    if (fcmHash.remove(fcm, tmp)) {
        numFCM--;
        return 0;
    }
    tassert(0, err_printf("No FCM (%p) to detach from SyncService\n", fcm));
    /** @todo: correct error code? */
    return 0;
}

/* static */ void
SyncService::HandleTimer(uval p)
{
    DREFGOBJK(TheSyncServiceRef)->handleTimer();
}

void
SyncService::handleTimer()
{
    syncFCMs();
    timer.start();
}

void 
SyncService::syncFCMs()
{
    FCMFileRef fcm = NULL;
    uval tmp;
    SysStatus rc;

    for (rc=fcmHash.getFirst(fcm, tmp); rc; rc=fcmHash.getNextWithFF(fcm, tmp))
	DREF(fcm)->fsync(0);
}
