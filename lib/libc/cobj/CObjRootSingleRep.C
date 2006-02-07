/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRootSingleRep.C,v 1.4 2001/06/01 14:00:55 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/sys/COSMgr.H>
#include <alloc/MemoryMgrPrimitive.H>
#include "CObjRootSingleRep.H"

void
CObjRootSingleRep::init(CObjRep *co)
{
    therep=co;
    therep->setRoot(this);
}

CObjRootSingleRep::CObjRootSingleRep(CObjRep *co)
    : CObjRoot(AllocPool::PAGED)
{
    init(co);
}

CObjRootSingleRep::CObjRootSingleRep(uval8 pool, CObjRep *co)
    : CObjRoot(pool)
{
    init(co);
}

CObjRootSingleRep::CObjRootSingleRep(CObjRep *co, RepRef ref,
				     CObjRoot::InstallDirective idir)
    :  CObjRoot(ref,idir)
{
    init(co);
}

/* virtual */ VPSet
CObjRootSingleRep::getTransSet()
{
    return transSet;
}

/* virtual */ VPSet
CObjRootSingleRep::getVPCleanupSet()
{
    // For the single rep clustered object the default implementation
    // does not use the COSMgr ability to invoke cleanup on a specific
    // processor (eg. the processor on which the rep was created).
    // So we simply pass back an empty bit set here.
    VPSet emptyVPSet;
    return emptyVPSet;
}

SysStatus
CObjRootSingleRep::installRep(CORef r, VPNum vp, COSTransObject *rep)
{
    LTransEntry *lte=(LTransEntry *)r;

    lte->setCO(rep);

    return 0;
}

SysStatus
CObjRootSingleRep::handleMiss(COSTransObject * &co, CORef ref, uval methodNum)
{
#if 0
    // No Locking.  Worst case we repeat work.
    LTransEntry *lte=(LTransEntry *)ref;
    //err_printf("*** CObjRootSingleRep::handleMiss() : caught the miss on"
    //         "  ref=%p methodNum=%ld lte=%p\n", ref, methodNum, lte);
    lte->setCO(therep);
    vpMask |= (1 << Scheduler::GetVP());
#endif
    VPNum myvp = Scheduler::GetVP();

    transSet.atomicAddVP(myvp);
    installRep(ref, myvp, therep);
    co=therep;
    return 0;
}

SysStatus
CObjRootSingleRep::cleanup(CleanupCmd cmd)
{
//    err_printf("CObjRootSingleRep::cleanup called, this=%p\n", this);

    tassert(cmd == STARTCLEANUP, err_printf("The only valid cleanup command is"
					    " is STARTCLEANUP\n"));

    // cleanup the rep
    therep->cleanup();
    // delete the root itself
    delete this;
    
    return 1;
}

//FIXME: destroyUnchecked should 
//{
//    myRoot->destory(CObjRoot::setToDeleted);
//    return(0);
//}
