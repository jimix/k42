#ifndef __FRCOMPUTATION_H_
#define __FRCOMPUTATION_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRComputation.H,v 1.45 2004/10/29 16:30:31 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Paging FR
 *
 * Paging is done differently that file IO.  In files, the meta data is
 * in the file server.
 *
 * For paging, each FR maintains a map from offset to paging block.
 * The FSSwap object actually maps paging blocks to files or paging devices.
 * We assume that the FSSwap mapping is very simple, mapping large blocks
 * of contigous block numbers to a device or file.
 *
 * This implementation makes it possible to cheaply learn if an offset
 * is stored, and to move paging disk blocks from one FR to another.  Both
 * operations are needed for fork.
 * **************************************************************************/

#include "mem/FRCommon.H"
#include <meta/MetaFRComputation.H>
#include <misc/HashSimple.H>

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class FRComputation;
typedef FRComputation **FRComputationRef;
class FCMComputation;
typedef FCMComputation** FCMComputationRef;

class FRComputation : public FRCommon __xbase(FR) {

 protected:

// stubgen can't parse this and doesn't need to see it
#ifndef _IN_STUBGEN
    HashSimple<uval, uval, AllocGlobalPadded, LOG_PAGE_SIZE> blockHash;
    volatile uval pagerContext;		// provides the pager a means
					// to keep track of this object
#endif /* #ifndef _IN_STUBGEN */

    uval pageable;			// indicates if we have a swap file
    uval numanode;			// node to allocate pages from
    DEFINE_GLOBAL_NEW(FRComputation);

    DEFINE_REFS(FRComputation);

    virtual SysStatus init(uval numanode);

    static SysStatus InternalCreate(ObjectHandle &frOH, ProcessID toProcID,
				    uval pgSize, uval numanode);

    SysStatus putPageInternal(uval physAddr, uval offset, uval async, 
			      IORestartRequests *rr);

public:
    DEFINE_ACCESS_SERVER(
	FRComputation,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    static void ClassInit(VPNum vp);

    // create used internally, for example by forkCopy
    static SysStatus Create(FRRef& frRef, uval pgSize);

    // used by fork to transfer fr contents to new parent
    // call is made on old parent, passing ref of new parent
    virtual SysStatus forkCopy(FCMComputationRef parent);

    virtual SysStatus putPage(uval physAddr, uval objOffset);

    virtual SysStatus startPutPage(uval physAddr, uval objOffset,
				   IORestartRequests *rr=0);

    virtual SysStatusUval startFillPage(uval physAddr, uval objOffset);

    virtual SysStatus getType(TypeID &id);

    virtual SysStatus _fsync();

    virtual SysStatus destroy();

    virtual SysStatus locked_getFCM(FCMRef &r);

    virtual SysStatus _giveAccess(__out ObjectHandle &oh,
				  __in ProcessID toProcID);

    virtual SysStatus exportedXObjectListEmpty();

    virtual SysStatus ioComplete(uval fileOffset, SysStatus rc);

    // returns 0 if fileOffset exists in file, 1 otherwise
    virtual SysStatus checkOffset(uval fileOffset);

    // give a disk block to this FR
    virtual SysStatus setOffset(uval fileOffset, uval blockID);

    // free disk block associated with this fileOffset
    virtual SysStatus freeOffset(uval fileOffset);

    // get a disk block from this FR
    // blockID = uval(-1) if not found
    // block is removed from this FR but NOT freed.
    // caller must add it to another FR.
    virtual SysStatus getBlockID(uval fileOffset, uval& blockID);

    //see FCMComputation - used to detect leaks in debug builds
    virtual SysStatus maanofcm() {
	return fcmRef?0:1;
    }
    
__xpublic:
    static SysStatus _Create(__out ObjectHandle &frOH,
			     __CALLER_PID caller);
    static SysStatus _CreateLargePage(__out ObjectHandle &frOH,
				      __in uval pgSize,
				      __CALLER_PID caller);
    static SysStatus _CreateFixedNumaNode(__out ObjectHandle &frOH,
					  __in  uval numanode,
					  __CALLER_PID caller);
};
#endif /* #ifndef __FRCOMPUTATION_H_ */
