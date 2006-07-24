#ifndef __MPMSG_MGR_H_
#define __MPMSG_MGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPMsgMgr.H,v 1.36 2004/09/29 13:23:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Handles messages sent between virtual
 * processors of the application it is in.
 * **************************************************************************/

#include <alloc/MemoryMgrPrimitive.H>
#include <sys/SoftIntr.H>

class MPMsgMgr {
public:
    enum {
	MSG_CHUNK_SIZE = 128,	// should be a multiple of the cache line size
	MSG_HOLDER_SIZE = (2*MSG_CHUNK_SIZE)
    };

    /*
     * Synchronous messages are allocated on the caller's stack.  We want each
     * message to be cache-line-aligned, however, so we don't declare a Msg
     * object directly.  Instead we declare a MsgSpace object, which is at
     * least one cache line larger than the maximum messsage size, and then
     * allocate an aligned message from the space.
     */
    struct MsgSpace {
	char holderSpace[MSG_HOLDER_SIZE + MSG_CHUNK_SIZE];
    };

    struct Msg {
	virtual void handle() = 0;
	virtual SysStatus send(DispatcherID dspid) = 0;
        DEFINE_NOOP_NEW(Msg);
        virtual ~Msg() { }
    };

    struct MsgAsync : Msg {
	virtual void handle() = 0;
	void *operator new(size_t size, MPMsgMgr *mgr) {
	    return mgr->alloc(size);
	}
	void operator delete(void *p, uval size) {
	    // free will take care of it
	}
	void free();

	virtual SysStatus send(DispatcherID dspid);
    };

    struct MsgSync : Msg {
	virtual void handle() = 0;
	void * operator new(size_t size, MPMsgMgr *mgr, MsgSpace &space) {
	    return mgr->alloc(size, space);
	}
	virtual SysStatus send(DispatcherID dspid);
	void reply();

    };

    static SysStatus SendAsyncUval(MPMsgMgr *mgr, DispatcherID target,
				   SysStatusFunctionUval func, uval val);

    static SysStatus SendSyncUval(MPMsgMgr *mgr, DispatcherID target,
				  SysStatusFunctionUval func, uval val,
				  SysStatus &funcRC);

protected:
    friend struct MsgAsync;
    friend struct MsgSync;

    struct MsgHeader {
	MsgHeader *next;		// next on pending list
	MPMsgMgr *manager;		// MPMsgMgr where msg originated
	union {
	    uval busy;			// for async messages
	    ThreadID sender;		// for sync messages
	};
    };

public:
    enum {
	MAX_MSG_SIZE = (MSG_HOLDER_SIZE - sizeof(MsgHeader))
    };

protected:
    struct MsgHolder : MsgHeader {
	char msgSpace[MAX_MSG_SIZE];
    };

    static Msg *GetMessage(MsgHeader *hdr) {
	return (Msg *) (hdr + 1);
    }

    static MsgHeader *GetHeader(Msg *msg) {
	return ((MsgHeader *) msg) - 1;
    }

    struct MsgQueue {
	MsgHeader *head;
	SoftIntr::IntrType interruptBit;
	void init();
	void setInterruptFunction(SoftIntr::IntrType bit,
				  SoftIntr::IntrFunc func);
    };

    // allocate a message buffer
    void * alloc(uval size);			// for async messages
    void * alloc(uval size, MsgSpace &space);	// for sync messages

    void addPendingMsg(MsgHeader *hdr, MsgQueue &q);

    class MPMsgMgrRegistry;
    typedef MPMsgMgrRegistry **MPMsgMgrRegistryRef;

    MPMsgMgrRegistryRef getRegistry() { return registryRef; }
    void addPendingSend(MsgHeader *hdr)  { addPendingMsg(hdr, sendQueue);  }
    void addPendingReply(MsgHeader *hdr) { addPendingMsg(hdr, replyQueue); }

    SysStatus processReplyQueue();

    void init(DispatcherID dspid, MemoryMgrPrimitive *pa,
	      MPMsgMgrRegistryRef &registry);
    void postFork();
    void setInterruptFunctions(
		    SoftIntr::IntrType sendBit,  SoftIntr::IntrFunc sendFunc,
		    SoftIntr::IntrType replyBit, SoftIntr::IntrFunc replyFunc);
    void addToRegistry(DispatcherID dspid);

    void * operator new(size_t size, MemoryMgrPrimitive *pa);

    class MsgAsyncUval;
    class MsgSyncUval;

    DispatcherID thisDspID;

    MsgQueue sendQueue;
    MsgQueue replyQueue;

    enum { NUM_MSGS = 32 };

    MsgHolder *msgHolder;	// fixed pool (array) of message buffers
    uval nextMsgIdx;		// index of next message to be allocated (hint)

    // lock for allocating message
    typedef BLock AllocMsgLockType;
    AllocMsgLockType allocMsgLock;

    MPMsgMgrRegistryRef registryRef;
};

#endif /* #ifndef __MPMSG_MGR_H_ */
