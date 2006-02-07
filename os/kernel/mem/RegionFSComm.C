/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionFSComm.C,v 1.30 2004/10/29 16:30:33 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Special region for kernel/File system
 * communication.  The FR in the kernel plugs individual pages into
 * this region to get them in the file systems address space when
 * sending physical pages to the file system to handle faults.
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/Access.H"
#include "mem/RegionFSComm.H"
#include "mem/FCM.H"
#include "mem/FRPlaceHolder.H"
#include "mem/FCMFile.H"
#include "meta/MetaRegionFSComm.H"
#include <cobj/CObjRootSingleRep.H>

#define NUM_DEFS
uval ok_attchtry = 0;
uval ok_attchfail = 0;
uval ok_attchblk = 0;
uval ok_attchfin = 0;
uval ok_attchpush = 0;
uval ok_detach = 0;

uval ok_enqueue=0;
uval ok_crthread=0;
uval ok_sleepthread = 0;
uval ok_wokethread = 0;
uval ok_wakethread = 0;
uval ok_wakeFCM = 0;

#include "mem/IORestartRequests.H"

/* virtual */ SysStatus
RegionFSComm::attachPage(uval paddr, uval &vaddr, IORestartRequests *rr)
{
    uval bitOffset;

    commLock.acquire();
    ok_attchtry++;
    do {
	for(bitOffset=0; (bitOffset < RegionPageCount) &&
		(pagesUsed & (((uval)1)<<bitOffset));
	    bitOffset++);
	if (bitOffset >= RegionPageCount) {
	    if (rr) {
		rr->enqueue(head);
		ok_attchfail++;
		commLock.release();
		return _SERROR(2860, FR::WOULDBLOCK, EBUSY);
	    }

	    // couldn't find a page, no IORes...provided, block
	    ok_attchblk++;
	    IORestartRequests srr;
	    srr.enqueue(head);
	    commLock.release();
	    srr.wait();
	    commLock.acquire();
	}
    } while (bitOffset >= RegionPageCount);
    pagesUsed = pagesUsed | (((uval)1)<<bitOffset);

    // now allocated page
    vaddr = regionVaddr + bitOffset*PAGE_SIZE;

    // remember the paddr for later
    index[bitOffset] = paddr;
    ok_attchfin++;
    commLock.release();
    return 0;
}


/* virtual */ SysStatus
RegionFSComm::attachPagePush(uval paddr, uval &vaddr, IORestartRequests *rr)
{
    ok_attchpush++;
    SysStatus rc = attachPage(paddr,vaddr, rr);

    if(_SUCCESS(rc)) {
	uval srcAddr = PageAllocatorKernPinned::realToVirt(paddr);

	memcpy((void*)(vaddr - regionVaddr + (uval)buf),(void*)srcAddr, 
	       PAGE_SIZE);
    }
    return rc;
}

/* virtual */ SysStatus
RegionFSComm::detachPage(uval vaddr)
{
    uval offset = vaddr - regionVaddr;
    uval bitOffset = offset/PAGE_SIZE;
    IORestartRequests *copy;

    commLock.acquire();
    tassert( ((bitOffset>=0)&&(bitOffset<RegionPageCount)&&
	      (pagesUsed&(((uval)1)<<bitOffset))),
	     err_printf("bad bitOffset\n"));

    uval pageID = (vaddr - regionVaddr) / PAGE_SIZE;

    ok_detach++;
    if (index[pageID] != uval(-1)) {
	memcpy((void*)PageAllocatorKernPinned::realToVirt(index[pageID]),
	       (void*)((uval)buf + pageID*PAGE_SIZE),
	       PAGE_SIZE);
	index[pageID] = uval(-1);
    }
    
    pagesUsed = pagesUsed  & ~(((uval)1)<<bitOffset);

    copy = head;
    head = 0;
    commLock.release();
    IORestartRequests::NotifyAll(copy);
    return 0;
}

/* static */ SysStatus
RegionFSComm::_Create(__out uval &vaddr, __CALLER_PID caller)
{
    ProcessRef pref=0;
    FCMRef fcmRef;
    SysStatus rc;
    FRRef frStartRef;
    uval buf;
    uval regionSize = RegionPageCount*PAGE_SIZE;	// one word of bits

    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(buf, regionSize+PAGE_SIZE);
    tassert(_SUCCESS(rc),
	    err_printf("shared buf allocation failed: %016lx\n",rc));

    rc = PrefFromTarget(0, caller, pref);
    tassert(_SUCCESS(rc), err_printf("should be able to work on own proc\n"));

    // create FR to back FCM
    FCMFile::Create(fcmRef, NULL, 0/*not pageable*/);

    FRPlaceHolder::Create(frStartRef);
    rc = DREF(frStartRef)->installFCM(fcmRef);

    RegionFSComm* reg = new RegionFSComm;
    reg->init((void*)buf);
    reg->index = (uval*)(buf + regionSize);

    CObjRootSingleRep::Create(reg);
    rc = reg->initRegion(pref, vaddr, 0, regionSize, 0, frStartRef, 1, 0,
			 AccessMode::writeUserWriteSup, 0,
			 RegionType::K42Region);
    reg->commLock.init();
    reg->head = NULL;

    tassert( _SUCCESS(rc), err_printf("woops: %016lx\n",rc));

    for(uval i = 0; i<RegionPageCount; ++i) {
	// map the pages in from our buffer
	rc = DREF(fcmRef)->
	    establishPage(i*PAGE_SIZE, buf+i*PAGE_SIZE,PAGE_SIZE);
	tassert( _SUCCESS(rc), err_printf("woops: %016lx\n",rc));
	reg->index[i] = uval(-1);	// when not -1 address of read target
    }
    return rc;
}

/* virtual */ SysStatus
RegionFSComm::destroy()
{
    /*
     * file system isn't gonna go away any time soon,
     * eventually, change this to detach all pages and kill FR/FCM
     * to generalize functionality
     */
    passertMsg(0, "NYI: attempt to delete RegionFSComm\n"
	       "this means that file system has died\n");
    return 0;
}

/* static */ void
RegionFSComm::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaRegionFSComm::init();
}

/* virtual */ SysStatus
RegionFSComm::getType(TypeID &id)
{
    id = MetaRegionFSComm::typeID();
    return 0;
}
