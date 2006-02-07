#ifndef __SHMEMBUF_H_
#define __SHMEMBUF_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ShMemBuf.H,v 1.4 2004/09/27 19:19:06 dilma Exp $
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
#ifndef EXPORT_SHMEMBUF
#define EXPORT_SHMEMBUF
#include <cobj/ObjectHandle.H>
#include <misc/HashSimple.H>
#endif /* #ifndef EXPORT_SHMEMBUF */


#ifndef _IN_STUBGEN
#include <stub/StubFR.H>
#include <misc/AutoList.H>
#include <scheduler/Scheduler.H>
#include <meta/MetaShMemBuf.H>
#else /* #ifndef _IN_STUBGEN */
// FIXME: fix stub-compiler grammar so don't need to do this
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifndef _IN_STUBGEN */

class ShMemBuf;
typedef ShMemBuf** ShMemBufRef;

class ShMemBuf: public Obj __xbase(Obj) {
protected:
    ObjectHandle frOH;
    uval addr;
    ProcessID key;
    DEFINE_GLOBAL_NEW(ShMemBuf);
    DEFINE_REFS(ShMemBuf);
    DEFINE_ACCESS_SERVER(ShMemBuf, MetaObj::none, MetaObj::none);

    typedef FairBLock LockType;
    LockType lock;

#ifndef _IN_STUBGEN
    const uval bufSize;
    const uval minSize;
    ShMemBuf(uval minObjSize):bufSize(SEGMENT_SIZE), minSize(minObjSize) {};

    struct AllocElement{
	DEFINE_GLOBAL_NEW(AllocElement);
	uval offset;
	uval32 blocks;	//How many "minSize" sized objects
	uval32 refCount;
	AllocElement* next;
	AllocElement(uval off, uval num):
	    offset(off), blocks(num), refCount(0), next(NULL) {};
    };

    typedef HashSimple<uval, AllocElement*, AllocGlobal, 0> ClientAllocs;
    typedef HashSimpleLocked<ProcessID, ShMemBufRef, AllocGlobal, 0> ExpHash;
    static ExpHash* exports;
    ClientAllocs allocs;
    AllocElement* free;

    struct ClientData{
	DEFINE_GLOBALPADDED_NEW(ClientData);
	ProcessID pid;
	ObjectHandle oh;
	ClientAllocs mine;
	virtual ~ClientData() {
	    // free the list of clients
	    uval restart = 0;
	    uval key;
	    AllocElement *datum;
	    while (mine.removeNext(key, datum, restart));
	    mine.destroy();
	}
    };

#else
    struct AllocElement;
    struct ClientData;
#endif /* #ifndef _IN_STUBGEN */

    virtual SysStatus locked_getProcInfo(ProcessID pid,
					 ShMemBuf::ClientData* &cd);

    virtual SysStatus init(ProcessID pid);
    static SysStatus Create(ShMemBufRef &ref, ProcessID pid, uval minObjSize);
    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type);
    virtual SysStatus handleXObjFree(XHandle xhandle);
    virtual SysStatus forkDestroy();
public:
    virtual SysStatus addClient(ProcessID pid, ObjectHandle &oh);
    virtual SysStatus getOH(ProcessID pid, ObjectHandle &oh);
    static void ClassInit(VPNum vp);
    static void PostFork();
    static SysStatus Fetch(ProcessID pid, ObjectHandle &oh,
			   ShMemBufRef &smbRef, uval minObjSize);

    //Returns number of bytes allocated
    //Sets offset to offset of memory in shared mem region
    virtual SysStatusUval shareAlloc(ProcessID pid, uval &offset,
				     uval &addr, uval size);

    // Without allocating memroy from region,
    // remembers that an existing portion is being shared to pid
    // If addr != 0, use addr, else use offset to identify memory
    virtual SysStatus shareExisting(ProcessID pid, uval addr, uval offset);
    virtual SysStatus destroy();

    static void BeingFreed(XHandle xhandle);
    virtual SysStatus exportedXObjectListEmpty();
    virtual SysStatus unShare(__in uval addr, ProcessID pid);

__xprotected:
    virtual SysStatus _registerClient(__out ObjectHandle &oh, __out uval &size,
				      __XHANDLE xh, __CALLER_PID pid);
__xpublic:
    virtual __async SysStatus _unShare(__in uval addr, __XHANDLE xh);
};

#endif /* __SHMEMBUF_H_ */
