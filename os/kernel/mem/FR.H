#ifndef __FR_H_
#define __FR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FR.H,v 1.46 2004/10/29 16:30:31 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The base class from which all File
 * Representatives (FRs) are derived.
 * **************************************************************************/

#include <meta/MetaFR.H>
#include <cobj/TypeMgr.H>
#include <mem/Access.H>

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#undef passertMsg
#define passertMsg(COND, MSG)
#endif /* #ifdef _IN_STUBGEN */

class FR;
typedef FR **FRRef;
class Region;
typedef Region **RegionRef;
class PM;
typedef PM **PMRef;
class HAT;
typedef HAT **HATRef;

/* the following object is passed to potentially blocking code paths
   to provide a structure that can be used to block I/O requests */
class IORestartRequests;		


class FR : public Obj __xadef(fileSystemAccess) {
  public:
    enum ClassCodes { 
	WOULDBLOCK=1 			// error request would block
    };
  protected:
#ifndef _IN_STUBGEN
    // getFCM is dangerous - only allow use from other FR's and FCM's
    // normal calls request on FCM service from the FR
    // see attachRegion for example.
    // friend SysStatus FCM::getFCM(FRRef, FCMRef&);
    // tried to make only the one function in FCM a friend but can't
    friend class FCM;
    friend class FSFRSwap;
#endif

    // get the FCM associated with this FR
    // must only be used if the FCM is guaranteed to continue to
    // exist
    virtual SysStatus getFCM(FCMRef &r)=0;

public:
    DEFINE_ACCESS_SERVER(
	FR,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    /* used by some FR's to signal that the requested page does
     * not exist in the file.  Normally, the FR does not "know" this
     * until the IO request completes, but if the FR caches meta data
     * it may be able to respond immediately
     */

    enum{PAGE_NOT_FOUND=1};

    // attach to the FR's FCM to map (possibly) part of it
    virtual SysStatus attachRegion(
	FCMRef& fcmRef, RegionRef regRef, PMRef pmRef,
	AccessMode::mode accessMode) = 0;

    // attach a copy FCM to the source FCM associated with the FR
    // and return the source FCM reference
    // see detachChild in FCM for disconnecting
    virtual SysStatus attachFCM(FCMRef &f, FCMRef attachingFCMRef) = 0;
    
    // fill a specified page frame with data fetched from the offset in file
    // may return PAGE_NOT_FOUND rather than starting the IO
    virtual SysStatusUval startFillPage(uval physAddr, uval objOffset)=0;

    /** put a specified page frame data back to  offset in file
     * if page extends or is beyond current file length, FR saves the (rest)
     * of the data in private memory.
     * async version with callback
     * the IORestartReuquests is a data structure used to enqueue
     * FCM when request would otherwise block for async call back
     */
    virtual SysStatus startPutPage(uval physAddr,  uval objOffset, 
				   IORestartRequests *rr=0)=0;

    virtual SysStatus detachFCM()=0;

    virtual SysStatus destroy()=0;

    // if the FR does not already have an FCM use this else return error
    virtual SysStatus installFCM(FCMRef fcmRef)=0;

    virtual SysStatus getType(TypeID &id);

    virtual SysStatus fcmNotInUse()=0;

    // paging only
    // returns 0 if fileOffset exists in file, 1 otherwise
    virtual SysStatus checkOffset(uval fileOffset)=0;

    // give a disk block to this FR
    virtual SysStatus setOffset(uval fileOffset, uval blockID)=0;

    // free disk block associated with this fileOffset
    virtual SysStatus freeOffset(uval fileOffset)=0;

    // get a disk block from the FR
    virtual SysStatus getBlockID(uval fileOffset, uval& blockID)=0;

    virtual SysStatus setFileLength(uval fileLength)=0;

    virtual SysStatusUval getPageSize() {
	return _SRETUVAL(PAGE_SIZE);
    }
    
__xprivate:
    virtual SysStatus _fsync()=0;
};

#endif /* #ifndef __FR_H_ */
