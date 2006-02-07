/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRepArbiter.C,v 1.1 2004/01/24 20:58:15 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/COVTable.H>
#include <cobj/CObjRepArbiter.H>
#include <cobj/CObjRepArbiterTarget.H>
#include <cobj/CObjRootArbiter.H>

/* static */
//SysStatus CORepArbiter::Create(ArbiterRef& ref){
    //ref = CORootArbiter::Create();
    //return 0;
//}

/* virtual */
SysStatus CORepArbiter::captureTarget(CORef targetRef){

    // substitute arbiter for target
    CORootArbiter* root = reinterpret_cast<CORootArbiter*>(myRoot);
    return root->captureTarget(targetRef);
}

/* virtual */
SysStatus CORepArbiter::releaseTarget(){
    // don't need to save miss handler this time
    CORootArbiter* root = reinterpret_cast<CORootArbiter*>(myRoot);
    return root->releaseTarget();
}

/* virtual */
SysStatus CORepArbiter::makeCallAndReturn(CallDescriptor* cd, uval fnum){
    tassert(0, err_printf("CORepArbiter::makeCallAndReturn() says I'm NYI\n"));
    return 1;
}

/* virtual */
SysStatus CORepArbiter::makeCall(CallDescriptor* cd, uval fnum){
    uval funcPtr;
    targetRepLock.acquire();
    // FIXME Figure out if this is the right place to lock--other option would
    // be to sync between targetRep= and targetRepInstalled=, and let the miss
    // handler deal with concurrency. Then you would only need to get the
    // targetRepLock if there was no rep installed.
    if(!targetRepInstalled){
        LTransEntry targetLTE;
        targetLTE.tobj = NULL;
        targetLTE.co = reinterpret_cast<COSTransObject*>(&(targetLTE.tobj));

        // get rep for target
        SysStatus err = reinterpret_cast<CORootArbiter*>(myRoot)
           ->targetMH->handleMiss(targetRep,
                                  reinterpret_cast<CORef> (targetLTE.co), fnum);
        // should never happen...
        if(err != 0)
            return err;

        // get vtable for this call
        // This has to be done before installing the entry from the LTE in case
        // that overwrites the rep with a different value; I suppose you could
        // use that to initialize a CO on the first call if you were really
        // irritating.
        COVTable* vtable = *(reinterpret_cast<COVTable **>(targetRep));
        funcPtr = vtable->vt[fnum].getFunc();

        // determine if rep is cacheable (ie. was it installed in LTE)
        if(targetLTE.tobj != NULL){
            // save target rep
            targetRep = targetLTE.tobj;
            targetRepInstalled = true;
        }
        targetRepLock.release();
    }
    else{
        targetRepLock.release();
	// normal case: rep already cached
        COVTable* vtable = *(reinterpret_cast<COVTable **>(targetRep));
        funcPtr = vtable->vt[fnum].getFunc();

    }
    return arbiterCallOriginalMethod(this, cd, funcPtr);
}
