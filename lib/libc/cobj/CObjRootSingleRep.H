#ifndef __COBJ_ROOT_SINGLE_REP_H_
#define __COBJ_ROOT_SINGLE_REP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRootSingleRep.H,v 1.11 2005/03/02 05:27:56 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <cobj/CObjRoot.H>
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>

class CObjRootSingleRep : public CObjRoot {
    void init(CObjRep *co);
protected:
    CObjRep *therep;
    VPSet transSet;

    CObjRootSingleRep(uval8 pool, CObjRep *co);

    SysStatus installRep(CORef ref, VPNum vp, COSTransObject *rep);
    virtual void resetTransSet() { transSet.init(); }
public:
    // We expect that there will be few references to a single rep root
    // so GLOBAL (unpadded) storage is appropriate
    DEFINE_GLOBAL_NEW(CObjRootSingleRep);
    DEFINE_PRIMITIVE_NEW(CObjRootSingleRep);

    //public because its needed if Root is allocated inside an object
    //rather than dynamically
    CObjRootSingleRep(CObjRep *co);

    //public because its used by some static objects
    CObjRootSingleRep(CObjRep *co, RepRef ref,
		    CObjRoot::InstallDirective idir=CObjRoot::Install);

    virtual SysStatus handleMiss(COSTransObject * &co,
 				 CORef ref, uval methodNum);
    virtual SysStatus cleanup(COSMissHandler::CleanupCmd cmd);
    virtual VPSet getTransSet();
    virtual VPSet getVPCleanupSet();

    virtual CObjRep *getRepOnThisVP()
    {
        transSet.atomicAddVP(Scheduler::GetVP());
        return therep;
    }

    // simple case, create a root for a single object
    static RepRef Create(CObjRep *co, void *ref=NULL) {
	if (ref==NULL) {
	    new CObjRootSingleRep(co);
	} else {
	    new CObjRootSingleRep(co, (RepRef)ref);
	}
	return co->getRef();
    }
};

class CObjRootSingleRepPinned: public CObjRootSingleRep {
public:
    DEFINE_PINNEDGLOBAL_NEW(CObjRootSingleRepPinned);
    DEFINE_PRIMITIVE_NEW(CObjRootSingleRepPinned);

    CObjRootSingleRepPinned(CObjRep *co)
	: CObjRootSingleRep(AllocPool::PINNED, co) {
	/* empty body */
    }

    //public because its used by some static objects
    CObjRootSingleRepPinned(CObjRep *co, RepRef ref,
			    CObjRoot::InstallDirective idir=CObjRoot::Install)
	:  CObjRootSingleRep(co, ref, idir) {
	/* empty body */
    }

    // simple case, create a root for a single object
    static RepRef Create(CObjRep *co, void *ref=NULL) {
	if (ref==NULL) {
	    new CObjRootSingleRepPinned(co);
	} else {
	    new CObjRootSingleRepPinned(co, (RepRef)ref);
	}
	return co->getRef();
    }

    static RepRef CreatePrimitive(CObjRep *co, MemoryMgrPrimitive *pa) {
	new(pa) CObjRootSingleRepPinned(co);
	return co->getRef();
    }
};

#endif /* #ifndef __COBJ_ROOT_SINGLE_REP_H_ */
