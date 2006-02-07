/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRepArbiterCallCounter.C,v 1.1 2004/01/24 20:58:15 bob Exp $
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
#include "CObjRepArbiterCallCounter.H"

/*static*/ cocccRef CORepArbiterCallCounter::CORootArbiterCC::Create(){
    CORootArbiterCC* root = new CORootArbiterCC();
    return reinterpret_cast<cocccRef>(root->getRef());
}

/*virtual*/ CObjRep* CORepArbiterCallCounter::CORootArbiterCC::
                                                     createRep(VPNum vp){
    return reinterpret_cast<CObjRep*>(new CORepArbiterCallCounter());
}

SysStatus CORepArbiterCallCounter::handleCall(CallDescriptor* cd, uval fnum){

    FetchAndAddSignedSynced(&(callCount[fnum]), 1);
    return makeCall(cd, fnum);
}

/*static*/ cocccRef CORepArbiterCallCounter::Create(){
    return CORootArbiterCC::Create();
}

/*virtual*/
SysStatus CORepArbiterCallCounter::getCallCount(sval count[256]){

    CObjRootMultiRep* root = reinterpret_cast<CObjRootMultiRep*>(myRoot);
    for(int i = 0; i < 256; i++)
        count[i] = 0;

    root->lockReps();

    CORepArbiterCallCounter* rep = 0;
    for(void* curr = root->nextRep(0, reinterpret_cast<CObjRep*&>(rep)); curr;
        curr = root->nextRep(curr, reinterpret_cast<CObjRep*&>(rep)))
        for(int i = 0; i < 256; i++)
            count[i] += rep->callCount[i];

    root->unlockReps();

    return 0;
}
