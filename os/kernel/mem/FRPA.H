#ifndef __FRPA_H_
#define __FRPA_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRPA.H,v 1.43 2005/04/15 17:39:37 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Primitive FR that obtains data via the thinwire
 * file system.
 * **************************************************************************/

#include "mem/FRCommon.H"
#include <misc/ListSimpleLocked.H>
#include <meta/MetaFRPA.H>
//#include <trace/traceMem.h>

#ifndef EXPORT_FRPA
#define EXPORT_FRPA
#endif /* #ifndef EXPORT_FRPA */

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

#include <defines/mem_debug.H>
#include "KernelPagingTransport.H"

class FRPA;
class RegionFSComm;
typedef FRPA           **FRPARef;
typedef RegionFSComm   **RegionFSCommRef;

#include "StubFileHolder.H"

class FRPA : public FRCommon  __xbase(FR) {
public:
    virtual SysStatus discardCachedPages();

protected:

    // FIXME: check this
    DEFINE_ACCESS_SERVER(
	FRPA, MetaFR::fileSystemAccess|MetaObj::controlAccess|MetaObj::attach,
	MetaObj::none);

    StubFileHolder *stubFile;

    KernelPagingTransportRef kptref;

    uval filelen;                       // current length of the file
    uval32 ohCount;			// count of user OH connections

    // FIXME: maintain a list of outstanding requests
    sval outstanding;			// num calls awaiting callback

    uval removed:1;			// file is dead once current users
					// go away

    uval fileToken;   /* when talking to the paging transport object in
		       * the file system, identify which file we correspond
		       * through this fileToken (which is a ref to the
		       * ServerFile object that should carry out requests */

#ifdef HACK_FOR_FR_FILENAMES
    /* This is the file name used when a ServerFile instance was being
     * created for the file. This information may become obsolete
     * due to operations such as rename, link, unlink, etc. */
    static const uval NAMESIZE = 64;
    char fileNameHint[NAMESIZE];
#endif //#ifdef HACK_FOR_FR_FILENAMES

    virtual SysStatus locked_destroy();

#ifdef _IN_STUBGEN
    uval locked_notInUse();
#else
    uval locked_notInUse() {
	if ((ohCount == 0) &&
	   ((fcmRef == 0) ||
	    (DREF(fcmRef)->isNotInUse() &&
	     (removed || DREF(fcmRef)->isEmpty())))) {
	    return 1;
	} else {
	    return 0;
	}
    }
#endif

    virtual SysStatus convertAddressReadFrom(uval physAddr, uval &vaddr) {
	vaddr = physAddr;
	return 0;
    }

    virtual SysStatus convertAddressWriteTo(uval physAddr, uval &vaddr, 
					    IORestartRequests *rr=0) {
	vaddr = physAddr;
	return 0;
    }

    virtual SysStatus releaseAddress(uval vaddr) {
	return 0;
    }

    SysStatus init(ObjectHandle fileOH,
		   uval len, uval fileToken,
		   char *name, uval namelen, KernelPagingTransportRef ref);

    DEFINE_REFS(FR);
    DEFINE_GLOBALPADDED_NEW(FRPA);

    /*
     * We need to count all give and release access calls and lock
     * to synchronize giveaccess and destroy.
     */
    virtual SysStatus giveAccessSetClientData(
	ObjectHandle &oh, ProcessID toProcID,
	AccessRights match, AccessRights nomatch,
	TypeID type = 0);

    virtual SysStatus handleXObjFree(XHandle xhandle);

    virtual SysStatus locked_getFCM(FCMRef &r);

public:
    // put a specified page frame data back to  offset in file
    virtual SysStatus startPutPage(uval physAddr, uval objOffset,
				   IORestartRequests *rr=0);

    virtual SysStatusUval startFillPage(uval physAddr, uval objOffset);

    virtual SysStatus fcmNotInUse();

    /* ---- support for trying IO request  -------- */

#ifdef HACK_FOR_FR_FILENAMES
    virtual SysStatus initFileName(char *name, uval namelen) {
	/* If we don't have space for the whole thing, copy only
	 *  final part of name */
	char *ptr = name;
	if (namelen != 0) {
	    passertMsg(namelen > 0, "should be > 0\n");
	    if (namelen >= NAMESIZE) {
		ptr = &name[namelen - NAMESIZE + 1];
		namelen = NAMESIZE - 1;
	    }
	    memcpy(fileNameHint, ptr, namelen);
	}
	fileNameHint[namelen] = '\0';
	//err_printf("filenameHint is %s\n", fileNameHint);
	return 0;
    }
#endif //#ifdef HACK_FOR_FR_FILENAMES

private:
    // functions called by application
    virtual SysStatus _fsync();
__xprivate:
    /*
     *  Functions called by the file system
     */

    virtual SysStatus _discardCachedPages() __xa(fileSystemAccess) {
	return discardCachedPages();
    }

    virtual SysStatus _setFileLengthFromFS (__in uval len)
	__xa(fileSystemAccess) {
	filelen = len;
	return 0;
    }
    virtual SysStatus _ioComplete(__in uval addr,
				  __in uval fileOffset,
				  __in SysStatus rc)
	__xa(fileSystemAccess);

    /*
     * request from file system to go away.
     * returns 1 if FR is again in use and cannot
     * destroy itself
     */
    virtual SysStatus _destroyIfNotInUse(__out XHandle &xh)
	__xa(fileSystemAccess);

    virtual SysStatusUval _removed() __xa(fileSystemAccess);

    virtual SysStatus _explicitFsync() 	__xa(fileSystemAccess);

__xpublic:
    static SysStatus _Create(__out ObjectHandle &oh, __CALLER_PID processID,
			     __in ObjectHandle file,
			     __in uval len,
			     __in uval filetoken,
			     __inbuf(namelen) char *name,
			     __in uval namelen);

  private:
    //FIXME temporary, just for monitoring file sizes. This should
    // go away soon. I could not #ifdef this because the stub compiler
    // didn't deal with it properly.
    static SysStatus ReportFileSize(__in uval size, __in uval readOnly);
    static SysStatus PrintReportFileSize();
    static uval howmany_ro[9];
    static uval howmany_w[9];
    // end of code to be thrown away

 public:
    static SysStatus Create(ObjectHandle &oh, ProcessID processID,
			    ObjectHandle file,
			    uval len, uval filetoken,
			    char *name, uval namelen,
			    KernelPagingTransportRef transp);

    static void ClassInit(VPNum vp);

    // attach to the FR's FCM to map (possibly) part of it
    // override to check for detach race
    virtual SysStatus attachRegion(
	FCMRef& _fcmRef, RegionRef regRef, PMRef pmRef,
	AccessMode::mode accessMode) {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	/*
	 * this used to check ohcount == 0, but that's wrong since
	 * an FR can have an ohcount of 0 and still be in use
	 * if its FCM is mapped.  This is in fact the normal
	 * situtation for memory mapped access when a forkcopy
	 * is needed.
	 */
	if (beingDestroyed) return _SDELETED(2046);
	return locked_attachRegion(_fcmRef, regRef, pmRef, accessMode);
    }


    virtual SysStatus setFileLength(uval fileLength) {
	filelen = fileLength;
	return 0;
    }
};

#endif /* #ifndef __FRPA_H_ */

