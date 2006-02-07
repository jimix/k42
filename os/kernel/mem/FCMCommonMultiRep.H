#ifndef __FCMCOMMON_MULTI_REP_H_
#define __FCMCOMMON_MULTI_REP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMCommonMultiRep.H,v 1.19 2004/10/20 18:10:28 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implements base functionality required by
 * nearly all FCMs
 * This is a pure base class - it can't be used directly
 * **************************************************************************/

#include "FCM.H"
#include "FCMCommonMultiRepRoot.H"
#include "mem/PageSet.H"
#include "mem/PageSetDense.H"
#include "mem/PageDescData.H"
#include <misc/DHashTable.H>

template<class ROOTBASE, class GALLOC, class LALLOC>
class FCMCommonMultiRep : public FCM {
protected:
#if 0
    typedef class LocalDHashTable<LocalPageDescData,MasterPageDescData,
	                          AllocLocalStrict,AllocGlobal> LHashTable;
#else
    typedef class LocalDHashTable<LocalPageDescData,MasterPageDescData,
	                          LALLOC,GALLOC>
    LHashTable;
#endif
    LHashTable localDHashTable;

    LHashTable *getLocalDHashTable() { return &localDHashTable; }


    SysStatus detectModified(LocalPageDescData* pg,
			     AccessMode::mode &access,
			     AccessMode::pageFaultInfo pfinfo);

    // support function for interacting with HAT to map a page
    virtual SysStatus mapPageInHAT(uval virtAddr,
				   AccessMode::pageFaultInfo pfinfo,
				   AccessMode::mode access, VPNum vp,
				   HATRef hat, RegionRef reg,
				   LocalPageDescData *pg,
				   uval offset, uval *retry);

    /*
     * called on a copy on write parent as well as a fork parent
     * for now we just reference count this users, but
     * in the future we will track them to implement copy on write,
     * where we must be able to call back to unmap frames.
     */
    virtual SysStatusUval detachForkChild(
	FCMComputationRef child, uval lastChildCount) {
	return COGLOBAL(removeReference());
    }

    virtual SysStatusUval adoptChildAndLock(
	FCMComputationRef child, FRRef& parentFR,
	uval& count, uval& unmapped, uval& parentPages) {
	COGLOBAL(lock.acquire());
	return 0;			// refuse
    }

    virtual SysStatus locked_completeAdoption() {
	COGLOBAL(lock.release());
	return 0;
    }

    virtual SysStatus unLockPage(uval token);

    virtual ~FCMCommonMultiRep() { localDHashTable.cleanup(); }
    FCMCommonMultiRep(uval numPages,uval minSize) : 
        localDHashTable(numPages, minSize){ 
//        err_printf("CRep n=%ld m=%ld\n",numPages,minSize);
    }
    FCMCommonMultiRep() : localDHashTable(){
//        err_printf("CRep\n");
    }
public:
    FCMCommonMultiRepRoot<ROOTBASE,GALLOC,LALLOC> *root() {
	return (FCMCommonMultiRepRoot<ROOTBASE,GALLOC,LALLOC> *)myRoot;
    }

    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode) {
	return COGLOBAL(attachRegion(regRef, pmRef, accessMode));
    }

    // detach strategy differs for each kind of FCM
    virtual SysStatus detachRegion(RegionRef regRef)=0;

    // when an fr/fcm pair is build externally, the fr must connect
    virtual SysStatus attachFR(FRRef frRefArg){
	tassert(COGLOBAL(frRef) == NULL, err_printf("already attached\n"));
	COGLOBAL(frRef) = frRefArg;
	return 0;
    }

    virtual SysStatus getFRRef(FRRef& frRefArg) {
	frRefArg = COGLOBAL(frRef); return 0;}

    virtual SysStatus destroy() {
	return COGLOBAL(destroy());
    }

    // provide a frame mapping to the FCM
    virtual SysStatus establishPage(uval offset, uval vMapsRAddr,
				    uval length){
	return COGLOBAL(establishPage(offset, vMapsRAddr, length));
    }

    virtual SysStatus establishPagePhysical(uval offset, uval paddr,
					    uval length) {
        SysStatus retvalue;
	retvalue = 
             COGLOBAL(establishPage(offset,
			      PageAllocatorKernPinned::realToVirt(paddr),
		              length));
	return(retvalue);
    }

    // Unwire established pages - FCM may now page them and
    // return frames to PageManager
    virtual SysStatus disEstablishPage(uval offset, uval length) {
	tassert(0, err_printf("disEstablishPage\n"));
	return -1;
    }

    // Remove an established page.  Unlike disEstablish, in this case
    // the page is forgotten by the FCM.  The caller is responsible for
    // the allocation of the page.  The vMapsRAddr of the page is returned.
    virtual SysStatus removeEstablishedPage(
	uval offset, uval length, uval &vMapsRAddr) {
	return COGLOBAL(removeEstablishedPage(offset, length, vMapsRAddr));
    }

    // If something other than a region needs to prevent FCM destruction
    // is must add a reference (count).  see ProcessVPList for example.
    virtual SysStatus addReference() {
	return COGLOBAL(addReference());
    }

    virtual SysStatus removeReference() {
	return COGLOBAL(removeReference());
    }

    virtual SysStatusUval isNotInUse() {
	return COGLOBAL(isNotInUse());
    }

    virtual SysStatusUval isEmpty() {
	return COGLOBAL(isEmpty());
    }

    virtual SysStatus notInUse() {
	return COGLOBAL(notInUse());
    }

    // unmap and throw away all pages - even dirty ones!
    virtual SysStatus discardCachedPages() {
	return COGLOBAL(discardCachedPages());
    }

    // non zero if this fcm supports shared segments
    virtual SysStatus sharedSegments() {
	return COGLOBAL(noSharedSegments)?0:1;
    }
	
    // used for debugging
    virtual SysStatus printStatus(uval kind);
};

#endif /* #ifndef __FCMCOMMON_MULTI_REP_H_ */
