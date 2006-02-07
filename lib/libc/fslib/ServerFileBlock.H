#ifndef __SERVER_FILE_BLOCK_H_
#define __SERVER_FILE_BLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlock.H,v 1.34 2005/01/10 15:27:22 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Common implementation of block file system,
 * maintains relationship with paging object (FR) in memory manager.
 * The *only* calls that should be in a derived objects are the calls
 * related to actual paging, where we guarantee here that the FR stays
 * valid while paging may be occuring
 * **************************************************************************/

#include "ServerFile.H"
#include <stub/StubKernelPagingTransport.H>

template<class STUBFR> class ServerFileBlock : public ServerFile {
protected:
    /*
     * This templated class encapsulates a stubFR.  Since Stub objects
     * cannot be dynamically allocated, need some form of holder object.
     */
    class StubFRHolder {
    public:
	STUBFR stub;

	StubFRHolder():stub(StubObj::UNINITIALIZED) { /* empty body */ }

	DEFINE_GLOBAL_NEW(StubFRHolder);
    };

    // fr in memory manager for this file, if 0 is not valid
    StubFRHolder *stubFR;

    /* stub to communicate with kernel MM paging transport.
     * This is provided by the file system creation */
    StubKernelPagingTransport stubKPT;

    /*
     * Protects from more than one detack to FR at a time.  Can also be used
     * if it is critical to protect FR from disapearing while invoking it.
     */
    BLock stubDetachLock;

    // invoked with stubLockDetach locked
    virtual SysStatus locked_createFR()=0;

    virtual SysStatus locked_setFileLength(uval len) {
	_ASSERT_HELD_PTR(getLockPtr());
	AutoLock<BLock> al(&stubDetachLock); // locks now, unlocks on return
	// may not be attached, in which case don't care
	if (stubFR != NULL) {
	    stubFR->stub._setFileLengthFromFS(len);
	}
	fileLength = len;
	return 0; // always return 0, since FR can go away
    }

    virtual SysStatus ftruncate(off_t len);

    /*
     * Tries to Detaches from FR. If it succeeds, returns 1, otherwise 0
     */
    uval detachFR();

    virtual SysStatus destroy();

    virtual uval locked_isThereNonFRClient();

    virtual void frInteractionForDeletion();

    /* runFreeProtocol() detects if the file is free for destruction.
     * It returns 1 if file can be destroyed, 0 otherwise.
     */
    virtual SysStatusUval runFreeProtocol(uval nextPhase);

    virtual SysStatus init(ObjectHandle kpt) {
	stubFR = NULL;
	stubDetachLock.init();
	stubKPT.setOH(kpt);
	return ServerFile::init();
    }

public:
    ServerFileBlock<STUBFR>() : stubKPT(StubBaseObj::UNINITIALIZED) {};
    virtual SysStatus getFROH(ObjectHandle &oh, ProcessID pid) {
	SysStatus rc;
	AutoLock<BLock> al(&stubDetachLock); // locks now, unlocks on return
	tassertMsg(!removed, "getFROH on removed FS\n");
	if (stubFR == NULL) {
	    locked_createFR();
	}
	rc = stubFR->stub._giveAccess(oh, pid);
	return rc;
    }

    virtual SysStatus frIsNotInUse();

    // blow away cached pages of a file, requires at least write access
    // hence this goes through file system for access checks (NYI)
    virtual SysStatus _discardCachedPages();

    virtual SysStatus handleXObjFree(XHandle xhandle);

    virtual SysStatus _getSharedBuf(__out ObjectHandle &oh, __out uval &offset,
				    __inout uval &length, __XHANDLE xhandle,
				    __CALLER_PID pid);

    virtual SysStatus _syncSharedBuf(__in uval newLength, //Total len of file
				     __in uval start,     //Start mod data
				     __in uval modLen,    //Length mod data
				     __in uval release,
				     __XHANDLE xh,
				     __CALLER_PID pid);
    
    /* We used to have ServerFile objects offering the paging interfaces
     * directly to the FR, but now the FR talks to a PagingTransport object.
     * But we are still giving a ServerFile oh to the FR, so that we
     * keep the current scheme that the SF only goes away after the FR
     */
    virtual __async SysStatus _startWrite(uval virtAddr, uval objOffset,
					  uval len, __XHANDLE xhandle) {
	passertMsg(0, "This shouldn't be called\n");
	return _SERROR(2850, 0, 0);
    }

    virtual __async SysStatus _startFillPage(uval virtAddr, uval objOffset,
					     __XHANDLE xhandle) {
	passertMsg(0, "This shouldn't be called\n");
	return _SERROR(2851, 0, 0);
    }

    virtual __async SysStatus _frIsNotInUse(uval fileToken) {
	passertMsg(0, "This shouldn't be called\n");
	return _SERROR(2852, 0, 0);
    }

    virtual __async SysStatus _startIO(__XHANDLE xhandle) {
	passertMsg(0, "This shouldn't be called\n");
	return _SERROR(2854, 0, 0);
    }

};

#endif /* #ifndef __SERVER_FILE_BLOCK_H_ */
