#ifndef __PMLEAF_H_
#define __PMLEAF_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMLeaf.H,v 1.33 2005/03/02 05:27:57 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Leaf PMs (generally for a given Process); has no
 * children PMs, only FCMs.
 * **************************************************************************/

#include <misc/ListSimpleKey.H>
#include <scheduler/Scheduler.H>
#include "mem/PM.H"
#include <cobj/DTType.H>
#include <cobj/CObjRootSingleRep.H>
#include <cobj/Factory.H>

class PMLeaf : public PM {
protected:
    typedef BLock LockType;
    LockType      lock;			// lock on object

    PMRef parentPM;
    uval  refCount;
    uval  nonPageable;

    uval  numFCM;

    // list of FCMs attach
    ListSimple<FCMRef, AllocGlobal> FCMList;

    void locked_print();

    void scanFCMs(PM::MemLevelState memLevelState);

    FCMRef lastScanned;			// last fcm to be scanned

    PMRef uninitPM() { return PMRef(TheBPRef); }
    uval  uninitPM(PMRef pm) { return pm == PMRef(TheBPRef); }

    PMLeaf() {
	refCount = 0; lastScanned = NULL; numFCM = 0; nonPageable = 0;
	lock.init(); 
    }

    DEFINE_GLOBALPADDED_NEW(PMLeaf);
public:
    DEFINE_FACTORY_BEGIN(Factory);
      virtual SysStatus createReplacement(CORef ref, CObjRoot *&root);
      virtual SysStatus create(PMRef &pmref, PMRef parentPM);
    DEFINE_FACTORY_END(Factory);
    virtual SysStatus markNonPageable() {nonPageable = 1; return 0;}

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

class PMLeafRoot : public CObjRootSingleRep {
    virtual SysStatus getDataTransferExportSet(DTTypeSet *set) {
        set->addType(DTT_TEST);
        return 0;
    }
    virtual SysStatus getDataTransferImportSet(DTTypeSet *set) {
        set->addType(DTT_TEST);
        return 0;
    }

    virtual DataTransferObject * dataTransferExport(DTType dtt, VPSet dtVPSet);

    virtual SysStatus dataTranferImport(DataTransferObject *data, DTType,
                                        VPSet dtVPSet);
    friend class PMLeafExpRoot;
public:
    DEFINE_GLOBAL_NEW(PMLeafRoot);

    PMLeafRoot(PMLeaf *rep) : CObjRootSingleRep((CObjRep *)rep) {
      /* empty body */
    }
    PMLeafRoot(PMLeaf *rep, RepRef ref,
		   CObjRoot::InstallDirective idir=CObjRoot::Install) :
	CObjRootSingleRep((CObjRep *)rep, ref, idir) {
	/* empty body */
    }
    virtual SysStatus deRegisterFromFactory();
};
#endif /* #ifndef __PMLEAF_H_ */
