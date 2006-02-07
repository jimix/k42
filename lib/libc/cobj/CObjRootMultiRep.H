#ifndef __COBJ_ROOT_MULTI_REP_H_
#define __COBJ_ROOT_MULTI_REP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRootMultiRep.H,v 1.23 2005/03/02 20:21:54 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <cobj/CObjRoot.H>
#include <misc/ListSimpleKey.H>
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>

class CObjRootMultiRepBase : public CObjRoot {
protected:
// FIXME:  Optimize this!
    BLock lock;
    VPNum clustersize;
    VPSet transSet;
    //FIXME:  If this proves to be too much overhead it could be constructed
    //        at deletion time.
    VPSet repSet;

    SysStatus installRep(CORef ref, VPNum vp, COSTransObject *rep);
    CObjRep *locked_getRep(VPNum vp);
    void locked_addRep(VPNum vp, CObjRep *rep);

    /*
     * we want to be able to generate a pinned and paged version
     * of the list part of a MultiRep.  But its not performance sensitive
     * so we don't want two complete implementations.  Rather, we
     * provide the list either way
     * as subclasses of Base, and each implements the
     * list functions that follow
     */
    virtual void  replistAdd(uval vp, CObjRep * rep) = 0;
    virtual uval  replistFind(uval vp, CObjRep *& rep) = 0;
    virtual void *replistNext(void *curr, uval& vp, CObjRep*& rep) = 0;
    virtual uval  replistRemove(uval vp, CObjRep *& rep) = 0;
    virtual uval  replistRemoveHead(CObjRep *& rep) = 0;
    virtual uval  replistIsEmpty() = 0;
    virtual uval  replistHasRep(CObjRep * rep) = 0;

    virtual void  resetTransSet() { transSet.init(); }
public:
    // pool specifies kind of ref to allocate
    CObjRootMultiRepBase(sval c=1, uval8 pool=AllocPool::PAGED):
	CObjRoot(pool),clustersize(c) {
	/* empty body */
    }

    CObjRootMultiRepBase(RepRef ref, sval c=1,
			 CObjRoot::InstallDirective idir=CObjRoot::Install):
	CObjRoot(ref,idir),clustersize(c) {
	/* empty body */
    }

    virtual SysStatus handleMiss(COSTransObject * &co,
				 CORef ref, uval methodNum);

    virtual SysStatus cleanup(COSMissHandler::CleanupCmd cmd);

    virtual CObjRep *findRepOn(VPNum vp) { 
        lock.acquire(); 
        CObjRep *rep=locked_findRepOn(vp);
        lock.release();
        return rep;
    }
    virtual CObjRep *locked_findRepOn(VPNum vp);
    virtual void lockReps()   { lock.acquire(); }
    virtual void unlockReps() { lock.release(); }
    virtual void *nextRep(void *curr, CObjRep *&rep);
    virtual void *nextRepVP(void *curr, VPNum &vp, CObjRep *&rep);
    virtual CObjRep *createRep(VPNum vp)=0;
    virtual VPSet getTransSet();
    virtual VPSet getVPCleanupSet();
    virtual CObjRep *getRepOnThisVP();
};

class CObjRootMultiRep: public CObjRootMultiRepBase {
protected:
    ListSimpleKey<uval,CObjRep *,AllocGlobal> replist;
    virtual void replistAdd(uval vp, CObjRep * rep) {
	replist.add(vp, rep); }
    virtual uval replistFind(uval vp, CObjRep *&rep) {
	return replist.find(vp, rep); }
    virtual void* replistNext(void *curr, uval& vp, CObjRep*& rep) {
	return replist.next(curr, vp, rep); }
    virtual uval replistRemove(uval vp, CObjRep*& rep) {
	return replist.remove(vp, rep); }
    virtual uval replistRemoveHead(CObjRep *& rep) {
	uval vp;
	return replist.removeHead(vp, rep); }
    virtual uval replistIsEmpty() {
	return replist.isEmpty(); }
    virtual uval  replistHasRep(CObjRep * rep) {
        return replist.has(rep); }
public:
    CObjRootMultiRep(sval c=1):
	CObjRootMultiRepBase(c, AllocPool::PAGED) {
	/* empty body */
    }

    CObjRootMultiRep(RepRef ref, sval c=1,
		     CObjRoot::InstallDirective idir=CObjRoot::Install):
	CObjRootMultiRepBase(ref, c, idir) {
	/* empty body */
    }
};

class CObjRootMultiRepPinned: public CObjRootMultiRepBase {
protected:
    ListSimpleKey<uval,CObjRep *,AllocPinnedGlobal> replist;
    virtual void replistAdd(uval vp, CObjRep * rep) {
	replist.add(vp, rep); }
    virtual uval replistFind(uval vp, CObjRep *&rep) {
	return replist.find(vp, rep); }
    virtual void* replistNext(void *curr, uval& vp, CObjRep*& rep) {
	return replist.next(curr, vp, rep); }
    virtual uval replistRemove(uval vp, CObjRep*& rep) {
	return replist.remove(vp, rep); }
    virtual uval  replistRemoveHead(CObjRep *& rep) {
	uval vp;
	return replist.removeHead(vp, rep); }
    virtual uval replistIsEmpty() {
	return replist.isEmpty(); }
    virtual uval  replistHasRep(CObjRep * rep) {
        return replist.has(rep); }
public:
    CObjRootMultiRepPinned(sval c=1):
	CObjRootMultiRepBase(c, AllocPool::PINNED) {
	/* empty body */
    }

    CObjRootMultiRepPinned(RepRef ref, sval c=1,
		     CObjRoot::InstallDirective idir=CObjRoot::Install):
	CObjRootMultiRepBase(ref, c, idir) {
	/* empty body */
    }
};


/*
 * root must be declared by the rep to return myRoot cast to the
 * type of the Root.
 * Note that field cannot be parenthesized since it is a field name
 * that the -> operator is applied to.
 */
#define COGLOBAL(field) (root()->field)

#endif /* #ifndef __COBJ_ROOT_MULTI_REP_H_ */
