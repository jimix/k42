#ifndef __PAGE_DESC_H_
#define __PAGE_DESC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageDesc.H,v 1.19 2004/10/29 16:30:32 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines basic functionality common to many page
 * descriptors.  This is used within FCMs to try to maintain some common
 * code inheritence.  It should not be leaked outside of FCMs, since we
 * do not want to constrain the implementation of FCMs to a particular
 * page descriptor, at least not from an interface perspective.
 *
 * FIXME: current FCMs all acquire large granularity locks, what we
 * need to do is move the lock inside this class, and get rid of large
 * granularity per-FCM locks.
 * **************************************************************************/

class PageFaultNotification;

struct PageDesc {
    uval      paddr;			// physical address

    // FIXME: get rid of this and move into PageList::Entry, PageSetDense
    // doesn't require
    uval      fileOffset;		// offset in file it corresponds to

    enum { CLEAR=0, SET=1 };		// for flags
    // list of flags
    // FIXME: diff between used and mapped?
    // N.B. if you add/change flags fix setflags below
    uval      used:1;			// has the page been referenced?
    uval      dirty:1;			// was page modified ?
    uval      doingIO:1;		// I/O in progress for page
    uval      freeAfterIO:1;		// free page after write I/O callback
    uval      mapped:1;			// any mappings in force?
    uval      cacheSynced:1;		// is i cache syncronized
    uval      free:1;			// on freelist
    uval      established:1;            // pinned because it was established
    uval      forkCopied1:1;		// child 1 has it
    uval      forkCopied2:1;		// child 2 has it
    uval      forkIO:1;			// forkIO request outstanding
    uval      forkIOLock:1;		// a thread is working on this
    uval      forkMapParent:1;		// represents a parent page
    uval      copiedOnFork:1;		// frame was aggressively copied
    uval8     pinCount;			// number of pin requests

    uval32    len;			// number of bytes
    PageFaultNotification *fn;		// list things to be notified
    uval      ppset;			// phys. procs possibly mapping page

    DEFINE_NOOP_NEW(PageDesc);
    void init(uval fo, uval pa, uval ln) {
        //FIXME - assuming compiler is stupid, we should zero
	//        the flags by kludge for performance.
	fileOffset = fo; paddr = pa; len = ln; used = PageDesc::SET;
	dirty = PageDesc::CLEAR; doingIO = PageDesc::CLEAR;
	mapped = PageDesc::CLEAR; cacheSynced = PageDesc::CLEAR;
	freeAfterIO = PageDesc::CLEAR; free = PageDesc::CLEAR;
	established = PageDesc::CLEAR;
	forkCopied1 = PageDesc::CLEAR; forkCopied2 = PageDesc::CLEAR;
	forkIO = PageDesc::CLEAR;
	forkIOLock = PageDesc::CLEAR;
	forkMapParent = PageDesc::CLEAR;
	copiedOnFork = PageDesc::CLEAR;
	fn = NULL; ppset = 0;
	pinCount = 0;
    }

    void setFlags(PageDesc* pg) {
#define __PAGEDESC_COPYFLAG(flag) flag = pg->flag
	__PAGEDESC_COPYFLAG(used);
	__PAGEDESC_COPYFLAG(dirty);
	__PAGEDESC_COPYFLAG(doingIO);
	__PAGEDESC_COPYFLAG(freeAfterIO);
	__PAGEDESC_COPYFLAG(mapped);
	__PAGEDESC_COPYFLAG(cacheSynced);
	__PAGEDESC_COPYFLAG(free);
	__PAGEDESC_COPYFLAG(established);
	__PAGEDESC_COPYFLAG(forkCopied1);
	__PAGEDESC_COPYFLAG(forkCopied2);
	__PAGEDESC_COPYFLAG(forkIO);
	__PAGEDESC_COPYFLAG(forkIOLock);
#undef __PAGEDESC_COPYFLAG
    }
    virtual void destroy() = 0;		// allow delete using superclass

    // an enum for quen dequeing from free list 
    // to define if you want to remove from the head or if you want a 
    // clean or dirty element specifically
    enum dqtype {DQ_CLEAN, DQ_DIRTY, DQ_HEAD };
};

#endif /* #ifndef __PAGE_DESC_H_ */
