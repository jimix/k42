#ifndef __BLOCK_CACHE_TOOLS_H_
#define __BLOCK_CACHE_TOOLS_H_

/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * Changes by Dilma M. a Silva, from the IBM K42 project
 *            (dilma@watson.ibm.com)
 *
 * $Id: BlockCacheTools.H,v 1.2 2004/03/01 17:51:19 dilma Exp $
 **************************************************************************/

/*
 * This is an implementation of BlockCache to be used by KFS Tools, i.e.,
 * it does not rely on any OS specific support. It's a dummy cache.
 */

#include "BlockCache.H"
#include <sync/BLock.H>
#include "KFSHash.H"

class BlockCacheEntryTools : public BlockCacheEntry {
    friend class BlockCacheTools;
    uval32 blkno;
    char *data;//[OS_BLOCK_SIZE];
    Disk *d;
    uval users;
    BLock lock; /* FIXME: this lock is just to protect variable users; should
		 * use atomic inc/dec instead */
public:
    DEFINE_GLOBAL_NEW(BlockCacheEntryTools);
    // Constructor has to receive a block number to map to
    BlockCacheEntryTools(uval32 block, Disk *d);
    virtual ~BlockCacheEntryTools();

    // should return a OS_BLOCK_SIZE page with the block's content
    virtual char *getData();

    // read in (from disk) this block's data
    virtual void readData();

    // mark this block as clean
    virtual void markClean();

    virtual uval32 getBlockNumber() { return blkno; }

protected: // invoked by BlockCacheTools, a friend
    virtual void setBlockNumber(uval32 b) { blkno = b; }
    uval removeUser();
    void addUser();
    // mark this block as dirty
    virtual uval32 markDirty();
};

class BlockCacheTools : public BlockCache {
protected:
    Disk *d;
    KFSHash<uval32,BlockCacheEntryTools*> *blockHash;
public:
    DEFINE_GLOBAL_NEW(BlockCacheTools);
    BlockCacheTools(Disk *disk);

    // should actually keep track of all blocks and force-delete them
    // here
    virtual ~BlockCacheTools();

    virtual BlockCacheEntry *getBlock(uval32 block);
    virtual BlockCacheEntry *getBlockRead(uval32 block); 
    virtual void freeBlock(BlockCacheEntry *entry);


    virtual uval32 markDirty(BlockCacheEntry *entry);
    virtual void updateBlockNumber(BlockCacheEntry *entry, uval32 b);

    virtual SysStatus readBlock(uval32 pblkno, char *buffer, uval local);
    virtual SysStatus writeBlock(uval32 pblkno, char *buffer, uval local);

    virtual void forget(uval32 block);

    virtual void sync() { } // no op
};

#endif // #ifndef __BLOCK_CACHE_H_
