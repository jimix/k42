/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: diskTest.C,v 1.9 2005/08/29 20:23:38 dilma Exp $
 *****************************************************************************/
 /*****************************************************************************
  * Module Description:
  * **************************************************************************/

#include <sys/sysIncs.H>
#include <io/DiskClientAsync.H>
#include <stdlib.h>
#include <sys/systemAccess.H>
#include <scheduler/Scheduler.H>
#include <stdio.h>

#define MAX_BLOCK 100000
static uval contAcks = 0;
static BLock lock;
static uval nb = 0;
static ThreadID tid = Scheduler::NullThreadID;

void CallBack(uval rc, uval arg)
{
    lock.acquire();
    contAcks++;
#if 0 // if need to print progress
    if (contAcks  % 1000 == 0) {
	printf(".");
    }
#endif
    if (contAcks == nb) {
	passertMsg(tid != Scheduler::NullThreadID, "ops\n");
	Scheduler::Unblock(tid);
    }
    lock.release();
}

static void
usage(char *prog)
{
    printf("This program tests DiskClientAsync.\n"
	       "Usage: %s device numBlocksToWrite\n"
	       "e.g. %s /dev/scsi/host0/bus0/target0/lun0/part3 40000\n",
	       prog, prog);
}

int
main(int argc, char **argv)
{
    NativeProcess();

    SysStatus rc;
    char *dev;
    DiskClientAsyncRef dref;
    uval blkSize, origBlkSize;
    double secs;

    if (argc != 3) {
	usage(argv[0]);
	return 1;
    }

    DiskClientAsync::ClassInit(0);

    // open device, print information about block size
    dev = argv[1];
    rc = DiskClientAsync::Create(dref, dev, &CallBack, 0 /*needPhysAddr*/);
    if (_FAILURE(rc)) {
	printf("Can't open device %s (rc 0x%lx)\n", dev, rc);
	return 1;
    }
    
    rc = DREF(dref)->getBlkSize();
    if (_FAILURE(rc)) {
	printf("getBlkSize for device failed with rc 0x%lx\n", rc);
	return 1;
    }
    origBlkSize = _SGETUVAL(rc);
    blkSize = PAGE_SIZE;
    if (origBlkSize != PAGE_SIZE) {
	printf("Block size value adjusted from %ld to %ld\n",
		origBlkSize, blkSize);
    } else {
	printf("Block size is %ld\n", blkSize);
    }

    lock.init();

    char *endptr;
    nb = strtol(argv[2], &endptr, 0);
    if (*endptr!= '\0') {
	printf("%s: invalid argument for number of blocks to write"
	       ": %s\n", argv[0], argv[2]);
	usage(argv[0]);
	return 1;
    }

    if (nb <= 0) {
	printf("Argument numBlocksToWrite (%ld) should be > 0\n", nb);
	return 1;
    }

    printf("numBlocksToWrite is %ld\n", nb);

    uval amountDataMB = nb*blkSize >> 20;

    /* I wanted to have  all operations using the same buffer, but
     * then the disk layer tries to pin this pages a big number of
     * times, and pg pinCount is uval8.
     * So I'm allocating so that no buffer has more than 100 requests
     * on it */
    uval numBuffers = nb/100 + 1;
    char **buffer = (char**) allocGlobal(numBuffers * sizeof(char*));
    if (buffer == NULL) {
	printf("%s: Allocation of memory for buffers failed\n", argv[0]);
	return 1;
    }

    for (uval i = 0; i < numBuffers; i ++) {
	buffer[i] = (char*) allocGlobal(blkSize);
	if (buffer[i] == NULL) {
	    printf("%s: Allocation of memory for buffers failed (i is %ld)\n",
		   argv[0], i);
	    return 1;
    }

	memset(buffer[i], 'a' + (i % sizeof(char)), blkSize);
    }

    SysTime start, end;
    start = Scheduler::SysTimeNow();
    uval failures = 0;
    uval delay = 10;
    for (uval i = 0; i < nb ; i++) {
	rc = DREF(dref)->tryAsyncWriteBlock(i % MAX_BLOCK,
					    (void*)buffer[i % numBuffers],
					    i/*continuation token*/);
	if (_FAILURE(rc)) {
	    passertWrn(_SGENCD(rc) == EBUSY, "tryAsyncWriteBlock returned "
		       "rc 0x%lx (not the expected success or EBUSY)", rc);
	    Scheduler::DelayMicrosecs(delay);
	    delay *= 10;
	    failures++;
	    if (failures > 100) {
		printf("The disk is failing too much, let's give up\n");
		goto out;
	    }
	    i--;
	}
	failures = 0;
	delay = 10;
    }

    tid = Scheduler::GetCurThread();
    Scheduler::Block();
    end = Scheduler::SysTimeNow();

    secs = (double) ((double)(end-start)/Scheduler::TicksPerSecond());
    printf("Time for operations %lf secs (%lf MB/sec)\n",
	   secs, amountDataMB/secs);

  out:    
    for (uval  i = 0; i < numBuffers; i++) {
	freeGlobal(buffer[i], blkSize);
    }
    freeGlobal(buffer, numBuffers*sizeof(char*));

    return 0;
}
    
