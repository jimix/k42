#ifndef __BLOCK_CACHE_H_
#define __BLOCK_CACHE_H_

/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * Changes by the K42 project (k42@watson.ibm.com)
 *
 * $Id: BlockCache.H,v 1.2 2004/03/01 17:51:19 dilma Exp $
 **************************************************************************/

/*
 * This is the "central" block cache for KFS. The function of this
 * class is to communicate with 2 parts of the OS:
 *
 *  o The memory management system. This class should provide memory
 *    pages (representing disk blocks) for KFS' metadata.
 *
 *  o IO system. This class should set everything up for the OS to do
 *    read/writes on the blocks it contains.
 *
 *  The actual implementation of this class is OS-specific, as the OS
 *  might do block caching already, and it is essencial
 *  (performance-wise) to have this class make use of OS-specific
 *  caches.
 *
 */

class Disk;
class KFSGlobals;

class BlockCacheEntry {
    friend class BlockCache;
public:
    DEFINE_GLOBAL_NEW(BlockCacheEntry);
    virtual ~BlockCacheEntry() {};
    // should return a OS_BLOCK_SIZE page with the block's content
    virtual char *getData() = 0;

    // read in (from disk) this block's data
    virtual void readData() = 0;

    // mark this block as clean
    virtual void markClean() = 0;

    virtual uval32 getBlockNumber() = 0;

protected: // methods to be invoked by BlockCache clients only
    virtual void setBlockNumber(uval32 b) = 0;
    // mark this block as dirty
    virtual uval32 markDirty() = 0;
};

class BlockCache {
public:
    static BlockCache* CreateBlockCache(Disk *d, KFSGlobals *gl);

    // should actually keep track of all blocks and force-delete them
    // here
    virtual ~BlockCache() {}

    virtual BlockCacheEntry *getBlock(uval32 block) = 0;
    virtual BlockCacheEntry *getBlockRead(uval32 block) = 0; 
    virtual void freeBlock(BlockCacheEntry *entry) = 0;

    virtual uval32 markDirty(BlockCacheEntry *entry) = 0;

    // this is necessary as it might be necessary to re-hash this entry
    virtual void updateBlockNumber(BlockCacheEntry *entry, uval32 b) = 0;

    virtual SysStatus readBlock(uval32 pblkno, char *buffer, uval local) = 0;
    virtual SysStatus writeBlock(uval32 pblkno, char *buffer, uval local) = 0;

    virtual void forget(uval32 block) = 0;

    virtual void sync() = 0;
};

#endif // #ifndef __BLOCK_CACHE_H_


