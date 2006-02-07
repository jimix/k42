#ifndef __BLOCK_CACHE_LINUX_H_
#define __BLOCK_CACHE_LINUX_H_

/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: BlockCacheLinux.H,v 1.2 2004/03/01 17:51:26 dilma Exp $
 **************************************************************************/

#include "BlockCache.H"
#include "KFSHash.H"
#include "PSOBase.H"
#include "sys/types.H"

class Disk;

/*
 *  An entry in the block cache. This should make it easier for block
 *  cache user's to keep a reference to the desired blocks.
 */
class BlockCacheEntryLinux : public BlockCacheEntry {
    friend class BlockCacheLinux;
protected:
    uval32 blkno;
    char *data;//[OS_BLOCK_SIZE];
    uval users;
    uval uptodate;

public:
    DEFINE_GLOBAL_NEW(BlockCacheEntryLinux);
    // Constructor has to receive a block number to map to
    BlockCacheEntryLinux(uval32, Disk *);
    virtual ~BlockCacheEntryLinux();

    // should return a OS_BLOCK_SIZE page with the block's content
    virtual char *getData();

    // mark this block as clean
    virtual void markClean();

    // read in (from disk) this block's data
    virtual void readData();
    virtual uval32 getBlockNumber() { return blkno; }

protected:
    // The following methods are involed by BlockCache clients (a friend class)
    // mark this block as dirty
    virtual uval32 markDirty();
    virtual void setBlockNumber(uval32 b);
    virtual uval removeUser() { return --users; }
    virtual void addUser() { users++; }
    virtual uval hasUsers() { return users; }
};

/*
 * The actual BlockCache class
 */ 
class BlockCacheLinux : public BlockCache {
protected:
    Disk *d;
    KFSHash<uval32, BlockCacheEntryLinux *> blockHash;
public:
    DEFINE_GLOBAL_NEW(BlockCacheLinux);

    // constructors
    BlockCacheLinux(Disk *disk):d(disk), blockHash() { }

    // should actually keep track of all blocks and force-delete them
    // here
    virtual ~BlockCacheLinux();

    virtual BlockCacheEntry *getBlock(uval32);
    virtual BlockCacheEntry *getBlockRead(uval32 b);
    virtual void freeBlock(BlockCacheEntry *);

    virtual uval32 markDirty(BlockCacheEntry *entry);
    virtual void updateBlockNumber(BlockCacheEntry *entry, uval32 b);

    virtual SysStatus readBlock(uval32, char*, uval);
    virtual SysStatus writeBlock(uval32, char*, uval);

    virtual void forget(uval32 block);

    virtual void sync() {
	// This is a no-op in Linux
    }

    // Linux specific method
    virtual uval releasePage(uval32 blkno);
};

#endif // #ifndef __BLOCK_CACHE_LINUX_H_
