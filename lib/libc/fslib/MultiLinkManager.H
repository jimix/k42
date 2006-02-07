#ifndef __MULTI_LINK_MANAGER_H_
#define __MULTI_LINK_MANAGER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MultiLinkManager.H,v 1.2 2004/04/21 18:52:12 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * Embedded object used to keep track of files with multiple links.
 * parameter unique is file system specific
 * FIXME: should just change this to using HashSimpleLocked
 * **************************************************************************/
#include <misc/HashSimple.H>
#include <fslib/FSFile.H>

class MultiLinkManager {
public:
    MultiLinkMgrLock lock;
    typedef struct {
	ServerFileRef fref;
	BLock lock;
    } SFHolder;
private:
    HashSimple<uval,SFHolder*,AllocGlobal,0> linkTable;
public:
    static SFHolder* AllocHolder(ServerFileRef fref) {
	SFHolder * ref = (SFHolder*)
	    AllocGlobalPadded::alloc(sizeof(SFHolder));
	ref->fref = fref;
	ref->lock.init();
	return ref;
    }
    static void FreeHolder(SFHolder* ref) {
	tassertMsg((!ref->lock.isLocked()), "freeing with lock held\n");
	AllocGlobalPadded::free(ref, sizeof(SFHolder));
    }
    
    void init() { lock.init(); }
    void locked_add(uval unique, SFHolder *ref) {
	_ASSERT_HELD(lock);
	linkTable.add(unique, ref);
    }
    // != 0 if found something
    uval locked_search(uval unique, SFHolder* &ref) {
	_ASSERT_HELD(lock);
	return linkTable.find(unique, ref);
    }
    void acquireLock() { lock.acquire(); }
    void releaseLock() { lock.release(); }
    
    // operations that acquire lock internally
    uval remove(uval unique) {
	AutoLock<BLock> al(&lock);
	SFHolder* tmp;
	uval ret = linkTable.remove(unique, tmp);
	if (ret == 1) {
	    FreeHolder(tmp);
	} else {
#ifdef DILMA_DEBUG_ONLY
	    // Let's look who is triggering this wrong removal request
	    tassertMsg(0, "remove didn't find element\n");
#else
	    return 1;
#endif
	}
	return ret;
    }
    void add(uval unique, SFHolder *ref) {
	AutoLock<BLock> al(&lock);
	linkTable.add(unique, ref);
    }
};
#endif /* __MULTI_LINK_MANAGER_H_ */
