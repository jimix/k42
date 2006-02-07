/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * Changes/additions by the K42 group, 2004.
 *
 * $Id: BlockCacheLinux.C,v 1.11 2004/09/15 20:50:02 dilma Exp $
 **************************************************************************/

#include "kfsIncs.H"
#include "BlockCache.H"
#include "BlockCacheLinux.H"
#include "Disk.H"

extern "C" {
    void * LinuxGetBlock(uval32 b);
    void LinuxFreeBlock(void *bh);
    char *LinuxGetBlockData(void *bh);
    void LinuxDirtyBlock(void *b);
    void LinuxCleanBlock(void *b);
    void LinuxReadBlock(void *b);

    void *LinuxGetPage(uval32 b);
    void LinuxFreePage(void *page);
    char *LinuxGetPageData(void *page);
    void LinuxDirtyPage(void **page, uval32 b);
    void LinuxCleanPage(void *page, uval32 b);
    void LinuxReadPage(void *page, uval32 b);

    void *LinuxAllocPage();
}

/* BlockCacheEntry */

BlockCacheEntryLinux::BlockCacheEntryLinux(uval32 b, Disk *disk) : 
    blkno(b), users(0), uptodate(0)
{
    // Undetermined block number! The best we can do is return a
    // null-page
    if (!blkno) {
	data = (char *)LinuxAllocPage();
	memset(getData(), 0, OS_BLOCK_SIZE);
    }
    else {
	data = (char *)LinuxGetPage(b);
    }
}

BlockCacheEntryLinux::~BlockCacheEntryLinux()
{
    passertMsg(!blkno || !users, "Deleting entry=0x%p, data 0x%p, users=%lu\n",
	       this, data, users);

    LinuxFreePage((void *)data);
}

// should return a OS_BLOCK_SIZE page with the block's content
char *
BlockCacheEntryLinux::getData()
{
    return LinuxGetPageData((void *)data);
}

// mark this block as dirty
uval32
BlockCacheEntryLinux::markDirty()
{
    if (blkno) {
	LinuxDirtyPage((void **)&data, blkno);
	uptodate = 1;
    } else {
	passertMsg(0, "BlockCacheEntry::markDirty() no block!\n");
    }
    return 0;
}

// mark this block as clean
void
BlockCacheEntryLinux::markClean()
{
//    if (blkno) {
    LinuxCleanPage((void *)data, blkno);
    uptodate = 1;
//    }
}

// read in (from disk) this block's data
void
BlockCacheEntryLinux::readData()
{
    if (blkno && !uptodate) {
	LinuxReadPage((void *)data, blkno);
	uptodate = 1;
    }
}

void
BlockCacheEntryLinux::setBlockNumber(uval32 b) 
{
    if (blkno != b) {
	char *newData = (char *)LinuxGetPage(b);
	
	if (data) {
	    memcpy(LinuxGetPageData((void *)newData), getData(), OS_BLOCK_SIZE);
	    LinuxFreePage((void *)data);
	}
	data = newData;
	blkno = b;
    }
}

/* BlockCache */

BlockCacheLinux::~BlockCacheLinux()
{
    blockHash.removeAll();
    blockHash.destroy();
}

BlockCacheEntry *
BlockCacheLinux::getBlock(uval32 b)
{
    BlockCacheEntryLinux *block;

    if (!b) {
    	// block 0 is used by LSOBasicDir when requesting a page 
	block = new BlockCacheEntryLinux(b, d);
	block->addUser();
	return block;
    }

    KFSHashEntry<BlockCacheEntryLinux *> *entry;
    uval found = blockHash.findAddLock(b, &entry);
    if (!found) {
	block = new BlockCacheEntryLinux(b, d);
	passertMsg(block, "Something's wrong here!\n");
	entry->setData(block);
    } else {
	block = entry->getData();
    }

    block->addUser();
    entry->unlock();
    return block;
}

BlockCacheEntry *
BlockCacheLinux::getBlockRead(uval32 b)
{
    BlockCacheEntry *entry = getBlock(b);
    entry->readData();
    return entry;
}

void
BlockCacheLinux::freeBlock(BlockCacheEntry *blk)
{
    BlockCacheEntryLinux *b = (BlockCacheEntryLinux *) blk;
    uval32 blkno = b->getBlockNumber();

    if (blkno) {
	KFSHashEntry<BlockCacheEntryLinux *> *entry;
	uval found  = blockHash.findLock(blkno, &entry);
	if (found) {
	    // Just remove the user, and let Linux call releasepage later
	    b->removeUser();
	    entry->unlock();
	}
    } else {
	b->removeUser();
	delete b;
    }
}

void
BlockCacheLinux::updateBlockNumber(BlockCacheEntry *bentry, uval32 nb)
{
    KFSHashEntry<BlockCacheEntryLinux *> *entry, *oldentry;
    BlockCacheEntryLinux *block = (BlockCacheEntryLinux *)bentry;
    uval found  = blockHash.findAddLock(nb, &entry);

    if (!found) {
	// no need to change "users" field
	
	if (block->getBlockNumber()) {  // 0 wouldn't be in the hash
	    uval found  = blockHash.findLock(block->getBlockNumber(),
					     &oldentry);
	    if (found) {
		blockHash.removeUnlock(oldentry);
	    } else {
		passertMsg(0, "shouldn't happen\n");
	    }
	}
	
	block->setBlockNumber(nb);
	entry->setData(block);
	entry->unlock();
    } else {
	// This shouldn't happen!
	passertMsg(0, "Trying to change to already "
	       "registered block=0x%p, num=%u\n", bentry, nb);
    }
}

uval32
BlockCacheLinux::markDirty(BlockCacheEntry *entry)
{
    return ((BlockCacheEntryLinux*)entry)->markDirty();
}

SysStatus
BlockCacheLinux::readBlock(uval32 pblkno, char *buffer,
			   PSOBase::AsyncOpInfo *cont /* = NULL */)
{
    sval rc;

    if ( cont == NULL) { // sync operation
        rc = d->readBlock(pblkno, buffer);
    } else {
        rc = d->aReadBlock(pblkno, buffer);
    }
    return rc;
}

SysStatus
BlockCacheLinux::writeBlock(uval32 pblkno, char *buffer,
			    PSOBase::AsyncOpInfo *cont /* = NULL */)
{
    sval rc;

    if (cont == NULL) { // sync operation
        rc = d->writeBlock(pblkno, buffer);
    } else {
        rc = d->aWriteBlock(pblkno, buffer);
    }

    return rc;
}

void
BlockCacheLinux::forget(uval32 blkno)
{
    if (blkno) { // we don't add 0 to hash
	KFSHashEntry<BlockCacheEntryLinux *> *entry;
	if (blockHash.findLock(blkno, &entry)) {
	    entry->getData()->markClean();
	    blockHash.removeUnlock(entry);
	}
    }
}

/*
 * Returns 1 is Linux may free this page, and 0 otherwise
 *
 */
uval
BlockCacheLinux::releasePage(uval32 blkno)
{
    KFSHashEntry<BlockCacheEntryLinux *> *entry;
    uval ret = blockHash.findLock(blkno, &entry);

    if (!ret) {
	// is this a problem?
	return 1;
    }

    BlockCacheEntryLinux* bentry = entry->getData();
    if (!bentry->hasUsers()) {
	blockHash.removeUnlock(entry);
	delete bentry;
	return 1;
    }

    entry->unlock();
    return 0;
}
