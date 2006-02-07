#ifndef __PMROOT_H_
#define __PMROOT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000-2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMRoot.H,v 1.35 2005/06/20 06:59:51 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Root of all PMs; also has orphaned FCMs as children, as
 * as shareable FCMs (at least for now until we add a special file cache PM).
 *
 * The implementation is mostly copied from PMLeaf; implementation
 * inheritance was an option, but they would like diverge quite quickly.
 * **************************************************************************/

#include <cobj/CObjRootMultiRep.H>
#include <misc/HashSimple.H>
#include <scheduler/Scheduler.H>
#include "mem/PM.H"

class PMRoot : public PM {
protected:
    typedef BLock LockType;
    class MyRoot;

    uval exploitMultiRep();
    
    friend class MyRoot;
    // conventional access function used by COGLOBAL
    MyRoot* root() {return (MyRoot*)(myRoot);}

    // quick list of free frames guarded by lock for now
    LockType      repLock;

    static const uval ELEM_IN_FREE_FRAME_LIST = FreeFrameList::NATURAL_ALLOC*4;

    FreeFrameList freeFrameList;
    MemLevelState currMemLevel; 	// memory available
    FCMRef lastFCMScanned;		// last fcm to be scanned    
    uval  numFCM;			// number of attached FCM's

    // attached FCM's, no status anymore, so bogus second value
    HashSimple<FCMRef, uval, AllocGlobal, 4> FCMHash;

    PMRoot();

    void locked_print();

    uval getFrame(uval& addr);

    SysStatus getFreeFrameList(uval count, FreeFrameList *ffl);
    
    void freeList(FreeFrameList *ffl);

    void freeFrame(uval addr);

    // set the memory level in all reps, give back frames if changed down
    void setMemLevel(MemLevelState memLevelState);

    void scanFCMs();

    void printFCMList();

    //FIXME:  If we decide to function ship these will need to make
    //        them virtual so that they can be invoked via a DRef.
    //        But for the moment since I have choosen to reach in via
    //        a rep pointer I am not making the virtual
    void repAttachFCM(FCMRef fcm);
    void repDetachFCM(FCMRef fcm);

    virtual SysStatus establishRep();

    virtual SysStatus allocPagesInternal(uval &ptr, uval size, uval pageable,
					 uval flags, VPNum node);
public:
    // create singleton class for system
    static void ClassInit(VPNum vp, uval exploitMultiRep = 1);
    // second phase for parge pages
    virtual SysStatus ClassInit2(VPNum vp);

    // attach/detach an fcm to/from this pm
    virtual SysStatus attachFCM(FCMRef fcm);
    virtual SysStatus detachFCM(FCMRef fcm); 

    // attach/detach a pm to/from this pm ('this' becomes 'pm's parent)
    virtual SysStatus attachPM(PMRef pm);
    virtual SysStatus detachPM(PMRef pm);

    virtual SysStatus kickPaging();


    /* the following are taken from PageAllocator; we could use subclassing,
     * but we lose much of the benefit with the cobj double indirections
     */

    // allocates contiguous region of memory,
    virtual SysStatus allocPages(FCMRef fcm, uval &ptr, uval size,
				 uval pageable, 
				 uval flags=0,
				 VPNum node=PageAllocator::LOCAL_NUMANODE);
    virtual SysStatus allocPages(PMRef pm, uval &ptr, uval size,
				 uval pageable, 
				 uval flags=0,
				 VPNum node=PageAllocator::LOCAL_NUMANODE);

    // list of standard size, standard node, frames
    virtual SysStatus allocListOfPages(FCMRef fcm, uval count, 
				       FreeFrameList *ffl);
    virtual SysStatus allocListOfPages(PMRef pm, uval count, 
				       FreeFrameList *ffl);

    // free memory at specified physical address
    virtual SysStatus deallocPages(FCMRef fcm, uval paddr, uval size);
    virtual SysStatus deallocPages(PMRef pm, uval paddr, uval size);

    // free memory at addresses specified in a list
    virtual SysStatus deallocListOfPages(FCMRef fcm, FreeFrameList *ffl);
    virtual SysStatus deallocListOfPages(PMRef pm, FreeFrameList *ffl);

    /* End PageAllocator stuff */

    // various entities might hold references to us for adding to the
    // hierarchy, this allows them to logically obtain/release their reference
    virtual SysStatus attachRef();
    virtual SysStatus detachRef();

    // request pages back from a pm and current summary info
    virtual SysStatus giveBack(PM::MemLevelState memLevelState);

    // Push all free frames back to allocator
    virtual SysStatus pushAllFreeFramesAllReps();
    // Intended to be used for debugging - eg measuring fragmentation of
    // free memory

    // Push all free frames back to allocator
    virtual SysStatus pushAllFreeFrames();
    // Just for this rep

    // rules for destruction not yet worked out
    virtual SysStatus destroy();

    // for testing - print summary info
    virtual SysStatus print();

    virtual SysStatus printFreeListStats();

    virtual SysStatus getLargePageInfo(uval& largePgSize,
				       uval& largePgReservCount,
				       uval& largePgFreeCount);

    // not sure if this really needs to be pinned, since the pmleafs are not
    // pinned, but just to be safe for now
    DEFINE_PINNEDGLOBALPADDED_NEW(PMRoot);

};


#endif /* #ifndef __PMROOT_H_ */
