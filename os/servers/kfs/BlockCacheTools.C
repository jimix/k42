/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * Changes by the K42 project, 2003, 2004 (k42@watson.ibm.com)
 *
 * $Id: BlockCacheTools.C,v 1.12 2005/04/15 17:39:37 dilma Exp $
 **************************************************************************/

#include "kfsIncs.H"
#include "BlockCacheTools.H"
#include "Disk.H"
#include "PSOBase.H"
#include "sys/types.H"

#include "KFSDebug.H"

#define DBC(meth, blkno)  KFS_DPRINTF(DebugMask::CACHE, \
                                      "BlockCacheTools::%s blkno %u\n", \
                                      meth, blkno)
#define DBCE(meth, blkno) KFS_DPRINTF(DebugMask::CACHE, \
                                      "BlockCacheEntryTools::%s blkno %u\n",\
                                      meth, blkno)

BlockCacheEntryTools::BlockCacheEntryTools(uval32 b, Disk *disk) : blkno(b)
{
    DBCE("constructor", b);
    data = new char[OS_BLOCK_SIZE];
    //users = 0;
    users = 1;
    d = disk;
    lock.init();
}

BlockCacheEntryTools::~BlockCacheEntryTools()
{
    DBCE("destructor", blkno);
    delete[] data;
}

// should return a OS_BLOCK_SIZE page with the block's content
char *
BlockCacheEntryTools::getData()
{
    DBCE("getData", blkno);
    return data;
}

// mark this block as dirty
uval32
BlockCacheEntryTools::markDirty()
{
    DBCE("markDirty", blkno);
    if (blkno) {
	d->writeBlock(blkno, data);
    }
    return 0;
}

// mark this block as clean
void
BlockCacheEntryTools::markClean()
{
    DBCE("markClean", blkno);
    // nothing to do
}

// read in (from disk) this block's data
void
BlockCacheEntryTools::readData()
{
    DBCE("readData", blkno);
    if (blkno) {
	d->readBlock(blkno, data);
    }
}

uval
BlockCacheEntryTools::removeUser()
{
    DBCE("removeUser", blkno);
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return --users;
}

void
BlockCacheEntryTools::addUser()
{
    DBCE("addUser", blkno);
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    users++;
}

BlockCacheTools::BlockCacheTools(Disk *disk) : d(disk)
{
    blockHash = new KFSHash<uval32, BlockCacheEntryTools *>;
}

BlockCacheTools::~BlockCacheTools()
{
    blockHash->destroy();
    delete blockHash;
}

BlockCacheEntry *
BlockCacheTools::getBlock(uval32 b)
{
    DBC("getBlock", b);

    BlockCacheEntryTools *block;

    if (b == 0) {
	// block 0 is used by LSOBasicDir when requesting a page 
	block = new BlockCacheEntryTools(b, d);
	block->addUser();
	return block;
    }

    KFSHashEntry<BlockCacheEntryTools *> *entry;
    uval found  = blockHash->findAddLock(b, &entry);
    if (!found) {
	block = new BlockCacheEntryTools(b, d);
	entry->setData(block);
    } else {
	block = entry->getData();
    }
    block->addUser();
    entry->unlock();

    return block;
}

BlockCacheEntry *
BlockCacheTools::getBlockRead(uval32 b)
{
    DBC("getBlockRead", b);
    BlockCacheEntry *entry = getBlock(b);
    entry->readData();
    return entry;
}

void
BlockCacheTools::freeBlock(BlockCacheEntry *blk)
{
    BlockCacheEntryTools *b = (BlockCacheEntryTools *) blk;
    uval32 blkno = b->getBlockNumber();
    DBC("freeBlock", blkno);

    KFSHashEntry<BlockCacheEntryTools *> *entry;
    uval found  = blockHash->findLock(blkno, &entry);
    if (found) {
	if (!b->removeUser()) {
	    if (blkno) {  	// we don't add blkno 0 to the cache
		blockHash->removeUnlock(entry);
	    }
	    delete b;
	} else {
	    entry->unlock();
	}
    } else {
	tassertMsg(blkno == 0, "shouldn't happen\n");
	if (!b->removeUser()) {
	    delete b;
	}
    }
	    
}

void
BlockCacheTools::updateBlockNumber(BlockCacheEntry *bentry, uval32 nb)
{
    DBC("updateBlockNumber", nb);

    KFSHashEntry<BlockCacheEntryTools *> *entry;
    uval found  = blockHash->findAddLock(nb, &entry);
    if (!found) {
	// no need to change "users" field
	
	if (bentry->getBlockNumber()) {  // 0 wouldn't be in the hash
	    KFSHashEntry<BlockCacheEntryTools *> *oldentry;
	    uval fnd  = blockHash->findLock(bentry->getBlockNumber(),
					    &oldentry);
	    if (fnd) {
		blockHash->removeUnlock(oldentry);
	    } else {
		passertMsg(0, "shouldn't happen\n");
	    }
	}
	((BlockCacheEntryTools*)bentry)->setBlockNumber(nb);
	entry->setData((BlockCacheEntryTools*)bentry);
	entry->unlock();
    } else {
	// This shouldn't happen!
	passertMsg(0, "Trying to change to already "
	       "registered block=0x%p, num=%u\n", bentry, nb);
    }
}

/* virtual */ uval32
BlockCacheTools::markDirty(BlockCacheEntry *entry)
{
    return ((BlockCacheEntryTools*)entry)->markDirty();
}

SysStatus
BlockCacheTools::readBlock(uval32 pblkno, char *buffer,
			   void *cont /* = NULL */) 
{
    DBC("readBlock", pblkno);
    sval rc;

    if (cont == NULL) { // synchronous I/O
        rc = d->readBlock(pblkno, buffer);
    } else {
        rc = d->aReadBlock(pblkno, buffer, (PSOBase::AsyncOpInfo*)cont);
    }
    return rc;
}

SysStatus
BlockCacheTools::writeBlock(uval32 pblkno, char *buffer,
			    void *cont /* = NULL */) 
{
    DBC("writeBlock", pblkno);
    sval rc  = 0;
     
    if (cont == NULL) { // synchronous operation
        rc = d->writeBlock(pblkno, buffer);
    } else {
        rc = d->aWriteBlock(pblkno, buffer, (PSOBase::AsyncOpInfo*)cont);
    }
    return rc;
}

void
BlockCacheTools::forget(uval32 blkno)
{
    DBC("forget", blkno);
    if (blkno) { // we don't add 0 to hash
	KFSHashEntry<BlockCacheEntryTools *> *entry;
	if (blockHash->findLock(blkno, &entry)) {
	    entry->getData()->markClean();
	    blockHash->removeUnlock(entry);
	}
    }
}
