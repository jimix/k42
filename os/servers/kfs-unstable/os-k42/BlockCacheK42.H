#ifndef __BLOCK_CACHE_K42_H_
#define __BLOCK_CACHE_K42_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockCacheK42.H,v 1.2 2004/03/01 17:51:25 dilma Exp $
 *****************************************************************************/

#include "BlockCache.H"
#include "KFSDHash.H"
#include "sys/types.H"
#include "TimerEventBC.H"
#include "defines.H"

#ifdef GATHER_BC_STATS
#define BCSTAT(st) incStat(st);
#else
#define BCSTAT(st)
#endif // #ifdef GATHER_BC_STATS

class BlockCacheEntryK42 : public BlockCacheEntry {
    friend class BlockCacheK42;

protected:
    uval32 blkno;
    uval isEmpty;
    char *data;//[OS_BLOCK_SIZE];
    Disk *d;
    uval users;
    uval isDirty;
    SysTime flushTime; /* earliest time we want to have this entry flushed
			* by the periodic execution of BlockCache->flush() */
    BlockCacheEntryK42 *nextDirty;

protected:
    static const uval FLUSH_TIME = 30; /* minimum time (in secs) between
					* block cache entry becoming dirty and
					* the time the BlockCache flush daemon
					* writing it out */
 
    // following protected methods will be used by BlockCache
    virtual uval32 markDirty();
    virtual void setBlockNumber(uval32 b) { blkno = b; }
    uval getIsDirty() { return isDirty; }
    SysTime getFlushTime() { return flushTime; }
    void flush();
    uval removeUser();
    uval getUsers() { return users; }

public:
    void addUser();
    void setNotEmpty() { isEmpty = 0; }

    // currently used by BlockCacheK42::DirtyQueue
    void setNextDirty(BlockCacheEntryK42* entry) { nextDirty = entry; }
    BlockCacheEntryK42 *getNextDirty() { return nextDirty; }


public:
    DEFINE_GLOBAL_NEW(BlockCacheEntryK42);
    // Constructor has to receive a block number to map to
    BlockCacheEntryK42(uval32, Disk *);
    virtual ~BlockCacheEntryK42();

    // should return a OS_BLOCK_SIZE page with the block's content
    virtual char *getData();

    // read in (from disk) this block's data
    virtual void readData();

    // mark this block as clean
    virtual void markClean();

    virtual uval32 getBlockNumber() { return blkno; }
};

/*
 * The actual BlockCache class
 */ 
class BlockCacheK42 : public BlockCache {
protected:
    Disk *d;
    KFSDHash blockHash;
    TimerEventBC te;
    sval doingFlush; // flag to indicate a flush operation is going on
    
#ifndef NDEBUG // trying to make sure the timer events are being triggered
    SysTime lastTimerCreationTime;
#endif // #ifndef NDEBUG

public:
    class DirtyQueue {
	BlockCacheEntryK42 *firstDirty, *lastDirty;
	BLock lock;
    public:
	DEFINE_GLOBAL_NEW(DirtyQueue);
	DirtyQueue() : firstDirty(NULL), lastDirty(NULL) {
	    lock.init();
	}
	BlockCacheEntryK42 *getHead() {
	    lock.acquire();
	    BlockCacheEntryK42 *entry = firstDirty;
	    lock.release();
	    return entry;
	}
	// adds to end of queue
	void add(BlockCacheEntryK42 *entry) {
	    lock.acquire();
	    if (lastDirty == NULL) {
		tassertMsg(firstDirty == NULL, "?");
		firstDirty = lastDirty = entry;
		tassertMsg(entry->getNextDirty() == NULL, "?");
	    } else {
		lastDirty->setNextDirty(entry);
		lastDirty = entry;
	    }
	    lock.release();
	}
	// remove from front of queue
	BlockCacheEntryK42* remove() {
	    lock.acquire();
	    BlockCacheEntryK42 *entry = firstDirty;
	    if (entry) {
		firstDirty = entry->getNextDirty();
		entry->setNextDirty(NULL);
		if (firstDirty == NULL) {
		    lastDirty = NULL;
		}
	    }
	    lock.release();
	    return entry;
	}
    };
    DirtyQueue dirtyQueue;

protected:
#ifdef GATHER_BC_STATS
    // for gathering statistics
    typedef enum {MISS, HIT, ALLOC, FREE, MAX_STAT} StatType;
    uval stats[MAX_STAT+1];
    void incStat(StatType st);
public:
    void printStats();
#endif // #ifdef GATHER_BC_STATS

public:
    DEFINE_GLOBAL_NEW(BlockCacheK42);

    BlockCacheK42(Disk *disk);

    // should actually keep track of all blocks and force-delete them
    // here
    virtual ~BlockCacheK42() { }


    virtual BlockCacheEntry *getBlock(uval32);
    virtual BlockCacheEntry *getBlockRead(uval32 b);
    virtual void freeBlock(BlockCacheEntry *);

    virtual uval32 markDirty(BlockCacheEntry *entry);
    // this is necessary as it might be necessary to re-hash this entry
    virtual void updateBlockNumber(BlockCacheEntry *entry, uval32 b);

    virtual SysStatus readBlock(uval32, char*, uval);
    virtual SysStatus writeBlock(uval32, char*, uval);

    virtual void forget(uval32 block);

    // checkFlushTime == 1 means that a dirty entry in the BlockCache is only
    // flushed  if it's flushTime timestamp mandates so; a value different
    // from 1 means that dirty entries are synced regardless of how recently
    // they have been added to the dirty list
    virtual SysStatus flush(uval checkFlushTime = 1);

    virtual void sync();
};

#endif // #ifndef __BLOCK_CACHE_K42_H_
