/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRootArbiter.C,v 1.2 2004/07/11 21:59:23 andrewb Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRepArbiter.H>
#include <cobj/CObjRepArbiterTarget.H>
#include <cobj/CObjRootArbiter.H>

extern "C" void verr_printf(const char *fmt, va_list ap);

CORootArbiter::CORootArbiter(){
    targetInfoLock.acquire();
    targetRef = 0;
    // For some reason we create a multi rep CO for the ArbiterTarget, but
    // we only access it through the root, never the reps. That's probably
    // because calling the reps makes use of its special breed of magic.
    // In fact, we don't even have a ref for it until later; maybe it's a good
    // FIXME idea to put in a check so that its assigned ref isn't called?
    arbiterTargetRoot = new CORootArbiterTarget(getRef());
    targetInfoLock.release();
}

CObjRep* CORootArbiter::CORootArbiterTarget::createRep(VPNum vp){
    return reinterpret_cast<CObjRep*>(new CObjRepArbiterTarget(
                                          reinterpret_cast<ArbiterRef>(cref)));
}

// This is modified because cleanup (like everything else) points to
// arbiterMethodCommon for the CObjRepArbiterTarget. Deleting a targetted CO
// isn't handled now; if it were there would likely have to be changes to this.
/*virtual*/ SysStatus CORootArbiter::CORootArbiterTarget::
                                      cleanup(COSMissHandler::CleanupCmd cmd){
    VPNum myvp = Scheduler::GetVP();
    CObjRep* rep = 0;
    SysStatus rtn = -1;

    if(cmd == COSMissHandler::STARTCLEANUP){
        myRef = 0;
        rtn = 1;
    }

    if(myRef == 0){
        tassert(repSet.isSet(myvp),
                err_printf("cleanup called on vp=%ld but it is not in repSet\n",
                           myvp));
        // no lock needed as no one can be modifying rep list now
        replistFind(myvp, rep);
        tassert(rep!=0, err_printf("oops expected to find a rep for vp=%ld\n",
                                   myvp));
        delete rep;
        if(repSet.atomicRemoveVPAndTestEmpty(myvp)){
            // FIXME: May want to move this up into per vp action but will
            //        then have to use locking.
            while(replistRemoveHead(rep));
            delete this;
        }
        rtn = 1;
    }
    return rtn;
}

// the following two functions need to go in the descendant because
// CORootArbiter::createRep can't be implemented because the rep type isn't
// known at this point (it is preumably a descendant of CORepArbiter).
/*static ArbiterRef CORootArbiter::Create(){
    return reinterpret_cast<ArbiterRef>((new CORootArbiter())->getRef());
}*/

/*virtual CObjRep* CORootArbiter::createRep(VPNum vp){
    return reinterpret_cast<CObjRep*>(new RepClass(this));
}*/

/*virtual*/ SysStatus CORootArbiter::captureTarget(CORef target){
    // substitute will blow away target reps and install our miss handler
    // we just need to save the original so that we can instantiate target reps
    targetInfoLock.acquire();

    tassert(targetRef == 0, err_printf("trying to set target of targetted arbiter\n"));

    COSMissHandler* nMH = reinterpret_cast<COSMissHandler*>(arbiterTargetRoot);

    targetRef = target;
    DREFGOBJ(TheCOSMgrRef)->substitute(targetRef, targetMH, nMH);

    targetInfoLock.release();

    return 0;
};

/*virtual*/ SysStatus CORootArbiter::releaseTarget(){
    COSMissHandler* ourMH = 0; // dummy variable to get returned data--not used

    targetInfoLock.acquire();

    tassert(targetRef != 0, err_printf("trying to release untargetted arbiter\n"));

    // substitute restores the original and returns our miss handler
    DREFGOBJ(TheCOSMgrRef)->substitute(targetRef,
                                             ourMH, targetMH);
    targetMH = 0;
    targetRef = 0;

    targetInfoLock.release();

    lockReps();

    CORepArbiter* rep = 0;
    for(void* curr = nextRep(0, reinterpret_cast<CObjRep*&>(rep)); curr;
        curr = nextRep(curr, reinterpret_cast<CObjRep*&>(rep))){
        rep->targetRep = 0;
        rep->targetRepInstalled = false;
    }

    unlockReps();

    return 0;
};

CORootArbiter::~CORootArbiter(){
    targetInfoLock.acquire();
    if(targetRef)
        releaseTarget();
    targetInfoLock.release();
    DREFGOBJ(TheCOSMgrRef)->destroyCO(
                      reinterpret_cast<CORef>(arbiterTargetRoot->getRef()),
                      reinterpret_cast<COSMissHandler*>(arbiterTargetRoot));
}
