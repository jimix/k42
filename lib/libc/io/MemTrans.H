#ifndef __MEM_TRANS_H_
#define __MEM_TRANS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemTrans.H,v 1.22 2004/10/08 21:40:06 jk Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines shared memory transport interface
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#include <meta/MetaObj.H>
#include <cobj/XHandleTrans.H>
#include <cobj/TypeMgr.H>
#include <sync/BLock.H>
#include <alloc/ObjCache.H>
#ifndef EXPORT_MEMTRANS
#define EXPORT_MEMTRANS
#include <cobj/ObjectHandle.H>
#include <misc/HashSimple.H>
#endif /* #ifndef EXPORT_MEMTRANS */

#ifndef _IN_STUBGEN
#include <stub/StubFR.H>
#include <stub/StubMemTrans.H>
#include <meta/MetaMemTrans.H>
#include <misc/AutoList.H>
#include <scheduler/Scheduler.H>
#else /* #ifndef _IN_STUBGEN */

// FIXME: fix stub-compiler grammar so don't need to do this
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifndef _IN_STUBGEN */

class MemTrans;
typedef MemTrans** MemTransRef;

class MemTrans : public Obj __xbase(Obj) {
#ifndef _IN_STUBGEN
    MemTrans(uval myKey=0):localFR(StubObj::UNINITIALIZED),key(myKey) {
	/* empty body */
    }
 public:
    //This is an error class code to identify errors that can be
    //recovered from if the caller performs the appropriate actions
    //(freeing memory or consuming ring buffers)
    enum {MemTransErr = 0xaa};

    struct ControlRing;

    // Describes memory area of another process which is
    // associated with this object
    struct ClientData {
	DEFINE_GLOBALPADDED_NEW(ClientData);
	ClientData(ProcessID id):remoteFR(StubObj::UNINITIALIZED),
				 remoteBase(0),
				 pid(id),
				 remoteMT(StubObj::UNINITIALIZED) {
	    /* empty body */
	}

	StubFR remoteFR;
	uval remoteBase;	//Addr where remoteFR mapping exists
	uval remoteSize;
	ControlRing *controlRing;
	ProcessID pid;
	ObjectHandle givenOH;	//The OH used by the client to get to this obj.
	StubMemTrans remoteMT;
	void init(ObjectHandle FR, ObjectHandle MT, uval base, uval size);
	virtual ~ClientData() { /* empty body */ };
    };
    struct ControlRing : public AutoList{
	DEFINE_NOOP_NEW(ControlRing);
	FairBLock readLock;
	FairBLock writeLock;
	ClientData *cd;
	uval remote;
	volatile uval32 localHead;
	volatile uval32 remoteTail;
	uval32 size;
	uval32 lowMark;
	uval32 highMark;
	volatile uval data[0];
    };

    // Object for receiving notifications of ring events
    struct MTEvents {
	DEFINE_GLOBAL_NEW(MTEvents);
	MTEvents() : ring(NULL) { /* empty body */ }
	ThreadID owner;
	XHandle other;
	ControlRing* ring;

	virtual void recvConnection(MemTransRef mtr, XHandle otherMT);
	virtual SysStatus allocRing(MemTransRef mtr, XHandle otherMT);
	virtual uval pokeRing(MemTransRef mtr, XHandle otherMT);

	//Called when MemTrans is being destroyed
	virtual void nowDead(MemTransRef mtr) { /* empty body */ };

	virtual ~MTEvents() { /* empty body */ };
    };
#else /* #ifndef _IN_STUBGEN */
    struct MTEvents;
#endif /* #ifndef _IN_STUBGEN */

    // This must always be called in case of a fork, even if the object
    // is to be destroyed anyway.
    virtual SysStatus postFork();

    virtual SysStatus destroy();
    DEFINE_GLOBAL_NEW(MemTrans);
    DEFINE_ACCESS_SERVER(MemTrans, MetaObj::none, MetaObj::none);
    DEFINE_REFS(MemTrans);
 protected:
    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type);

    virtual SysStatus init(ProcessID pid, XHandle &remote,
			   uval size,  MTEvents *handler);

#ifndef _IN_STUBGEN

    static ClientData* clnt(XHandle xhandle);


#else /* #ifndef _IN_STUBGEN */
    struct ControlRing;
#endif /* #ifndef _IN_STUBGEN */

    // Information about our local memory area
    uval localBase;
    uval localSize;
#ifndef _IN_STUBGEN
    StubFR localFR;

    struct PageUse:public AutoListNode{
	DEFINE_GLOBALPADDED_NEW(PageUse);
	PageUse(sval c, uval val):AutoListNode(),validator(val),pageCount(c) {
	    /* empty body */
	}
	uval validator;
	sval pageCount;
	~PageUse() { detach();};
    };

    //List of free/used pages in local memory area
    AutoListHead usedPages;


    // List of people waiting for pages to be freed
    struct WaitElement: public AutoListNode {
	DEFINE_NOOP_NEW(WaitElement);
	ThreadID id;
	volatile uval stopBlock;
	uval numPages;
	WaitElement(ThreadID thr, uval pages)
	    :AutoListNode(),id(thr), stopBlock(0), numPages(pages) {
	    /* empty body */
	}
	~WaitElement() { detach();};
    };
    AutoListHead waitList;
    AutoListHead readyList;


    typedef FairBLock LockType;
    LockType allocLock;

    struct PageSource : public ObjCache::PageSource {
	MemTransRef mtr;
	DEFINE_GLOBALPADDED_NEW(PageSource);
	PageSource(MemTransRef ref): mtr(ref) { /* empty body */ }
	virtual SysStatus getPage(uval &page) {
	    uval size = 1;
	    return DREF(mtr)->allocPagesLocal(page, size);
	}
    };

#endif /* #ifndef _IN_STUBGEN */


    static const ControlRing* remoteAddr(uval base, uval offset) {
	return (ControlRing*)(base + offset);
    }

    virtual ControlRing* __allocRing(uval &size);
    virtual SysStatus handleXObjFree(XHandle xhandle);
    static void FreeCDData(XHandle xhandle);

    virtual SysStatus locked_allocPagesLocal(uval &addr, uval &size,
					     uval validator);

    //Debugging tool
    virtual void dumpAllocList();


    MTEvents *cbs;     // Default callbacks for ring events

#ifndef _IN_STUBGEN
    static LockType createLock;
    // This is meant to be a hash-table which we can use to re-use
    // MemTrans objects when a service wants to use MemTrans to communicate
    // with a process to which a MemTrans connection is already established.
    typedef HashSimple<ProcessID, MemTransRef, AllocGlobal,0> SmtHash;
    typedef HashSimpleLocked<ProcessID, ClientData*, AllocGlobal,0> CDHash;
    static SmtHash *smtTbl;
    CDHash cdHash;

#endif /* #ifndef _IN_STUBGEN */
    uval refCount;
    uval noShare;
 public:
    const uval key;
    virtual MTEvents* getMTEvents() { return cbs; };
    virtual SysStatus incRefCount();
    virtual SysStatus detach();


    // Return XHandle used by pid (handle to ClientData).
    virtual uval getOH(ProcessID pid, ObjectHandle &oh);
    static SysStatus getMemTrans(ProcessID pid, MemTransRef &ref);

    uval debug;
    virtual uval localOffset(uval ptr) { return ptr - localBase;};
    virtual void* localPtr(uval offset) {
	if (offset >= localSize) {
	    return NULL;
	}
	return (void*)(offset + localBase);
    }

    // Get a pointer to data in a remote area
    virtual void* remotePtr(uval offset, XHandle xh);

    // Allocate a split ring in local area, and in remote area
    // size == number of ring entries desired
    virtual SysStatusUval allocRing(uval &size,	    //Number of entries
				    XHandle remote) {
	return allocRing(size, 0, size, remote);
    }

    virtual SysStatusUval allocRing(uval &size,	    //Number of entries
				    uval32 low, uval32 high, //When to notify
				    XHandle remote);

    virtual SysStatus setThreshold(XHandle xh, uval32 low, uval32 high);

    // Insert data into a ring buffer
    virtual uval insertToRing(XHandle xh, uval *value, uval numVals);

    // Read data from a ring buffer
    virtual SysStatus consumeFromRing(XHandle xh, uval *value, uval numVals);

    virtual uval insertToRing(XHandle xh, uval value) {
	return insertToRing(xh,&value,1);
    }

    virtual SysStatus consumeFromRing(XHandle xh, uval &value) {
	return consumeFromRing(xh, &value, 1);
    }

    virtual SysStatus dumpRingStatus(XHandle xh);

    static SysStatus Create(MemTransRef &mtr,   // ObjRef that is produced
			    ProcessID partner,	// Process to talk to
			    uval size,		// Size of local buffer
			    XHandle &remote,	// XHandle the other end uses
			    MTEvents *handler,  // Callbacks
			    uval key=0);

    static SysStatus GetMemTrans(MemTransRef &mtr,
				 XHandle &xh,
				 ProcessID pid,
				 uval key = 0);



    virtual SysStatus CreateObjCache(ObjCache* &oc, uval objSize);

    // Allocate from local memory pool
    // Validator is a key which must be matched in order to free
    // pages.
    virtual SysStatus allocPagesLocal(uval &addr, uval &size,
				      uval validator=0);

    virtual SysStatus allocPagesLocalBlocking(uval &addr, uval &size,
					      uval validator=0);
    // Release pages for future use
    // "validator" must match the value used in allocPagesLocal
    virtual void freePage(uval offset, uval validator=0);
    virtual void freePagePtr(void *addr);

    static void ClassInit(VPNum vp);
 __xpublic:

    virtual SysStatusUval _allocRing(__in uval localLoc,
				     __out uval &remoteLoc,
				     __XHANDLE xhandle);

    // Used to tell us that a ring needs attention
    virtual __async SysStatusUval _pokeRing(__XHANDLE xh);

 __xprivate:

    // These are used internally in negotiating the configuration
    // of the MemTrans objects.
    virtual SysStatus _swapHandle(ObjectHandle callerMT, //Caller's MT
				  ObjectHandle callerFR, //Caller's FR
				  uval callerSize,	 //Size of region
				  __out uval &remoteSize,
				  __out ObjectHandle &remoteFR,
				  __XHANDLE xhandle,
				  __CALLER_PID pid);
    virtual __async SysStatus _completeInit(__XHANDLE xhandle);

};

#endif /* #ifndef __MEM_TRANS_H_ */
