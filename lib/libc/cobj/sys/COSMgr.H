#ifndef __COSMGR_H_
#define __COSMGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COSMgr.H,v 1.58 2005/08/09 12:03:13 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The Clustered Object System Manager (which itself is a
 *    Clustered Object.  There is one rep per vp.  which manges the local trans
 *    cache and the vp's portion of the main trans table.
 * **************************************************************************/
#include <cobj/sys/COSconsts.h>
#include <cobj/sys/TransEntry.H>
#include <cobj/sys/COSBreakpointObject.H>
#include <cobj/sys/CODesc.H>
#include <misc/Callback.H>

extern LTransEntry lTransTableLocal[GCOPINNEDSIZE/sizeof(TransEntry)];

class COSMgr : public BaseObj {
protected:
    static GTransEntry      *gTransTablePinned;
    static GTransEntry      *gTransTablePaged;
    static LTransEntry      *lTransTablePinned;
    static LTransEntry      *lTransTablePaged;

public:
    enum {
           gTransTableSize         = GCOTRANSTABLESIZE,
	   gPinnedSize             = GCOPINNEDSIZE,
           gTransTablePagableSize  = GCOTRANSTABLEPAGABLESIZE,
           logGPartPagableSize     = GCOLOGPAGABLESIZEPERPART,
	   gPartPagableSize        = GCOPAGABLESIZEPERPART,
	   gTransPageSize          = GCOTRANSTABLEPAGESIZE,
           logGTransPageSize       = GCOLOGTRANSTABEPAGESIZE,
	   numGTransEntriesPerPage = GCOTRANSTABLEPAGESIZE/sizeof(TransEntry),
    };

    // dummy clustered object that generates a breakpoint for all calls
    COSBreakpointObject   theBreakpointObject;
    COSBreakpointObject  *theBreakpointObjectP; // dummy cobj entry

    typedef SysStatus (*DataTransferFunction)(CObjRoot *oldRoot,
					      CObjRoot *newRoot);

    typedef uval ThreadMarker;

    enum MarkerState { ACTIVE,     // Marker still has active threads
		       ELAPSED,    // All threads before mark have terminated
    };

    /* STATIC Methods for Translation table entry conversion  */
    static uval isValid(CORef ref) {
        if  (((uval)ref >= (uval)lTransTablePinned) &&
             ((uval)ref < 
              ((uval)lTransTablePinned + (uval)gPinnedSize))
             || (lTransTablePaged && 
                 ((uval)ref >= (uval)lTransTablePaged &&
                  ((uval)ref < ((uval)lTransTablePaged +
                                (uval)gTransTablePagableSize))))) {
            return 1;
        }
        return 0;
    }

    static LTransEntry *globalToLocal(GTransEntry *gte)
	{
            if ((uval)((uval)gte - (uval)gTransTablePinned)
                < (uval)gPinnedSize) {
                return (LTransEntry *)((uval)lTransTablePinned
                                       + (uval)gte - (uval)gTransTablePinned);
            } else {
                tassertMsg(lTransTablePaged, "What used a paged entry"
                           " before the paged portion of the local table"
                           " has been setup\n");
                return  (LTransEntry *)((uval)lTransTablePaged +
                                        (uval)gte - (uval)gTransTablePaged);
            }
	}
    static GTransEntry *localToGlobal(LTransEntry *lte)
	{
	    tassertMsg((((uval)lte >= (uval)lTransTablePinned) &&
			 ((uval)lte < 
			     ((uval)lTransTablePinned + (uval)gPinnedSize)))
			|| (lTransTablePaged && 
			    ((uval)lte >= (uval)lTransTablePaged &&
			     ((uval)lte < ((uval)lTransTablePaged +
					   (uval)gTransTablePagableSize)))),
		       "lte (%p) invalid?\n", lte);
            if (((uval)lte - (uval)lTransTablePinned) < (uval)gPinnedSize) {
                return (GTransEntry *)
                    ((uval)gTransTablePinned
                     + ((uval)lte - (uval)lTransTablePinned));
            } else {
                tassertMsg(lTransTablePaged, "What used a paged entry"
                           " before the paged portion of the local table"
                           " has been setup\n");
                return (GTransEntry *)((uval)gTransTablePaged
                                       + (uval)lte - (uval)lTransTablePaged);
            }
	}
    static uval pool(CORef ref) 
        {
            LTransEntry *lte = (LTransEntry *)ref;
            if (((uval)lte >= (uval)lTransTablePinned) &&
			 ((uval)lte < 
                          ((uval)lTransTablePinned + (uval)gPinnedSize))) {
                return AllocPool::PINNED;
            }
            if (lTransTablePaged && 
			    ((uval)lte >= (uval)lTransTablePaged &&
			     ((uval)lte < ((uval)lTransTablePaged +
                                           (uval)gTransTablePagableSize)))) {
                return AllocPool::PAGED;
            }
            passertMsg(0, "not a valid ref=%p\n", ref);
            return 0;
        }
    /* provided for management of well-known clustered objects, used by
     * CObjGlobals and CObjGlobalsKern */
    static LTransEntry *indexToRef(uval index)
        {
            return &lTransTablePinned[index];
        }

    virtual SysStatus substitute(CORef target, COSMissHandler*& curMH,
                                 COSMissHandler* newMH)=0;
#if 0
    /* STATIC Methods for dynamic clustered object switching */
    static SysStatus dataTransferNOP(CObjRoot *oldRoot, CObjRoot *newRoot)
	{
	  return 0;
	}

    static SysStatus switchCObj(CORef ref, CObjRoot *newRoot,
				DataTransferFunction dataXfer = dataTransferNOP,
				sval medClusterSize = 1);
#else /* #if 0 */
    enum SwitchDispDir { DO_NOT_DESTROY, DESTROY_WHEN_DONE, REASSIGN_WHEN_DONE};

    static SysStatus switchCObj(CORef ref, CObjRoot *newRoot,
                                SwitchDispDir dir = DESTROY_WHEN_DONE,
				sval medClusterSize = 1,
				Callback *cb = NULL);
#endif /* #if 0 */

    virtual SysStatus vpMaplTransTablePaged(VPNum vp,uval useExistingAddr=0)=0;
    /* Translation Table Entry Management Methods */
    virtual SysStatus   resetLocalTransEntry(CORef ref)=0;
    virtual SysStatus   initTransEntry(GTransEntry *gte, COSMissHandler *mh,
				       CORef &ref)=0;
    virtual SysStatus   initTransEntry(CORef ref, COSMissHandler *mh)=0;
    virtual SysStatus   alloc(COSMissHandler *mh, CORef &ref, uval pool)=0;
    // FIXME: only here in this class for testing really just needs to be
    //        a protected method of COSMgrObject
    virtual SysStatus   dealloc(CORef ref)=0;

    static  SysStatus   swingRoot(CORef ref, COSMissHandler *mh);
    static  SysStatus	gteWriteComplete(CORef ref);
    static  SysStatus	gteSwitchComplete(CORef ref);
    static  SysStatus   refIsCleaned(CORef ref);

    static uval clusterSetMask(VPNum vp, VPNum clustersize);
    /* Garbage Collection Methods */
    // add an object ready for deletion
    virtual SysStatus  destroyCO(CORef ref, COSMissHandler *obj)=0;
    virtual SysStatus  reclaimRef(CORef ref, COSMissHandler *obj)=0;
    // run the destruction manager to remove memory allocated by these objects
    virtual SysStatus  checkCleanup()=0;

    // Create a marker which can be tested to see if the threads on this
    // vp at the time of creation have terminated.  Indicated by the marker's
    // state.
    virtual SysStatus  setVPThreadMarker(ThreadMarker &marker)=0;

    // Method to update and test a thread marker created with the above method.
    virtual SysStatus  updateAndCheckVPThreadMarker(ThreadMarker marker,
						    MarkerState  &state)=0;

    // Create a marker which can be tested to see if the threads across
    // the entire system at the time of invocation have terminated.  Indicated
    // by the marker's state
    virtual SysStatus  setGlobalThreadMarker(ThreadMarker &marker)=0;

    // Method to update and test a global thread marker created with above
    // method
    virtual SysStatus  checkGlobalThreadMarker(ThreadMarker marker,
					       MarkerState &state)=0;

    // method to get a list of descriptors for the active objects on a vp basis
    virtual SysStatusUval getCOList(CODesc *coDescs, uval numDescs)=0;

    // method for debugging - print out statistics
    virtual SysStatus print()=0;
};

#endif /* #ifndef __COSMGR_H_ */
