 /******************************************************************************
  * K42: (C) Copyright IBM Corp. 2004.
  * All Rights Reserved
  *
  * This file is distributed under the GNU LGPL. You should have
  * received a copy of the license along with K42; see the file LICENSE.html
  * in the top-level directory for more details.
  *
  * $Id: DiskTrace.C,v 1.13 2005/01/24 16:11:27 dilma Exp $
  *****************************************************************************/
 /*****************************************************************************
  * Module Description:
  * **************************************************************************/

#include <sys/sysIncs.H>
#include "DiskTrace.H"
#include <stub/StubKBootParms.H>
#include <misc/DiskMountInfoList.H>
#include <scheduler/Scheduler.H>

#include <io/IO.H>

/* static */ SysStatus
DiskTrace::GetDiskDevice(char *buffer, uval bufferLen)
{
    SysStatus rc;
    DiskMountInfoList diskList;
    rc = diskList.init();
    if (_FAILURE(rc)) {
	err_printf("spec in K42_FS_DISK incorrect\n");
	return rc;
    }
    
    void *curr = NULL;
    DiskMountInfo *disk;
    uval found = 0;
    while ((curr = diskList.next(curr, disk))) {
	if (strncmp(disk->getFSType(), "raw", 3) == 0) {
	    /* FIXME: we have to find an available one. For now, we're
	     * using the first "raw" spec we find, without any check */
	    found = 1;
	    break;
	}
    }
    
    if (!found) {
	err_printf("In order to dump to the raw disk you have to specify"
		   " a \"raw\" disk at environment variable K42_FS_DISK\n");
	return _SERROR(2865, 0, 0);
     }
    
    if (strlen(disk->getDev()) < bufferLen) {
	strcpy(buffer, disk->getDev());
    } else {
	return _SERROR(2867, 0, ENOSPC);
    }
    
    return 0;
}

 /**
  * set up disk for trace
  */
 SysStatus
 DiskTrace::init(uval n)
 {
     if (n != 1) {
	 // not supported yet
	 return _SERROR(2853, 0, 0);
     }

     SysStatus rc;
     ncpus = n;

     // blocks 0 to RESERVED-1 are reserved for meta-data
     nextFreeBlock = RESERVED;

     char dev[256];
     rc = GetDiskDevice(dev, sizeof(dev));
     if (_FAILURE(rc)) {
	 err_printf("DiskTrace::GetDiskDevice failed with rc 0x%lx\n", rc);
	 return rc;
     }

     DiskClient::ClassInit(Scheduler::GetVP());

     rc = DiskClientAsync::Create(diskRef, dev,
				  DiskTrace::DiskCallBack,
				  0 /* needPhysAddr */);
     tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

     SysStatusUval rcuval;
     uval diskSize;
     rcuval = DREF(diskRef)->getSize();
     tassertMsg(_SUCCESS(rcuval), "rc 0x%lx\n", rc);
     diskSize = _SGETUVAL(rcuval);
     rcuval = DREF(diskRef)->getBlkSize();
     tassertMsg(_SUCCESS(rcuval), "rc 0x%lx\n", rc);
     blkSize = _SGETUVAL(rcuval);
     tassertMsg(diskSize % blkSize == 0, "diskSize 0x%lx, blkSize 0x%lx\n",
		diskSize, blkSize);
     AdjustBlockSize(blkSize);
     nBlks = diskSize / blkSize;

#ifdef DEBUGGING
     err_printf("blkSize %ld, nBlks %ld\n", blkSize, nBlks);
#endif // #ifdef DEBUGGING

     metaData.init();

     return 0;
}

SysStatusUval
DiskTrace::write(VPNum physProc, uval bufferAddr, uval size)
{
    passertMsg(physProc == 0, "Not dealing with multiproc yet\n");

    SysStatus rc;

    uval blkNumber;
    uval numBlocks = size / blkSize;
    uval addr = bufferAddr;

    for (uval i = 0; i < numBlocks; i++) {
	blkNumber = getFreeBlock(blkSize);
	rc = sendRequest(addr, blkNumber, blkSize);
	tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
	addr += blkSize;
    }

    // last part of the request
    if (numBlocks * blkSize < size) {
	uval sizeLeft = size - numBlocks*blkSize;
	tassertMsg(sizeLeft < blkSize, "????");
	blkNumber = getFreeBlock(sizeLeft);
	rc = sendRequest(addr, blkNumber, sizeLeft);
	tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    }

    //err_printf(">");
    return _SRETUVAL(size);
}    

/* static */ void
DiskTrace::DiskCallBack(uval retrc, uval arg)
{
    tassertMsg(_SUCCESS(retrc), "retrc 0x%lx\n", retrc);
    WriteRequest *wreq = (WriteRequest*) arg;
    (void)wreq->diskObj->requestCompleted(wreq);
}

SysStatus
DiskTrace::CopyToFile(char *fname)
{
    SysStatus rc;
    FileLinuxRef fref;
    rc = FileLinux::Create(fref, fname, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (_FAILURE(rc)) {
	err_printf("open of file %s failed rc 0x%lx\n", fname, rc);
	return rc;
    }

#ifdef DEBUGGING
    FileLinuxRef ackFile;
    rc = FileLinux::Create(ackFile, "/knfs/ackFile.txt", O_RDONLY, 0);
    passertMsg(_SUCCESS(rc), "?");

    GenState moreAvail;
    uval idx;
    uval ackArray[ACK_ARRAY_SIZE];
    do {
	rc = DREF(ackFile)->read((char*)&idx, sizeof(uval), NULL, moreAvail);
	passertMsg(_SUCCESS(rc), "?");
	if (moreAvail & FileLinux::ENDOFFILE) {
	    break;
	}
	passertMsg(_SGETUVAL(rc) == sizeof(uval), "?");
	passertMsg(idx < ACK_ARRAY_SIZE, "large idx\n");
	
	rc = DREF(ackFile)->read((char*)&ackArray[idx],
				 sizeof(sval), NULL, moreAvail);
	passertMsg(_SUCCESS(rc) && _SGETUVAL(rc) == sizeof(sval), "?");

	err_printf("Got from file blk %ld val %ld\n", idx, ackArray[idx]);
    } while (1);
#endif //#ifdef DEBUGGING

    char dev[256];
    rc = GetDiskDevice(dev, sizeof(dev));
    if (_FAILURE(rc)) {
	err_printf("DiskTrace::GetDiskDevice failed with rc 0x%lx\n", rc);
	return rc;
    }

    uval blkNumber, toBeRead, incompIdx = 0;

    DiskClient::ClassInit(Scheduler::GetVP());
    
    DiskClientRef dref;
    rc = DiskClient::Create(dref, dev, 0 /*needPhysAddr*/);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    uval blkSize = DREF(dref)->getBlkSize();
    AdjustBlockSize(blkSize);

    void *block = (void*) AllocGlobal::alloc(blkSize);
    // assuming RESERVED block for meta-data is block 0
    rc = DREF(dref)->readBlock(0, block);
    if (_FAILURE(rc)) {
	err_printf("Can't read block 0 of disk %s (rc 0x%lx)\n", dev, rc);
	goto error;
    }

    struct MetaData::DiskData data;
    memcpy(&data, block, sizeof(MetaData::DiskData));

#ifdef DEBUGGING
    err_printf("DEBUG INFO: reserved %ld size %ld\n", data.reserved,
	       data.size);
    for (uval i = 0; i < MetaData::MAX_INCOMPLETE_ENTRIES; i++) {
	if (data.incompleteBlocks[i].blkNumber == 0) {
	    break;
	}
	err_printf("Incomplete block %ld, size %ld\n",
		   data.incompleteBlocks[i].blkNumber,
		   data.incompleteBlocks[i].size);
    }
#else
    err_printf("Copying %ld bytes from raw disk to file\n", data.size);
#endif

    toBeRead = data.size;
    blkNumber = data.reserved;
    incompIdx = 0;
    while (toBeRead > 0) {
	//err_printf ("+%ld", blkNumber);
	rc = DREF(dref)->readBlock(blkNumber, block);
	if (_FAILURE(rc)) {
	    err_printf("Can't read block %ld of disk %s (rc 0x%lx)\n",
		       blkNumber, dev, rc);
	    goto error;
	}

	uval size;
	/* we know array incompleteBlocks is sorted on blkNumber, and
	 * last element has blkNumber zero */
	if (data.incompleteBlocks[incompIdx].blkNumber == blkNumber) {
	    size = data.incompleteBlocks[incompIdx++].size;
	} else {
	    size = (toBeRead > blkSize ? blkSize : toBeRead);
	}

#ifdef DEBUGGING
	uval value = ComputeValue((char*)block, size);
	passertMsg(value == ackArray[blkNumber], "value %ld ack %ld\n",
		   value, ackArray[blkNumber]);
#endif //#ifdef DEBUGGING

	blkNumber++;

	// write data to file
	GenState moreAvail;
	ThreadWait *tw = NULL;
	rc = DREF(fref)->write((const char *)block, size,
			       &tw, moreAvail);
	passertMsg(_SGETUVAL(rc) == size, "???");
	if (_FAILURE(rc)) {
	    err_printf("write operation to file %s failed\n", fname);
	    goto error;
	}

	/* We have this flush here so we can poke the FCM, making it send
	 * dirty pages to the disk. The way the memory management works
	 * currently, if we only flush at the end all the i/o will happen
	 * in the end ... Not a big difference here, but anyway ...*/
	
	rc = DREF(fref)->flush();
	tassertWrn(_SUCCESS(rc), "traced flush failed\n");

	toBeRead -= size;
    }

    rc = DREF(fref)->flush();
    tassertWrn(_SUCCESS(rc), "traced flush failed\n");
    
    err_printf("Data from trace disk has been written to file %s; "
	       "num of blocks %ld\n", fname, blkNumber-RESERVED);

  error:
    AllocGlobal::free(block, blkSize);
    return rc;
}

SysStatus
DiskTrace::sendRequest(uval addrarg, uval blkNumber, uval size)
{
    while (1) {
	lock.acquire();
	if (outstanding < MAX_OUTSTANDING) {
	    outstanding++;
	    tassertMsg(nextFreeRequest != NULL, "?");
	    WriteRequest *wreq = nextFreeRequest;
	    nextFreeRequest = nextFreeRequest->nextFree;

#ifdef DEBUGGING
	    sanityCheck();
#endif
	    // not necessary, but it makes debugging easier:
	    wreq->nextFree = NULL;

	    wreq->init(this, blkNumber, size);
	    /* always copying, because area can be overwritten by trace
	     * daemon before we get a chance to do I/O */
	    memcpy((void*) wreq->alignedArea, (void*) addrarg, size);

#ifdef DEBUGGING
	    passertMsg(blkNumber < ACK_ARRAY_SIZE,
		       "ackArray not large enough\n");
	    uval value = ComputeValue((char*)wreq->alignedArea, size);
	    ackArray[blkNumber] = -value;
	    // write this debugging data to file
	    SysStatus rc;
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    rc = DREF(ackFile)->write((const char *)&blkNumber,
				      sizeof(blkNumber),
				      &tw, moreAvail);
	    passertMsg(_SGETUVAL(rc) == sizeof(blkNumber), "???");
	    rc = DREF(ackFile)->write((const char *)&value, sizeof(value),
				      &tw, moreAvail);
	    passertMsg(_SGETUVAL(rc) == sizeof(value), "???");
	    rc = DREF(ackFile)->flush();
	    passertMsg(_SUCCESS(rc), "???");

#ifdef DEBUGGING_VERBOSE
#if 0
	    err_printf("ackArray val %ld for blk %ld wreq %p\n",
		       ackArray[blkNumber], blkNumber, wreq);
#endif
#endif //#ifdef DEBUGGING_VERBOSE
#endif // #ifdef DEBUGGING

	    if (outstandingDisk < MAX_OUTSTANDING_DISK) {
		outstandingDisk++;
		lock.release();
		return DREF(diskRef)->aWriteBlock(wreq->blkNumber,
						  (void *) wreq->alignedArea,
						  (uval)wreq);
	    } else {
		wreq->nextBlocked = blockedRequests;
		blockedRequests = wreq;
		lock.release();
		// fake that this has been already sent
		return size;
	    }
	} else {
	    lock.release();
	    // for now simply try again soon
#if 0
	    err_printf("Doing Scheduler::DelayMicrosecs() for 0.1 sec\n");
#endif
	    Scheduler::DelayMicrosecs(100000); // 0.1 second
	}
    }
}

SysStatus
DiskTrace::requestCompleted(WriteRequest *wreq)
{
    lock.acquire();
//    err_printf("Completed addr 0x%lx\n", wreq->alignedArea);

    outstanding--;
    outstandingDisk--;

    SysStatus rc;
    while (blockedRequests && outstandingDisk < MAX_OUTSTANDING_DISK) {
	outstandingDisk++;
	WriteRequest *wr = blockedRequests;
	rc = DREF(diskRef)->aWriteBlock(wr->blkNumber,
					(void *) wr->alignedArea,
					(uval)wr);
	tassertWrn(_SUCCESS(rc), "aWriteBlock failed with 0x%lx\n", rc);
	blockedRequests = blockedRequests->nextBlocked;
	wr->nextBlocked = NULL;
    }

    if (wreq->blkNumber != 0) { // not counting meta-data
#ifdef DEBUGGING
	sval value = (sval) ComputeValue((char*)wreq->alignedArea,
					 wreq->size);
	if (value != (-ackArray[wreq->blkNumber])) {
	    passertMsg(0, "ERROR: block %ld, value %ld, ackArray for it %ld "
		       "wreq %p, align 0x%lx\n", wreq->blkNumber, value,
		       ackArray[wreq->blkNumber], wreq, wreq->alignedArea);
	}
	ackArray[wreq->blkNumber] = -ackArray[wreq->blkNumber];
#endif // #ifdef DEBUGGING
	nBlksCompleted++;
    }

    wreq->inUse = 0;
    wreq->nextFree = nextFreeRequest;
    nextFreeRequest = wreq;

#ifdef DEBUGGING
    sanityCheck();
#endif

    metaData.diskData->size += wreq->size;
#ifdef DEBUGGING
    uval isOld = metaData.isOld();
#endif // #ifdef DEBUGGING

    lock.release();

#ifdef DEBUGGING
    if (isOld) {
	writeMetaData();
	// race on the following is ok
	metaData.lastWritten = Scheduler::SysTimeNow();
    }
#endif // #ifdef DEBUGGING

    return 0;
}

void
DiskTrace::writeMetaData(uval sync /* = 0 */)
{
    SysStatus rc;

    lock.acquire();

    if (sync) {
	// wait until we know we can write
	while (outstandingDisk >= MAX_OUTSTANDING_DISK) {
	    lock.release();
	    Scheduler::DelayMicrosecs(500000); // 0.5 second
	    lock.acquire();
	}
    } else {
	if (outstandingDisk >= MAX_OUTSTANDING_DISK) {
	    // disk is busy, give up on writing meta-data
	    lock.release();
	    return;
	}
    }

    tassertMsg(outstandingDisk < MAX_OUTSTANDING_DISK, "?");

    outstanding++;
    outstandingDisk++;
    tassertMsg(nextFreeRequest != NULL, "?");
    WriteRequest *wreq = nextFreeRequest;
    nextFreeRequest = nextFreeRequest->nextFree;

#ifdef DEBUGGING
    wreq->nextFree = NULL;
    sanityCheck();
#endif

    wreq->init(this, 0, blkSize);
    memcpy((void*)wreq->alignedArea, (void*)metaData.diskData, blkSize);

    rc = DREF(diskRef)->aWriteBlock(wreq->blkNumber,
				    (void *) wreq->alignedArea, (uval)wreq);
    tassertWrn(_SUCCESS(rc), "DiskTrace: writing of meta-data failed rc "
	       "0x%lx\n", rc);

    lock.release();
}

uval
DiskTrace::getFreeBlock(uval size)
{
    AutoLock<BLock> al(&lock);
    passertMsg(nextFreeBlock < nBlks, "Not enough blocks in disk: "
	       "nextFreeBlock %ld, nBlks %ld\n", nextFreeBlock, nBlks);

    uval bl = nextFreeBlock++;
    if (size != PAGE_SIZE) {
	tassertMsg(size < PAGE_SIZE, "size is %lx\n", size);
	metaData.addIncompleteBlock(bl, size);
    }
    return  bl;
}


