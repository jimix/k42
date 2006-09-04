
#ifndef __OBJ_CACHE_H_
#define __OBJ_CACHE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ObjCache.H,v 1.7 2002/10/10 13:08:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines simple allocator for use in SMT regions
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <misc/AutoList.H>
#include <misc/BitVec.H>
#include <sync/BLock.H>

class ObjCache {
public:
    struct PageSource {
	virtual SysStatus getPage(uval &page)=0;
        DEFINE_NOOP_NEW(PageSource);
        virtual ~PageSource() { }
    };
protected:
    uval objSize;
    PageSource *ps;
    typedef BLock LockType;
    LockedAutoListHead pages;
    struct PageDesc: public AutoListNode {
	DEFINE_GLOBALPADDED_NEW(PageDesc);
	PageDesc(uval page):pAddr(page) {
	    usage[0].clearAll();
	    usage[1].clearAll();
	};
	uval pAddr;
	BitVec64 usage[2]; //This is enough bits for 128 object/page
			   // which is 32 bytes per object.
			   // Don't expect much smaller.
	~PageDesc() { detach();};
    };
public:
    DEFINE_GLOBALPADDED_NEW(ObjCache);

    ObjCache(uval itemSize, PageSource *source)
	: objSize(itemSize), ps(source) {
	  /* empty body */
    }
    virtual ~ObjCache() { /* empty body */ }

    virtual void* allocObj();
    virtual void  freeObj(void* ptr);
};

#endif /* #ifndef __OBJ_CACHE_H_ */
