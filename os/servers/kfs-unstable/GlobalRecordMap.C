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
 * $Id: GlobalRecordMap.C,v 1.2 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "GlobalRecordMap.H"
#include "KFSGlobals.H"
#include "PSOTypes.H"
#include "ObjToken.H"
#include "SuperBlock.H"
#include "FSFileKFS.H"
#include "PSOTypes.H"

/*
 * GlobalRecordMap()
 *
 *   Constructs a new GlobalRecordMap
 */
GlobalRecordMap::GlobalRecordMap(uval32 b, KFSGlobals *g)
    : globals(g), flags(RMAP_NONE), blkno(b), localHash()
{
    lock.init();
    dataBlock = g->blkCache->getBlockRead(blkno);
    // set the appropriate metadata
    data = (GlobalRecordMapStruct *)dataBlock->getData();
}

void
GlobalRecordMap::init()
{
    ObjTokenID psoID, dummyID = {0};
    localRecordMap = new LocalRecordMap(data->getLocalRecordMapBlk(),
					dummyID, globals);
    localRecordMap->init();
    psoID = data->getPSOID();
    pso = (PSOBase *)globals->soAlloc->alloc(&psoID, this, OT_PRIM_UNIX);
    passertMsg(pso, "GlobalRecordMap::init() Problem with PSO!\n");
    //    err_printf("RecordMaps:: Global 0x%p, Local 0x%p\n", this, localRecordMap);
}

GlobalRecordMap::~GlobalRecordMap()
{
    localHash.removeAll();
    localHash.destroy();
    delete fsfilePSO;
    delete pso;
    delete localRecordMap;

    if (dataBlock) {
	globals->blkCache->freeBlock(dataBlock);
    }
}

/*
 * allocLocalRecordMap()
 */
SysStatus
GlobalRecordMap::allocLocalRecordMap(uval32 *offset)
{
    sval rc;

    rc = internal_allocLocalRecordMap(offset);

    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::RECORD_MAP,
		    "GlobalRecordMap::allocRecords() Problem allocating"
		    " record");
	// release the entries we've allocated
	freeLocalRecordMap(*offset);
	return rc;
    }

    KFS_DPRINTF(DebugMask::RECORD_MAP, "AllocRecord got id %u\n", *offset);

    //    err_printf("GlobalRecordMap(0x%p)::allocLocal() = got id %u\n", this, *offset);
    //    *offset = globalIDToObjTokenID(*offset);
    //    err_printf("GlobalRecordMap(0x%p)::allocLocal() = %u\n", this, *offset);
    return rc;
}

/*
 * internal_allocLocalRecordMap()
 *
 *   Searches for a free record linearly through the bitvector.
 *   I think we could do better... 
 *   Used internally in allocLocalRecordMap().
 */
SysStatus
GlobalRecordMap::internal_allocLocalRecordMap(uval32 *offset)
{
    BlockCacheEntry *block = NULL;
    uval32 *bitmap = NULL;
    uval i, j = RMAP_BITS_PAGE;

    lock.acquire();

    for (i = 0; data->getBitmap(i) && i < RMAP_GLOBAL_BITMAP_BLOCKS; i++) {
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
	if (i < RMAP_GLOBAL_BITMAP_BLOCKS && !data->getBitmap(i)) {
	    data->setBitmap(i, globals->super->allocBlock());
	    block = globals->blkCache->getBlock(data->getBitmap(i));
	    bitmap = (uval32*) block->getData();
	    memset(bitmap, 0, OS_BLOCK_SIZE); // zero-out the block
	    j = 0; // it's a new block! j = 0 is _certainly_ free ;-)
	} else {
	    passertMsg(0, "GlobalRecordMap::internal_allocRecord: something wrong!\n");
	}
    }

    passertMsg(bitmap, "GlobalRecordMap::allocRecord() no bitmap!?"
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
    *offset = i*RMAP_BITS_PAGE + j;

    if (*offset >= RMAP_GLOBAL_BITMAP_BLOCKS*RMAP_BITS_PAGE) {
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
SysStatus
GlobalRecordMap::freeLocalRecordMap(uval32 offset)
{
    //   uval32 internalOffset;
    //    internalOffset = objTokenIDToGlobalID(offset);
    //    return internal_freeLocalRecordMap(internalOffset);
    return internal_freeLocalRecordMap(offset);
}

/*
 * internal_freeRecord()
 *
 *   Recursive version which searches for the correct block to update,
 *   and dirty. Used internally in freeRecord(), above.
 */
SysStatus
GlobalRecordMap::internal_freeLocalRecordMap(uval32 offset)
{
    lock.acquire();

    BlockCacheEntry *block;
    uval lblkno = offset / RMAP_BITS_PAGE,
	index = (offset % RMAP_BITS_PAGE)/32,
	shift = offset % 32;
    uval32 *bitmap;

    passertMsg(data->getBitmap(lblkno), "GlobalRecordMap::freeRecord() no block!\n");

    block = globals->blkCache->getBlockRead(data->getBitmap(lblkno));
    bitmap = (uval32 *)block->getData();

    uval32 bitmapValue = TE32_TO_CPU(bitmap[index]);
    // Check if this ObjTokenID has been allocated
    if (!(bitmapValue & (1 << shift))) {
	passertMsg(0, "GlobalRecordMap::internal_freeRecord() already free "
		   "%u\n", offset);
    }

    // unset the proper bits in the bitmap
    bitmapValue &= ~(1 << shift);
    bitmap[index] = CPU_TO_TE32(bitmapValue);

    // FIXME!!! Check if this entire block is free, so we can free it
    globals->blkCache->markDirty(block);
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
SysStatusUval
GlobalRecordMap::getLocalRecordMap(uval offset)
{
    KFS_DPRINTF(DebugMask::RECORD_MAP, "GlobalRecordMap::getRecord()\n");

    uval lblkno;
    uval32 recordMapBlk;
    BlockCacheEntry *block;

    lock.acquire();

    // calculate the location
    lblkno = offset / RMAP_WORDS_PAGE;

    block = pso->readBlockCache(lblkno, 1);
	
    recordMapBlk = TE32_TO_CPU(((uval *)block->getData())[offset % RMAP_WORDS_PAGE]);

    pso->freeBlockCache(block);

    lock.release();

    return 0;
}

/*
 * setRecord()
 *
 */
SysStatus
GlobalRecordMap::setLocalRecordMap(uval offset, uval32 recordMapBlk)
{
    uval lblkno;
    BlockCacheEntry *block;

    lock.acquire();

    // calculate the location
    lblkno = offset / RMAP_WORDS_PAGE;
	
    block = pso->readBlockCache(lblkno, 1);
	
    ((uval *)block->getData())[offset % RMAP_WORDS_PAGE] = CPU_TO_TE32(recordMapBlk);

    pso->writeBlockCache(block, lblkno);
    pso->freeBlockCache(block);

    lock.release();

    return 0;
}

void
GlobalRecordMap::registerLocalRecordMap(ObjTokenID *otokID, RecordMapBase *local)
{
    //    err_printf("GlobalRecordMap(0x%p)::register(%u, 0x%p)\n",
    //	       this, id.id, local);
    KFSHashEntry<RecordMapBase *> *entry;
    //    uval ret = localHash.findAddLock(objTokenIDToGlobalID(id.id), &entry);
    uval ret = localHash.findAddLock(otokID->id, &entry);
    passertMsg(!ret, "GlobalRecordMap::registerLocal() RecordMap already "
	       "registered for id %llu !\n", otokID->id);
    entry->setData(local);
    entry->unlock();
}

void
GlobalRecordMap::unregisterLocalRecordMap(ObjTokenID *otokID)
{
    //    err_printf("GlobalRecordMap(0x%p)::unregister(%u)\n",
    //	       this, id.id);
    KFSHashEntry<RecordMapBase *> *entry;
    //    uval ret = localHash.findLock(objTokenIDToGlobalID(id.id), &entry);
    uval ret = localHash.findLock(otokID->id, &entry);
    passertMsg(ret, "GlobalRecordMap::unregisterLocal() RecordMap not "
	       "registered for id %llu !\n", otokID->id);
    localHash.removeUnlock(entry);
}

RecordMapBase *
GlobalRecordMap::getLocalRecordMap(ObjTokenID *otokID)
{
    RecordMapBase *local;
    KFSHashEntry<RecordMapBase *> *entry;
    uval32 globalID;

    globalID = objTokenIDToGlobalID(otokID->id);
    //    globalID = id.id;

    //    err_printf("GlobalRecordMap(0x%p)::getLocal(%u) = %u\n", this, id.id, globalID);

    // Check if this particular ID belongs to this Global RecordMap
    if (!globalID) {
	return this;
    }
    uval ret = localHash.findLock(globalID, &entry);
    if (ret) {
	local = entry->getData();
	entry->unlock();
	//	err_printf("GlobalRecordMap::getLocal() return 0x%p\n", local);
	return local;
    }
    passertMsg(0, "GlobalRecordMap::getLocal() RecordMap not registered for "
	       "this ID! We have a problem... :-(\n");
    return NULL;
}

/*
 * flush()
 *
 *   Flushes all PSO and sub-object metadata to the disk.
 */
uval32
GlobalRecordMap::flush()
{
    // flush the underlying PSO
    pso->flush();
    // and the underlying LocalRecordMap
    localRecordMap->flush();

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
GlobalRecordMap::locked_flush()
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
GlobalRecordMap::markDirty()
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
GlobalRecordMap::locked_markDirty()
{
    _ASSERT_HELD(lock);
    
    flags |= RMAP_DIRTY;
}

/*
 * locationAlloc()
 *
 *   Allocates space for a GlobalRecordMap and returns its location.
 */
/* static */ SysStatusUval
GlobalRecordMap::locationAlloc(KFSGlobals *globals)
{
    uval blkno;
    BlockCacheEntry *block, *bitmapBlock;
    struct GlobalRecordMapStruct *record;
    uval32 *bitmap;
    ObjTokenID otokid;

    // allocate a block on the disk for the metadata
    blkno = globals->super->allocBlock();

    if (_FAILURE((sval)blkno)) {
	return blkno;
    }

    block = globals->blkCache->getBlock(blkno);
    record = (GlobalRecordMapStruct *)block->getData();

    // initialize the metadata
    memset(block->getData(), 0, OS_BLOCK_SIZE);

    // allocate internal LocalRecordMap
    record->setLocalRecordMapBlk(LocalRecordMap::locationAlloc(globals));

    // now allocate PSO for underlying data in the local recordmap
    ObjTokenID dummyID = {0};
    LocalRecordMap *lrm = new LocalRecordMap(record->getLocalRecordMapBlk(),
					     dummyID, globals);
    lrm->init();
    lrm->allocRecord(OT_PRIM_UNIX, &otokid);
    record->setPSOID(otokid);
    lrm->flush();
    delete lrm;

    // Reserve the 0-th LocalRecordMap in the bitmap
    record->setBitmap(0, globals->super->allocBlock());
    bitmapBlock = globals->blkCache->getBlock(record->getBitmap(0));
    memset(bitmapBlock->getData(), 0, OS_BLOCK_SIZE); // zero-out the block
    bitmap = (uval32*) bitmapBlock->getData();
    // allocate record 0 (zero)
    bitmap[0] = CPU_TO_TE32(TE32_TO_CPU(bitmap[0]) | 1);

    globals->blkCache->markDirty(bitmapBlock);
    globals->blkCache->freeBlock(bitmapBlock);

    globals->blkCache->markDirty(block);
    globals->blkCache->freeBlock(block);

    return blkno;
}

/* virtual */
void
GlobalRecordMap::unlink()
{
    // free local recordmap
    localRecordMap->unlink();

    // free bitmap blocks
    uval i;
    for (i = 0; i < RMAP_BITMAP_BLOCKS; i++) {
	if (data->getBitmap(i)) {
	    globals->super->freeBlock(data->getBitmap(i));
	}
    }

    // free the metadata block
    globals->super->freeBlock(blkno);
}
