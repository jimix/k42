/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRootArbiter.H,v 1.1 2004/01/24 20:58:16 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#ifndef __COBJ_ROOT_ARBITER_H_
#define __COBJ_ROOT_ARBITER_H_

#include <cobj/sys/COSMgr.H>
#include <cobj/sys/COSMissHandler.H>
#include <cobj/CObjRoot.H>
#include <cobj/CObjRep.H>
#include <cobj/CObjRootMultiRep.H>
#include <cobj/CObjRootSingleRep.H>

class CObjRepArbiterTarget;
class CORepArbiter;

// to consider: should the takeover be done by the root? yes...
// since this allows the root to get information like the targetMH without
// going through the reps
class CORootArbiter : public CObjRootMultiRep{
protected:
    friend class CORepArbiter;
    friend class CObjRepArbiterTarget;
    COSMissHandler* targetMH;
    CORef targetRef;
    BLock targetInfoLock;
    // descendant needs to define:
    // virtual CObjRep* createRep(VPNum vp) = 0;
    DEFINE_GLOBAL_NEW(CORootArbiter);
    CORootArbiter();

    class CORootArbiterTarget : public CObjRootMultiRep{
    public:
        DEFINE_GLOBAL_NEW(CORootArbiterTarget);
        RepRef cref;
        CORootArbiterTarget(RepRef r) : cref(r) {}
        virtual CObjRep* createRep(VPNum vp);
        virtual SysStatus cleanup(COSMissHandler::CleanupCmd cmd);
    };
    CORootArbiterTarget* arbiterTargetRoot;
    ~CORootArbiter();
public:

    //static ArbiterRef Create();

    //virtual SysStatus handleMiss(COSTransObject*& co, CORef ref,
                                 //uval methodNum);
    // FIXME should identify if target is pinned and use pinned arbiter
    // FIXME write pinned arbiter
    virtual SysStatus captureTarget(CORef target);
    virtual SysStatus releaseTarget();
};

#endif
