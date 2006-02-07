/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockCacheK42.C,v 1.4 2004/03/01 17:51:25 dilma Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "BlockCacheK42.H"
#include "Disk.H"
#include "PSOBase.H"

#include <scheduler/Scheduler.H>

#include "KFSDebug.H"

#define DBC(meth, blkno)  KFS_DPRINTF(DebugMask::CACHE, \
                                      "BlockCacheK42::%s blkno %u\n", \
                                      meth, blkno)
#define DBCE(meth, blkno) KFS_DPRINTF(DebugMask::CACHE, \
                                      "BlockCacheEntryK42::%s blkno %u\n", \
                                      meth, blkno)


BlockCacheEntryK42::BlockCacheEntryK42(uval32 b, Disk *disk)
    : blkno(b), isEmpty(1), isDirty(0), flushTime(0), nextDirty(NULL)
{
    DBCE("constructor", b);

    data = (char *)AllocGlobalPadded::alloc(OS_BLOCK_SIZE);
    memset(data, 0, OS_BLOCK_SIZE);
    // Undetermined block number! The best we can do is return a
    // null-page
    if (!blkno) {
	KFS_DPRINTF(DebugMask::CACHE,
		    "In BlockCacheEntryK42 with blkno %u\n", blkno);
	// memset(data, 0, OS_BLOCK_SIZE);
    }

    users = 0;

    d = disk;
}

/* virtual */
BlockCacheEntryK42::~BlockCacheEntryK42()
{
    DBCE("destructor", blkno);
    if (isDirty) {
	d->writeBlock(blkno, data);
    }
    AllocGlobalPadded::free(data, OS_BLOCK_SIZE);
}

// should return a OS_BLOCK_SIZE page with the block's content
/* virtual */ char*
BlockCacheEntryK42::getData()
{
    DBCE("getData", blkno);
    return data;
}

// mark this block as dirty
/* virtual */ uval32
BlockCacheEntryK42::markDirty()
{
    DBCE("markDirty", blkno);
    tassertMsg(blkno != 0, "?");

    if (!isDirty) {
	isDirty = 1;
	flushTime = Scheduler::SysTimeNow() + 
	    Scheduler::TicksPerSecond()*FLUSH_TIME;
    }
    return 0;
}

// mark this block as clean
/* virtual */ void
BlockCacheEntryK42::markClean()
{
    DBCE("markClean", blkno);
    isDirty = 0;
    // we leave the element on the BlockCache dirtyQueue; flush() will get
    // it out of there if it's still clean
}

// read in (from disk) this block's data
/* virtual */ void
BlockCacheEntryK42::readData()
{
    DBCE("readData", blkno);
    if (blkno) {
	if (isDirty == 0  && isEmpty) {
	    d->readBlock(blkno, data);
	    isEmpty = 0;
	}
    }
}

uval
BlockCacheEntryK42::removeUser()
{
    DBCE("removeUser", blkno);
    return --users;
}

void
BlockCacheEntryK42::addUser()
{
    DBCE("addUser", blkno);
    users++;
}

void BlockCacheEntryK42::flush()
{
    tassertMsg(blkno, "?");
    SysStatus rc = d->writeBlock(blkno, data);
    if (_FAILURE(rc)) {
	tassertMsg(_SUCCESS(rc), "writeBlock failed (pid %ld), rc 0x%lx\n",
		   _SGETPID(DREFGOBJ(TheProcessRef)->getPID()), rc);
    } else {
	isDirty = 0;
	flushTime = 0;
	nextDirty = NULL;
	//err_printf("writeBlock on flush() succeeded (pid %ld)\n",
	//	   _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));
    }
}

BlockCacheK42::BlockCacheK42(Disk *disk) : d(disk), te(this),
					   doingFlush(0)
{
    TimerEventBC::ScheduleEvent(&te);
#ifndef NDEBUG // trying to make sure the timer events are being triggered
    lastTimerCreationTime = Scheduler::SysTimeNow();
#endif // #ifndef NDEBUG
}

/* virtual */ BlockCacheEntry *
BlockCacheK42::getBlock(uval32 b)
{
    DBC("getBlock", b);
    BlockCacheEntryK42 *block;

    if (b == 0) {
	// block 0 is used by LSOBasicDir when requesting a page 
	block = new BlockCacheEntryK42(b, d);
	block->addUser();
	block->setNotEmpty();
	return block;
    }

    KFSDHash::AllocateStatus st;
    HashData *data;
    (void) blockHash.findOrAddAndLock(b, &data, &st);
    if (st == KFSDHash::NOT_FOUND) {
	block = new BlockCacheEntryK42(b, d);
	data->setData(block);
	BCSTAT(MISS); // collect statistic about BlockCache
	BCSTAT(ALLOC); // collect statistic about BlockCache
    } else {
	block = (BlockCacheEntryK42*) data->getData();
	BCSTAT(HIT); // collect statistic about BlockCache
    }
    block->addUser();
    data->unlock();
    return block;
}

/* virtual */ BlockCacheEntry *
BlockCacheK42::getBlockRead(uval32 b)
{
    DBC("getBlockRead", b);

    BlockCacheEntry *entry = getBlock(b);
    entry->readData();
    return entry;
}

/* virtual */ void
BlockCacheK42::freeBlock(BlockCacheEntry *b)
{
    uval32 blkno = b->getBlockNumber();

    DBC("freeBlock", blkno);

    if (blkno != 0) { 	// we don't add blkno 0 to the cache
	// Sanity check; not necessary in terms of functionality
	HashData *data;
	(void)blockHash.findAndLock(blkno, &data);
	tassertMsg(data != NULL, "freeing block that is not there\n");
	data->unlock();
    }

    BlockCacheEntryK42 *block = (BlockCacheEntryK42*)b;
    block->removeUser();
    tassertMsg(block->getUsers() >= 0, "users %ld\n", block->getUsers());

    if (block->getUsers() == 0) {
#ifdef FREEING_ENTRY_WHEN_NO_USERS
	/* FIXME Notice that for now we're not freeing anything from the cache
	 * hash (the entry is only marked as "empty" in the DHash); if
	 * we were ideally we should have a good cache eviction instead of: */
	if (blkno) { 	// we don't add blkno 0 to the cache
	    if (block->isDirty) {
		block->d->writeBlock(block->blkno, block->data);
		block->isDirty = 0;
	    }
	    blockHash.removeData(blkno);
	    BCSTAT(FREE); // collect statistic about BlockCache
	    // for now we'll leak it, so let's output it
	} else {
	    tassertMsg(0, "debugging");
	    delete block;
	}
#else //#ifdef FREEING_ENTRY_WHEN_NO_USERS
	// Check if it's just an (annonymous) empty page, no need to panic.
	// Just kill/free it.
	if (!blkno) {
	    delete block;
	}
#endif //#ifdef FREEING_ENTRY_WHEN_NO_USERS
    }
}

/* virtual */ void
BlockCacheK42::updateBlockNumber(BlockCacheEntry *entry, uval32 newblk)
{
    DBC("updateBlockNumber", newblk);

    uval32 oldblk = entry->getBlockNumber();
    HashData *newdata;
    KFSDHash::AllocateStatus st;

    if (oldblk != 0) { // we don't keep 0 there
	HashData *olddata;
	(void)blockHash.findAndLock(oldblk, &olddata);
	tassertMsg(olddata != NULL, "updating block that is not there\n");
	olddata->unlock();
	// new key being used, so get rid of other one
	blockHash.removeData(oldblk);
    }

    (void) blockHash.findOrAddAndLock(newblk, &newdata, &st);
    if (st == KFSDHash::NOT_FOUND) {
	newdata->setData(entry);
	newdata->unlock();
    } else {
	tassertMsg(0, "ops! newblk %u already in cache?!\n", newblk);
    }

    ((BlockCacheEntryK42*)entry)->setBlockNumber(newblk);
}

/* virtual */ uval32
BlockCacheK42::markDirty(BlockCacheEntry *entry)
{
#ifndef NDEBUG
    // It seems the problem with TimerEvents has been fixed, but
    // I'm letting this check in (for partDeb, fullDeb) to catch
    // other misbehavior in the TimerEvent space
    BlockCacheEntryK42 *e = dirtyQueue.getHead();
    // we're not locking the entry, so it may be that it's being flushed as
    // we look at it
    if (e && e->getFlushTime() && Scheduler::SysTimeNow() >
	e->getFlushTime() + Scheduler::TicksPerSecond()*90) {
	// check if we have recently created a new timer
	if (Scheduler::SysTimeNow() > lastTimerCreationTime
	    + Scheduler::TicksPerSecond()*45) {
	    tassertMsg(0, "It seems the events stopped being generated!!!\n");
	    TimerEventBC *t = new TimerEventBC(this);
	    TimerEventBC::ScheduleEvent(t);
	    lastTimerCreationTime = Scheduler::SysTimeNow();
	}
    }
#endif // #ifndef NDEBUG

    uval wasDirty = ((BlockCacheEntryK42*)entry)->getIsDirty();
    uval32 blk = ((BlockCacheEntryK42*)entry)->markDirty();
    if (wasDirty == 0) { // just became dirty
	dirtyQueue.add((BlockCacheEntryK42*)entry);
    }
    return blk;
}

/* virtual */ SysStatus
BlockCacheK42::flush(uval checkFlushTime /* = 1 */)
{
    uval oldDoingFlush = FetchAndAddSignedVolatile(&doingFlush, 1);
    if (oldDoingFlush == 1) {
	tassertWrn(0, "flush() found other flush operation still going on\n");
	return _SERROR(2393, 0, 0);
    }

    uval counter = 0;
    BlockCacheEntryK42 *entry = dirtyQueue.getHead();
    while (entry) {
	if (entry->getIsDirty()) {
	    if (checkFlushTime != 1 ||
		entry->getFlushTime() <= Scheduler::SysTimeNow()) {
		// remove entry from list
		// This is the only routine that manipulates de head of
		// the list, so the element we just retrieve still has to
		// be there for removal
		BlockCacheEntryK42 *head = dirtyQueue.remove();
		passertMsg(head == entry, "not expecting this\n");
		//err_printf("It's going to invoke flush (pid %ld)\n",
		//	   _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));
		entry->flush();
	    } else {
		// can stop flushing things to disk
		break;
	    }
	} else {
	    // the entry has been cleaned up, so get it out of the list
	    BlockCacheEntryK42 *head = dirtyQueue.remove();
	    passertMsg(head == entry, "not expecting this\n");
	}
	counter++;
	entry = dirtyQueue.getHead();
    }

    doingFlush = 0;

    KFS_DPRINTF(DebugMask::CACHE_SYNC,
		"Leaving BlockCacheK42::flush() counter %ld\n", counter);

    return 0;
}

/* virtual */ SysStatus
BlockCacheK42::readBlock(uval32 pblkno, char *buffer, uval local)
{
    DBC("readBlock", pblkno);
    // For now bypassing the buffer cache when the application asks for
    // a block or writes a block ...
    sval rc;
    if (local == PSO_LOCAL) {
	rc = d->readBlock(pblkno, buffer);
    } else {
	rc = d->aReadBlock(pblkno, buffer);
    }
    return rc;
}

/* virtual */ SysStatus
BlockCacheK42::writeBlock(uval32 pblkno, char *buffer, uval local)
{
    DBC("writeBlock", pblkno);

    // For now bypassing the buffer cache when the application asks for
    // a block or writes a block ...

    sval rc;
    if (local == PSO_LOCAL)
        rc = d->writeBlock(pblkno, buffer);
    else
        rc = d->aWriteBlock(pblkno, buffer);

    return rc;
}

/* virtual */ void
BlockCacheK42::forget(uval32 b)
{
    HashData *data;
    (void)blockHash.findAndLock(b, &data);
    if (data) {
	((BlockCacheEntryK42*)data->getData())->markClean();
	data->unlock();
	blockHash.removeData(b);
    }
}

#ifdef GATHER_BC_STATS
void
BlockCacheK42::incStat(StatType st)
{
    switch (st) {
    case MISS:
    case HIT:
    case ALLOC:
    case FREE:
	stats[st]++;
	break;
    default:
	passertMsg(0, "invalid");
    }
}

void
BlockCacheK42::printStats()
{
    err_printf("BlockCache Stats:\n"
	       "\tMISS: %ld\n\tHIT: %ld\n\tALLOC: %ld\n\tFREE: %ld\n",
	       stats[MISS], stats[HIT], stats[ALLOC], stats[FREE]);
}
#endif // #ifdef GATHER_BC_STATS

/* virtual */ void
BlockCacheK42::sync()
{
    SysStatus rc;
    uval count = 0;
    do {
	rc = flush(0);
	count ++;
	passertWrn(_SUCCESS(rc), "invocation of BlockCacheK42::flush(0) "
		   "returned failure rc 0x%lx (count is %ld)\n", rc, count);
    } while (_FAILURE(rc) && count < 10);

    tassertWrn(_SUCCESS(rc), "BlockCacheK42::sync() has not been executed\n");
}
