/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Many corrections/changes by Livio Soares (livio@ime.usp.br)
 *
 * $Id: RecordMap.C,v 1.4 2004/05/06 19:52:49 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "RecordMap.H"
#include "KFSGlobals.H"
#include "PSOTypes.H"
#include "ObjToken.H"
#include "SuperBlock.H"

/*
 * RecordMap()
 *
 *   Constructs a new RecordMap from the given "object related state"
 */
RecordMap::RecordMap(uval32 b, KFSGlobals *g)
    : globals(g), flags(RMAP_NONE), blkno(b), shutdown(0), soHash()
{
    lock.init();
    dataBlock = g->blkCache->getBlockRead(blkno);
    // set the appropriate metadata
    data = (RecordMapStruct *)dataBlock->getData();
}

void
RecordMap::init()
{
    ObjTokenID psoid = {0};
    pso = (PSOBase *)globals->soAlloc->alloc(&psoid, this,
				     (PsoType) data->getPsoRecordType());
    passertMsg(pso, "RecordMap::init() Problem with PSO!\n");
}

RecordMap::~RecordMap()
{
    soHash.removeAll();
    soHash.destroy();
    delete pso;

    if (dataBlock) {
	globals->blkCache->freeBlock(dataBlock);
    }
}

/*
 * allocRecord()
 *
 *   Allocates the requested number of records in a sequential run.
 *   It returns an offset to the start of the first record.
 */
SysStatus
RecordMap::allocRecord(PsoType type, ObjTokenID *offset)
{
    sval rc;

    rc = internal_allocRecord(type, offset);

    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::RECORD_MAP,
		    "RecordMap::allocRecords() Problem allocating record"
		    " type=%u\n", type);
	// release the entries we've allocated
	freeRecord(offset);
	return rc;
    }

    KFS_DPRINTF(DebugMask::RECORD_MAP, "AllocRecord for type %d got id %ld\n",
		(uval32) type, (uval) offset->id);

    // write back the ServerObject type for this record
    setRecordType(offset, type);

    // Get location for this ServerObject
    // figure out what the location should be based on the type
    rc = globals->soAlloc->locationAlloc(type, offset, this);
    if (_FAILURE(rc)) {
        err_printf("RecordMap::allocRecords() Problem allocating ServerObject"
		   " type=%u\n", type);
        // release the entries we've allocated
        freeRecord(offset);
        return rc;
    }

    return rc;
}

/*
 * internal_allocRecord()
 *
 *   Searches for a free record linearly through the bitvector.
 *   I think we could do better... 
 *   Used internally in allocRecord().
 */
SysStatus
RecordMap::internal_allocRecord(PsoType type, ObjTokenID *offset)
{
    BlockCacheEntry *block = NULL;
    uval32 *bitmap = NULL;
    uval i, j = RMAP_BITS_PAGE;

    lock.acquire();

    for (i = 0; data->getBitmap(i) && i < RMAP_BITMAP_BLOCKS; i++) {
	// look inside the i-th block
	block = globals->blkCache->getBlockRead(data->getBitmap(i));
	bitmap = (uval32*) block->getData();
	
	// search the bitmap for free record
	for (j = 0; j < RMAP_BITS_PAGE; j++) {
	    if (bitmap[j/32] == 0xffffffff) { // not doing silly conversion
		j+=31;
		continue;
	    }
	    // check if the bit is set
	    if (!(TE32_TO_CPU(bitmap[j/32]) & (1 << (j%32)))) {
		goto out;
	    }
	}
	globals->blkCache->freeBlock(block);
    }

  out:

    // if we didn't find enough, allocate a new block for the bitmap, and retry
    if (j == RMAP_BITS_PAGE) {
	if (i < RMAP_BITMAP_BLOCKS && !data->getBitmap(i)) {
	    data->setBitmap(i, globals->super->allocBlock());
	    block = globals->blkCache->getBlock(data->getBitmap(i));
	    bitmap = (uval32*) block->getData();
	    memset(bitmap, 0, OS_BLOCK_SIZE); // zero-out the block
	    j = 0; // it's a new block! j = 0 is _certainly_ free ;-)
	} else {
	    passertMsg(0, "RecordMap::internal_allocRecord: something wrong!\n");
	}
    }

    passertMsg(bitmap, "RecordMap::allocRecord() no bitmap!?"
	       " i=%lu, j=%lu\n", i, j);

    // set the bits in the bitmap (note: reverse order)
    
    bitmap[j/32] = CPU_TO_TE32(TE32_TO_CPU(bitmap[j/32]) | (1 << (j%32)));

#ifndef KFS_SNAPSHOT
    globals->blkCache->markDirty(block);
#else
    uval32 dblkno;
    if ((dblkno = globals->blkCache->markDirty(block))) {
	data->setBitmap(i,dblkno);
	locked_markDirty();
    }
#endif
    globals->blkCache->freeBlock(block);

    // calculate the offset
    offset->id = i*RMAP_BITS_PAGE + j;

    if (offset->id >= RMAP_BITMAP_BLOCKS*RMAP_BITS_PAGE) {
	passertMsg(0, "i is %ld", i);
	lock.release();
	return -1;
    }

    // mark the pso as dirty!
    locked_markDirty();

    lock.release();
    return 0;
}

/*
 * freeRecord()
 *
 *   Deallocates the requested record.
 */
sval
RecordMap::freeRecord(ObjTokenID *offset)
{
    sval rc;

    // RACE! If we wait for this id to be removed from the hash during the
    // ServerObject destructor, the same id can be allocated to another object
    // and ObjToken::getObj() might return a stale (and incorrect) object.
    // So we do it here, before physically deallocating this id.
    KFSHashEntry<ServerObject*> *entry;
    uval ret = soHash.findLock(offset->id, &entry);
    if (ret) {
	soHash.removeUnlock(entry);
    }

    rc = internal_freeRecord(offset);

    // RACE! If we keep this object's ID unchanged, it will be removed
    // again in the ServerObject destructor. There is a very subtle
    // race where a new object allocates this ID (due to the fact that
    // we've just freed this entry), and adds itself to the
    // soHash. Then this object calls the ServerObject destructor
    // removing this id (which is owned by *another* object, and
    // should not be removed!). I think setting this to zero is the
    // "right thing to do", because this ID is not available neither
    // as an in-memory object, or a "physical" ORSMap entry.
    offset->id = 0;

    return 0;
}

/*
 * internal_freeRecord()
 *
 *   Recursive version which searches for the correct block to update,
 *   and dirty. Used internally in freeRecord(), above.
 */
SysStatus
RecordMap::internal_freeRecord(ObjTokenID *offset)
{
    if (offset->id == 0) {
	// Hold on... this is our internal PSO! We are probably shutting
	// down this RecordMap. Just return. (maybe check for shutdown?)
	return 0;
    }
    lock.acquire();
 
    // FIXME! FIXME! FIXME! There must be a better way to enforce ordering
    // of deletes. We need to have all the files represented in this RecordMap
    // deleted _before_ we begin deleting the RecordMap itself. Right now,
    // neither Linux or K42 enforce this kind of ordering.
    if (shutdown) {
	// Shutdown... too late to free Records, now! Go away!
	lock.release();
	return 0;
    }

    BlockCacheEntry *block;
    uval lblkno = offset->id / RMAP_BITS_PAGE,
	index = (offset->id % RMAP_BITS_PAGE)/32,
	shift = offset->id % 32;
    uval32 *bitmap;

    passertMsg(data->getBitmap(lblkno), "RecordMap::freeRecord() no block!\n");

    block = globals->blkCache->getBlockRead(data->getBitmap(lblkno));
    bitmap = (uval32 *)block->getData();

    uval32 bitmapValue = TE32_TO_CPU(bitmap[index]);
    // Check if this ObjTokenID has been allocated
    if (!(bitmapValue & (1 << shift))) {
	passertMsg(0, "RecordMap::internal_freeRecord() already free %llu\n",
		   offset->id);
    }

    // unset the proper bits in the bitmap
    bitmapValue &= ~(1 << shift);
    bitmap[index] = CPU_TO_TE32(bitmapValue);

    // There can be 4096/128 = 32 records in each block. Casting the
    // bitmap to (uval32 *) will give is the bitmap for the entire block.
    // WARNING!! If RMAP_RECORD_SIZE ever changes, this will fail!
    if (!bitmapValue) {
	pso->freeBlocks(offset->id*RMAP_RECORD_SIZE,
			offset->id*RMAP_RECORD_SIZE);
    } else {
	globals->blkCache->markDirty(block);
    }
    globals->blkCache->freeBlock(block);

    // mark the pso as dirty!
    locked_markDirty();

    lock.release();
    return 0;
}

/*
 * getRecord()
 *
 *   Returns a record of the given size from the given offset
 */
sval
RecordMap::getRecord(ObjTokenID *offset, char *rec)
{
    KFS_DPRINTF(DebugMask::RECORD_MAP, "RecordMap::getRecord()\n");

    uval lblkno;
    BlockCacheEntry *block;

    lock.acquire();

    if (offset->id == 0) {
	memcpy(rec, data->getPsoRecordPtr() + 4, KFS_RECORD_SIZE);
    } else {
	// calculate the location
	lblkno = offset->id / 32;
	
	block = pso->readBlockCache(lblkno, 1);
	
	// Don't forget to skip over the Type field
	memcpy(rec, block->getData() +
	       ((long)offset->id % 32) * RMAP_RECORD_SIZE + 4, KFS_RECORD_SIZE);
	
	pso->freeBlockCache(block);
    }

    lock.release();

    return 0;
}

/*
 * setRecord()
 *
 *   Sets a record of the given size at the given offset
 */
sval
RecordMap::setRecord(ObjTokenID *offset, char *rec)
{
    uval lblkno;
    BlockCacheEntry *block;

    lock.acquire();

    if (offset->id == 0) {
	memcpy(data->getPsoRecordPtr() + 4, rec, KFS_RECORD_SIZE);
    } else {
	// calculate the location
	lblkno = (long)offset->id / 32;

	block = pso->readBlockCache(lblkno, 1);

	memcpy(block->getData() + ((long)offset->id % 32)
	       * RMAP_RECORD_SIZE + 4, rec, KFS_RECORD_SIZE);

	pso->writeBlockCache(block, lblkno);
	pso->freeBlockCache(block);
    }

    lock.release();

    return 0;
}

/*
 * getRecordType()
 *
 *   Returns the type of a record at the given offset
 */
PsoType
RecordMap::getRecordType(ObjTokenID *offset)
{
    uval lblkno;
    BlockCacheEntry *block;
    PsoType type;

    lock.acquire();

    if (offset->id == 0) {
	type = (PsoType) data->getPsoRecordType();
    } else {
	// calculate the location
	lblkno = (long)offset->id / 32;

	block = pso->readBlockCache(lblkno, 1);

	uval32 *ptr = (uval32 *)(block->getData() +
				 ((long)offset->id % 32) * RMAP_RECORD_SIZE);
	type = (PsoType)TE32_TO_CPU(*ptr);

	pso->freeBlockCache(block);
    }

    lock.release();

    return type;
}

/*
 * setRecordType()
 *
 *   Sets the type of a record at the given offset
 */
sval
RecordMap::setRecordType(ObjTokenID *offset, PsoType type)
{
    uval lblkno;
    BlockCacheEntry *block;

    lock.acquire();

    if (offset->id == 0) {
	data->setPsoRecordType(type);
    } else {
	// calculate the location
	lblkno = (long)offset->id / 32;

	block = pso->readBlockCache(lblkno, 1);

	uval32 *ptr = (uval32 *)(block->getData() +
				 ((long)offset->id % 32) * RMAP_RECORD_SIZE);
	*ptr = CPU_TO_TE32(type);

	pso->writeBlockCache(block, lblkno);
	pso->freeBlockCache(block);
    }

    lock.release();

    return 0;
}

/*
 * flush()
 *
 *   Flushes all PSO and sub-object metadata to the disk.
 */
uval32
RecordMap::flush()
{
    // flush the underlying PSO
    pso->flush();

    // lock and flush
    lock.acquire();
    locked_flush();
    lock.release();

    return 0;
}

/*
 * locked_flush()
 *
 *   Does a flush assuming the PSO's lock is held.
 */
uval32
RecordMap::locked_flush()
{
    _ASSERT_HELD(lock);

    // flush this object if it is dirty
    if (flags & RMAP_DIRTY) {
	globals->blkCache->markDirty(dataBlock);
	flags &= ~RMAP_DIRTY;
    }
    return 0;
}

/*
 * markDirty()
 *
 *   Mark the PSO holding this logical block number as dirty.
 */
void
RecordMap::markDirty()
{
    // lock and dirty
    lock.acquire();
    locked_markDirty();
    lock.release();
}

/*
 * locked_markDirty()
 *
 *   Mark the PSO dirty assuming the lock is held.
 */
void
RecordMap::locked_markDirty()
{
    _ASSERT_HELD(lock);
    
    flags |= RMAP_DIRTY;
}

/*
 * locationAlloc()
 *
 *   Allocates space for a RecordMap and returns its location.
 */
/* static */ SysStatusUval
RecordMap::locationAlloc(KFSGlobals *globals)
{
    uval blkno;
    BlockCacheEntry *block, *bitmapBlock;
    struct RecordMapStruct *record;
    uval32 *bitmap;

    // allocate a block on the disk for the metadata
    blkno = globals->super->allocBlock();

    if (_FAILURE((sval)blkno)) {
	return blkno;
    }

    block = globals->blkCache->getBlock(blkno);
    record = (RecordMapStruct *)block->getData();

    // initialize the metadata
    memset(block->getData(), 0, OS_BLOCK_SIZE);

    // allocate the '0' record for our private use
    ((uval32 *)record)[0] = CPU_TO_TE32(OT_PRIM_UNIX);

    // Reserve it in the bitmap too
    record->setBitmap(0, globals->super->allocBlock());
    bitmapBlock = globals->blkCache->getBlock(record->getBitmap(0));
    memset(bitmapBlock->getData(), 0, OS_BLOCK_SIZE); // zero-out the block
    bitmap = (uval32*) bitmapBlock->getData();
    bitmap[0] = CPU_TO_TE32(TE32_TO_CPU(bitmap[0]) | 1); // allocate record 0 (zero)

    globals->blkCache->markDirty(bitmapBlock);
    globals->blkCache->freeBlock(bitmapBlock);

    globals->blkCache->markDirty(block);
    globals->blkCache->freeBlock(block);

    return blkno;
}

/*
 * unlink()
 */
/* virtual */ void
RecordMap::unlink()
{
    lock.acquire();

    shutdown = 1;

    // free the data in the PSO
    pso->unlink();

    // free the bitmap blocks
    uval i;
    for (i = 0; i < RMAP_BITMAP_BLOCKS; i++) {
	if (data->getBitmap(i)) {
	    globals->super->freeBlock(data->getBitmap(i));
	}
    }

    // free the metadata block
    globals->super->freeBlock(blkno);
    lock.release();
}

/* virtual */ ServerObject *
RecordMap::getObj(ObjTokenID *otokID)
{
    PsoType type;
    KFSHashEntry<ServerObject*> *entry;
    uval ret;
    ServerObject *obj;

    // make sure this is a valid object token
    if (otokID->id == 0) {
	//tassertMsg(0, "look\n");
        return NULL;
    }

    // check if we already have a pointer
    // locate the appropriate Server Object or create a new one
    ret = soHash.findAddLock(otokID->id, &entry);
    if (ret) {
	obj = entry->getData();
	tassertMsg(obj != NULL, "?");
    } else {
	if ((type = getRecordType(otokID)) <= 0) {
	    passertMsg(0, "ObjToken::getObj() Problem getting type id %lld, "
		       "type %ld\n", otokID->id, (sval) type);
	}

	// get a fresh ServerObject for this entry
	obj = globals->soAlloc->alloc(otokID, this, type);
	passertMsg(obj != NULL, "obj NULL?");
	entry->setData(obj);
    }

    entry->unlock();

    return obj;
}

/*
 *  Returns 1 if the object was found and removed, and 0 otherwise
 */
/* virtual */ SysStatus
RecordMap::removeObj(ObjTokenID *otokID)
{
    KFSHashEntry<ServerObject*> *entry;

    uval ret = soHash.findLock(otokID->id, &entry);
    if (ret) {
	soHash.removeUnlock(entry);
    }

    return ret;
}

/* virtual */SysStatus
RecordMap::sync()
{
    uval ret;
    uval64 key;
    KFSHashEntry<ServerObject*> *entry;
    ServerObject *sobj;

    soHash.acquireLock();
    ret = soHash.locked_getFirst(key, entry);
    while (ret != 0) {
	sobj = (ServerObject*) entry->getData();
	//err_printf("got server object %p\n", sobj);
	sobj->flush();
	ret = soHash.locked_getNext(key, entry);
    }
    soHash.releaseLock();

    return 0;
}
