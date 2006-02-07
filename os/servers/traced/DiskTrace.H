#ifndef __DISK_TRACE_H_
#define __DISK_TRACE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskTrace.H,v 1.12 2005/01/10 15:42:11 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <io/DiskClientAsync.H>
#include <sync/BLock.H>
#include <sys/PageAllocator.H>

/* The following macros add code to check if we got ack from all
 * I/O requests.This will be removed soon. */
//#define DEBUGGING
//#define DEBUGGING_VERBOSE

#ifdef DEBUGGING
#include <io/IO.H>
#endif

/**
 * All of this is still a big, horrible hack.
 * The interface between the disk and the app (tracedServer) needs to
 * be fixed so that it doesn't use async msgs (instead going through
 * a shared memory transport) needs to be fixed before it makes sense
 * to properly support dumping trace to disk.
 *
 * Also, for now we're restricted to dumping output from 1-way executions
 * only.
 *
 * Stay tuned, these hacks/restrictions will be fixed very soon.
 *
 */

class DiskTrace {
private:
    static SysStatus GetDiskDevice (char *buffer, uval bufferLen);

protected:
    BLock lock;
    uval ncpus; // for now only one CPU, one trace file at the disk
    uval dump;  /* indicates if tracedServer is using this object for
		 * dumping data and then exiting */

    DiskClientAsyncRef diskRef;

    uval blkSize;
    uval nBlks;

    static const uval MAX_OUTSTANDING_DISK = 22; /* max number of outstanding
						  * requests to the disk */

    static const uval MAX_OUTSTANDING = 256; /* max number of outstanding
					      * requests for sending data
					      * that we will accept (and queue
					      * if necessary) */

    uval outstanding;       /* either ongoing operation on disk or queued
			     * for disk submission */
    uval outstandingDisk;   /* ongoing operation on disk */

    uval nBlksCompleted;    /* number of blocks that we got ack for the
			     * write completion */

    /* For now we're not putting anything on the first block, but soon
     * we'll have the meta-data for the tracing there. */
    static const uval RESERVED = 1;
    uval nextFreeBlock;

    struct MetaData {
	static const uval MAX_INCOMPLETE_ENTRIES 
	= (PAGE_SIZE-2*sizeof(uval))/(2*sizeof(uval)) - 1;
	struct DiskData {
	    uval reserved;
	    uval size;
	    struct {
		uval blkNumber;
		uval size;
	    } incompleteBlocks[MAX_INCOMPLETE_ENTRIES+1];
	} *diskData;  // allocated dynamically so it's page aligned for i/o
	SysTime lastWritten;
	static const uval UPDATE_INTERVAL = 60; // 60 secs
	uval blkSize;
	uval incompleteCounter;
	DEFINE_GLOBAL_NEW(MetaData);
	MetaData() : lastWritten(0), incompleteCounter(0) {}
	void init() {
	    blkSize = PAGE_SIZE;
	    passertMsg(sizeof(DiskData) <= blkSize, "bug bug\n");
	    diskData = (struct DiskData*) AllocGlobal::alloc(blkSize);
	    diskData->reserved = RESERVED;
	    diskData->size = 0;
	    memset(&diskData->incompleteBlocks[0], 0,
		   sizeof(diskData->incompleteBlocks));
	}
	~MetaData() {
	    AllocGlobal::free(diskData, blkSize);
	}
	uval isOld() {
	    if (Scheduler::SysTimeNow() - lastWritten 
		> UPDATE_INTERVAL*Scheduler::TicksPerSecond()) {
		return 1;
	    }
	    return 0;
	}
	void addIncompleteBlock(uval bl, uval size) {
	    if (incompleteCounter == MAX_INCOMPLETE_ENTRIES) {
		passertMsg(0, "The MetaData data structure is not large "
			   "enough for the number of incomplete blocks "
			   "needed\n");
	    }
	    diskData->incompleteBlocks[incompleteCounter].blkNumber = bl;
	    diskData->incompleteBlocks[incompleteCounter].size = size;
	    incompleteCounter++;
	}
    } metaData;

    uval getFreeBlock(uval size);

    class WriteRequest {
    public:
	DiskTrace *diskObj;
	WriteRequest *nextFree;
	WriteRequest *nextBlocked;
	uval notified;
	uval threadPtr;
	uval blkNumber;
	uval size;
	uval alignedArea;
	uval inUse;
    public:
	DEFINE_GLOBAL_NEW(WriteRequest);
	WriteRequest() : nextFree(NULL), nextBlocked(NULL), notified(0),
	    inUse(0){
	    SysStatus rc = DREFGOBJ(ThePageAllocatorRef)
		->allocPagesAligned(alignedArea, PAGE_SIZE, PAGE_SIZE);
	    // FIXME: deal with possible error here
	    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
	};
	void init(DiskTrace *obj, uval bn, uval s) {
	    passertMsg(inUse == 0, "?");
	    inUse = 1;
	    diskObj = obj;
	    blkNumber = bn;
	    size = s;
	    threadPtr = (uval)Scheduler::GetCurThread();
	}
	ThreadID getThread() { return (ThreadID) threadPtr; }
	~WriteRequest() {
	    SysStatus rc = DREFGOBJ(ThePageAllocatorRef)
		->deallocPages(alignedArea, PAGE_SIZE);
	    // FIXME: deal with possible error here
	    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
	}
    };

    WriteRequest wreq[MAX_OUTSTANDING];
    WriteRequest *nextFreeRequest;
    WriteRequest *blockedRequests;

#ifdef DEBUGGING
    static const uval ACK_ARRAY_SIZE=80000;
    sval ackArray[ACK_ARRAY_SIZE];
    FileLinuxRef ackFile;
#endif // #ifdef DEBUGGING

    SysStatus sendRequest(uval addr, uval blkNumber, uval size);
    SysStatus requestCompleted(WriteRequest *wreq);

    /* sync !=0  means that the meta data has to be written; if sync is
     * 0 the meta data will only be written if there is space for one
     * more outstanding disk request */
    
    void writeMetaData(uval sync = 0);

public:
    DEFINE_GLOBAL_NEW(DiskTrace);
    DiskTrace(uval d) : dump(d), diskRef(NULL), outstanding(0),
	outstandingDisk(0), nBlksCompleted(0) {
	lock.init();
	nextFreeRequest = &wreq[0];
	for (uval i = 0; i < MAX_OUTSTANDING - 1; i++) {
	    wreq[i].nextFree = &wreq[i+1];
	}
	wreq[MAX_OUTSTANDING-1].nextFree = NULL;
#ifdef DEBUGGING
	memset(&ackArray, 0, sizeof(ackArray));
	SysStatus rc = FileLinux::Create(ackFile, "/knfs/ackFile.txt",
					 O_RDWR|O_CREAT|O_TRUNC, 0644);
	passertMsg(_SUCCESS(rc), "?");
#endif // #ifdef DEBUGGING
    }

    ~DiskTrace() { tassertMsg(outstanding == 0, "?"); }

    SysStatus init(uval ncpus);

    static void DiskCallBack(uval rc, uval arg);

    SysStatusUval write(VPNum physProc, uval bufferAddr, uval size);

    uval getOutstanding() { AutoLock<BLock> al(&lock); return outstanding; }

    void sync() {
	writeMetaData(1);
	err_printf("DiskTrace metadata synced to disk ... size %ld\n",
		   metaData.diskData->size);
    }

    static SysStatus CopyToFile(char *fname);

    void finishOutstanding() {
	uval cont = 0;
	if (getOutstanding() != 0) {
	    err_printf("Waiting for outstanding disk requests ");
	    // make sure all outstanding disk requests have finished
	    while (getOutstanding() != 0) {
		Scheduler::DelayMicrosecs(1000000); // 1 second
		err_printf(".");
		cont++;
		if (cont > 20) {
		    passertMsg(0, "debug this\n");
		}
	    }
	    err_printf(" done\n");
	}
	err_printf("Number of block I/O requests: %ld, nb completed: %ld\n",
		   nextFreeBlock-RESERVED, nBlksCompleted);
#ifdef DEBUGGING
	passertMsg(nextFreeBlock-RESERVED == nBlksCompleted, "look\n");
	for (uval i = 1; i <= nBlksCompleted; i++) {
	    passertMsg(ackArray[i] > 0, "ackArray[%ld] %ld\n", i,
		       ackArray[i]);
	}
#endif // #ifdef DEBUGGING
    }
protected:
    static void AdjustBlockSize(uval &blsize) {
	if (blsize != PAGE_SIZE) {
	    tassertMsg(blsize < PAGE_SIZE, "blkSize %ld", blsize);
	    tassertMsg(PAGE_SIZE % blsize == 0, "blkSize %ld", blsize);
	    tassertWrn(0, "In DiskTrace: got blkSize %ld, using %ld\n",
		       blsize, PAGE_SIZE);
	    blsize = PAGE_SIZE;
	}
    }
    static uval ComputeValue(char* area, uval size) {
	uval value = 0;
	for (uval i = 0; i < size; i++) {
	    value += area[i];
	}
	return value;
    }

#ifdef DEBUGGING
    void sanityCheck() {
	uval shouldBeFree = MAX_OUTSTANDING-outstanding;
	uval cont = 0;
	WriteRequest *tmp = nextFreeRequest;
	while (tmp) {
	    cont++;
	    tmp = tmp->nextFree;
	}
	passertMsg(cont == shouldBeFree, "ops\n");
    }
#endif //#ifdef DEBUGGING
};

#endif /* #ifndef __DISK_TRACE_H_ */
