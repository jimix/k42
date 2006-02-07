#ifndef __PMKERN_H_
#define __PMKERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMKern.H,v 1.17 2004/11/03 05:14:33 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Kernel PM, that is not attached to root pm
 * **************************************************************************/

#include "mem/PM.H"

class PMKern : public PM {
protected:    
    DEFINE_PINNEDGLOBALPADDED_NEW(PMKern);
    SysStatus getList(uval count, FreeFrameList *ffl);

public:
    static SysStatus Create(PMRef &pmref);

    // attach/detach an fcm to/from this pm
    virtual SysStatus attachFCM(FCMRef fcm);
    virtual SysStatus detachFCM(FCMRef fcm);

    // attach/detach a pm to/from this pm ('this' becomes 'pm's parent)
    virtual SysStatus attachPM(PMRef pm);
    virtual SysStatus detachPM(PMRef pm);

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

    // request pages back from a pm 
    virtual SysStatus giveBack(PM::MemLevelState memLevelState);

    // request from allocator to give back cached frames - called without
    // locks
    virtual SysStatus kickPaging() {
	passertMsg(0, "should be called on root\n");
	return 0;
    }
    
    // rules for destruction not yet worked out
    virtual SysStatus destroy();

    // for testing - print summary info
    virtual SysStatus print();
};

#endif /* #ifndef __PMKERN_H_ */
