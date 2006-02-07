/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: PSOSmall.C,v 1.3 2004/05/05 19:57:59 lbsoares Exp $
 **************************************************************************/

#include <kfsIncs.H>

#include "PSOSmall.H"
#include "SuperBlock.H"
#include "FSFileKFS.H"

/*
 * PSOSmall()
 *
 *   Constructs the PSOSmall class given its PSORecordMap record
 */
PSOSmall::PSOSmall(ObjTokenID *otokID, RecordMapBase *r, PSOBase *p,
		   KFSGlobals *g) : PSOBase(otokID, r, g), llpso(p)
{
    // read the PSO record
    getRecordMap()->getRecord(&id, recordBuf);

    record = (PSOSmallRecordStruct *)recordBuf;
}

PSOSmall::~PSOSmall()
{
    KFS_DPRINTF(DebugMask::PSO_SMALL,
		"PSOSmall::~PSOSmall() id=%llu IN\n", id.id);
    KFS_DPRINTF(DebugMask::PSO_SMALL,
		"PSOSmall::~PSOSmall() id=%llu OUT\n", id.id);
}

/*
 * blockToPath()
 *
 *   Given a certain logical block number, returns the depth of indirection
 *   necessary to reach the block number. Additionally, fills in 'offsets'
 *   array. 'offsets' indicates the offset of the pointer we want to have in
 *   each level of indirection.
 *
 *   Code taken from ext2/inode.c, and minix/itree_v2.c
 */
SysStatusUval
PSOSmall::blockToPath(uval32 lblkno, uval32 offsets[4])
{
    uval32 ptrs = PSO_SMALL_DIRECT_MAXBLK;
    uval32 ptrs_bits = PSO_SMALL_DIRECT_BITS;
    const uval32 direct_blocks = PSO_SMALL_RECORD_MAXBLK,
	indirect_blocks = ptrs,
	double_blocks = (1 << (ptrs_bits * 2));
    uval8 n = 0;

    if (lblkno < direct_blocks) {
	offsets[n++] = lblkno;
    } else if ( (lblkno -= direct_blocks) < indirect_blocks) {
	offsets[n++] = PSO_SMALL_RECORD_MAXBLK;
	offsets[n++] = lblkno;
    } else if ((lblkno -= indirect_blocks) < double_blocks) {
	offsets[n++] = PSO_SMALL_RECORD_MAXBLK + 1;
	offsets[n++] = lblkno >> ptrs_bits;
	offsets[n++] = lblkno & (ptrs - 1);
    } else if (((lblkno -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
	offsets[n++] = PSO_SMALL_RECORD_MAXBLK + 2;
	offsets[n++] = lblkno >> (ptrs_bits * 2);
	offsets[n++] = (lblkno >> ptrs_bits) & (ptrs - 1);
	offsets[n++] = lblkno & (ptrs - 1);
    } else {
	passertMsg(0, "PSOSmall::blockToPath() block too big %u\n", lblkno);
    }
    return n;
}

/*
 * getDblk()
 *
 *   Given a certain logical block number, returns the physical block number
 *   it's mapped to.
 *   The 'create' flag determines if new blocks should be allocated or just 
 *   return 0 in the case the mapping is previously not present.
 */
uval32
PSOSmall::getDblk(uval32 lblkno, uval8 create)
{
    uval8 depth;
    uval32 offsets[4], *ptr;
    uval32 *dblk = record->dBlk, blkno = 0;
    BlockCacheEntry *block = NULL;

    depth = blockToPath(lblkno, offsets);
    ptr = (uval32 *)offsets;

    while (depth--) {
	if (!TE32_TO_CPU(dblk[*ptr])) {
	    if (!create) {
		if (block) {
		    globals->blkCache->freeBlock(block);
		}
		return 0;
	    }
	    dblk[*ptr] = CPU_TO_TE32(globals->super->allocBlock());
	    // no need to initialize leaf-nodes
	    if (depth) {
		BlockCacheEntry *newEntry = globals->blkCache->
		    getBlock(TE32_TO_CPU(dblk[*ptr]));
		memset(newEntry->getData(), 0, OS_BLOCK_SIZE);
		globals->blkCache->markDirty(newEntry);
		globals->blkCache->freeBlock(newEntry);
	    }

	    // mark block dirty
	    if (block) {
		globals->blkCache->markDirty(block);
	    } else {
		markDirty();
	    }

	}
	
	// record number before we free the block
	blkno = TE32_TO_CPU(dblk[*ptr]);

	// release previous block
	if (block) {
	    globals->blkCache->freeBlock(block);
	}

	if (depth) {
	    block = globals->blkCache->getBlockRead(blkno);
	    dblk = (uval32 *)block->getData();
	    ptr++; // move to next index in 'offsets'
	}
    }

    return blkno;
}

/*
 * freeDblk()
 * 
 *    Given a certain logical number, frees the block, zeroes-out its entry
 *    in the correct meta-data block, and possibly frees unused meta-data 
 *    block.
 */
void
PSOSmall::freeDblk(uval32 lblkno)
{
    uval depth, n;
    uval32 offsets[4] = {0,0,0,0};
    uval32 *dblk = record->dBlk;
    BlockCacheEntry *blocks[4] = {0,0,0,0};

    depth = blockToPath(lblkno, offsets);

    // first, try to get the buffers
    for (n = 0; n < depth; n++) {
	if (n) {
	    blocks[n] = globals->blkCache->getBlockRead(TE32_TO_CPU(dblk[offsets[n-1]]));
	    dblk = (uval32 *)blocks[n]->getData();
	}

	if (!TE32_TO_CPU(dblk[offsets[n]])) {
	    // could not get to the block.. someone in the path is already free
	    goto out;
	}
    }

    // sweep 'offsets' in reverse order, freeing as necessary
    while (depth--) {
	if (!TE32_TO_CPU(dblk[offsets[depth]])) {
	    // do nothing... already free
	    goto out;
	}

	// free it in the SuperBlock
	globals->super->freeBlock(TE32_TO_CPU(dblk[offsets[depth]]));

	// clean old number and mark buffer dirty
	dblk[offsets[depth]] = CPU_TO_TE32(0); // silly translation

	if (depth) {
	    uval i;
	    // Check if it this was the last block here
	    for (i = 0; i < PSO_SMALL_DIRECT_MAXBLK; i++) {
		if (TE32_TO_CPU(dblk[i])) {
		    break;
		}
	    }
	    
	    // Block is being used, don't continue!
	    if (i != PSO_SMALL_DIRECT_MAXBLK) {
		globals->blkCache->markDirty(blocks[depth]);
		goto out;
	    }
	    // unused block, mark it clean (no need to write it out)
	    blocks[depth]->markClean();
	}
	else {
	    markDirty();
	}

	if (depth > 1) {
	    dblk = (uval32 *)blocks[depth-1]->getData();
	} else {
	    dblk = record->dBlk;	    
	}
    }

  out:
    for (n = 0; n < 4; n++) {
	if (blocks[n]) {
	    globals->blkCache->freeBlock(blocks[n]);
	}
    }

    return;
}

/*
 * truncate()
 * 
 *    Given a certain logical number, deletes all blocks greater than
 *    or equal to it. It frees all blocks, zeroing out entries in the
 *    meta-data blocks, and possibly freeing unused meta-data blocks
 *
 *    This is used instead of freeDblk() for truncating the file, as calling
 *    freeDblk() multiple times resulted in horrible performance.
 *
 *  FIXME: If we ever need to create "holes" in files, instead of just
 *         truncating up to the very end, this function could be changed to
 *         work with other ranges besides [lblkno-inf.[. Just change the
 *         main "while" conditional.
 */
void
PSOSmall::truncate(uval32 lblkno)
{
    uval8 depth;
    uval32 i, offsets[4] = {0,0,0,0};
    uval32 *dblk = record->dBlk;
    BlockCacheEntry *blocks[4] = {0,0,0,0};

    depth = blockToPath(lblkno, offsets);

    // first, try to get the buffers
    for (i = 0; i < depth; i++) {
	if (!TE32_TO_CPU(dblk[offsets[i]])) {
	    // could not get to the block.. someone in the path is already free
	    break;
	}

	if (depth > 1) {
	    blocks[i+1] = globals->blkCache->getBlockRead(TE32_TO_CPU(dblk[offsets[i]]));
	    dblk = (uval32 *)blocks[i+1]->getData();
	}
    }

    // depth should range between [0-3]
    depth--;
    dblk = record->dBlk;

    // sweep 'offsets', freeing as necessary
    while (offsets[0] < PSO_SMALL_RECORD_MAXBLK + 3) {
	
	// are we at the end of a block?
	if (depth && offsets[depth] == PSO_SMALL_DIRECT_MAXBLK) {
	    // End of the block. Check if it can be freed, and "step back"
	    // to parent-node.

	    KFS_DPRINTF(DebugMask::PSO_SMALL_TRUNCATE,
			"PSOSmall::truncate() end depth=%u, offset=%u\n",
			depth, offsets[depth]);

	    // check if this block is still useful
	    for (i = 0; i < PSO_SMALL_DIRECT_MAXBLK; i++) {
		if (TE32_TO_CPU(dblk[i])) {
		    break;
		}
	    }

	    // Unused block, mark it clean (no need to write it out)
	    if (i == PSO_SMALL_DIRECT_MAXBLK && blocks[depth]) {
		blocks[depth]->markClean();
	    }

	    if (blocks[depth]) {
		globals->blkCache->freeBlock(blocks[depth]); // release the block
		blocks[depth] = NULL;
	    }
	    offsets[depth] = 0; // start new block from the top
	    depth--;            // step back
	    if (depth) {
		dblk = (uval32 *)blocks[depth]->getData();
	    }
	    else {
		dblk = record->dBlk;
	    }

	    // tell parent, that the child-node is no more
	    if (i == PSO_SMALL_DIRECT_MAXBLK && dblk[offsets[depth]]) {
		// free the block
		globals->super->freeBlock(TE32_TO_CPU(dblk[offsets[depth]]));
		// clean old number and mark buffer dirty
		dblk[offsets[depth]] = CPU_TO_TE32(0); // very silly translation
		if (depth) {
		    globals->blkCache->markDirty(blocks[depth]);
		}
		else {
		    markDirty();
		}
	    }

	    offsets[depth]++;   // go to next indirect pointer
	} else {
	    // in the middle of a node
	    // are in a leaf or internal node?
	    if (offsets[0] < PSO_SMALL_RECORD_MAXBLK ||
		offsets[0] + 1 - PSO_SMALL_RECORD_MAXBLK == depth) {
		// LEAF NODE
		KFS_DPRINTF(DebugMask::PSO_SMALL_TRUNCATE,
			    "PSOSmall::truncate() LEAF depth=%u, offset=%u\n",
			    depth, offsets[depth]);

		if (TE32_TO_CPU(dblk[offsets[depth]])) {
		    // free the block
		    globals->super->freeBlock(
			    TE32_TO_CPU(dblk[offsets[depth]]));
		    // clean old number and mark buffer dirty
		    // silly endian translation, but better to have it here in
		    // case we change 0 to a value where endian is an issue
		    dblk[offsets[depth]] = TE32_TO_CPU(0);
		    if (depth) {
			globals->blkCache->markDirty(blocks[depth]);
		    }
		    else {
			markDirty();
		    }
		}
		offsets[depth]++;   // go to next indirect pointer
	    }
	    else {
		// INTERNAL NODE
		KFS_DPRINTF(DebugMask::PSO_SMALL_TRUNCATE,
			    "PSOSmall::truncate() INTERNAL depth=%u, "
			    "offset=%u\n", depth, offsets[depth]);

		// step forward to a child block
		if (TE32_TO_CPU(dblk[offsets[depth]])) {
		    // get new block, and update dblk
		    depth++;
		    blocks[depth] = globals->blkCache->getBlockRead
			(TE32_TO_CPU(dblk[offsets[depth-1]]));
		    dblk = (uval32 *)blocks[depth]->getData();
		    offsets[depth] = 0; // start new block from the top
		}
		else {
		    offsets[depth]++;   // go to next indirect pointer
		}
	    }
	}
    }

    // release any held blocks
    for (i = 0; i < 4; i++) {
	if (blocks[i]) {
	    err_printf("block not free! %u\n", i);
	    globals->blkCache->freeBlock(blocks[i]);
	}
    }

    return;
}


/*
 * readBlock()
 *
 *   Reads the logical block specified from the disk and returns it.
 */
sval
PSOSmall::readBlock(uval32 lblkno, char *buffer, uval local,
		    uval isPhysAddr /* = 0 */)
{
    // FIXME: this code is essentially the same we hae in
    //        PSOBasicRW::readBlock. We could factor it out
    //        on PSOBase, but them PSOBase wouldn't be a pure
    //        interface anymore ...

    sval rc;
    uval32 dblkno;

    // lock this PSO
    lock.acquire();

    dblkno = getDblk(lblkno, 0);

    // If got disk block zero, page is not on disk. It's a new page.
    if (!dblkno) {
	if (isPhysAddr) {
	    lock.release();
#ifndef KFS_TOOLS
	    return FSFileKFS::PageNotFound();
#endif // #ifndef KFS_TOOLS
	} else {
	    memset(buffer, 0, OS_BLOCK_SIZE);
	    lock.release();
	    return 0;
	}
    }

    KFS_DPRINTF(DebugMask::PSO_SMALL_RW,
		"PSOSmall::readBlock: reading block %u %u\n",
		dblkno, lblkno);

    // Else disk block not zero. Issue read block to disk object.
    // FIXME: change disk routines to handle correct token
    rc = llpso->readBlock(dblkno, buffer, local);

    // FIXME dilma : we should just propagate this error, but the code
    //               doesn't check for errors everywhere, so to make
    //               debugging easier, let's catch problems here
    passertMsg(_SUCCESS(rc), "disk block read failed\n");
    
    lock.release();
    return rc;
}

/*
 * readBlockPhys()
 *
 *   Reads the logical block specified from the disk and returns it.
 *   The difference from readBlock() is that if the block is not found,
 *   an error is returned instead of 0 filling the buffer
 */
sval
PSOSmall::readBlockPhys(uval32 lblkno, char *buffer, uval local)
{
    sval rc;
    uval32 dblkno;

    // lock this PSO
    lock.acquire();

    dblkno = getDblk(lblkno, 0);

    // If got disk block zero, page is not on disk. It's a new page.
    if (!dblkno) {
	passertMsg(0, "for debugging");
	lock.release();
	// FIXME: return proper SERROR!
	return -1;
    }

    KFS_DPRINTF(DebugMask::PSO_SMALL_RW,
		"PSOSmall::readBlockPhys: reading block %u %u\n",
		dblkno, lblkno);

    // Else disk block not zero. Issue read block to disk object.
    // FIXME: change disk routines to handle correct token
    rc = llpso->readBlock(dblkno, buffer, local);

    // FIXME dilma : we should just propagate this error, but the code
    //               doesn't check for errors everywhere, so to make
    //               debugging easier, let's catch problems here
    passertMsg(_SUCCESS(rc), "disk block read failed\n");
    
    lock.release();
    return rc;
}

// block-cache integration stuff
BlockCacheEntry *
PSOSmall::readBlockCache(uval32 b, uval local) 
{
    BlockCacheEntry* block;
    uval32 dblkno;

    lock.acquire();
    dblkno = getDblk(b, 0);

    KFS_DPRINTF(DebugMask::PSO_SMALL_RW,
		"PSOSmall::readBlockCache: reading block %u %u\n", dblkno, b);

    block = llpso->readBlockCache(dblkno, local);
    lock.release();
    return block;
}

/*
 * writeBlock()
 *
 *   Writes the logical block specified to the disk.
 */
SysStatus
PSOSmall::writeBlock(uval32 lblkno, char *buffer, uval local)
{
    sval rc;
    uval32 dblkno;

    // lock this PSO
    lock.acquire();

    dblkno = getDblk(lblkno, 1);

    // If there isn't a block already there, allocate one
    if (!dblkno) {
	tassertMsg(0, "PSOSmall::writeBlock() This shouldn't happen, no block!"
		   " %u %u\n", dblkno, lblkno);
	memset(buffer, 0, OS_BLOCK_SIZE);
	lock.release();
	return -1;
    }

    KFS_DPRINTF(DebugMask::PSO_SMALL_RW,
		"PSOSmall::writeBlock: writing block %u %u\n", dblkno, lblkno);
    passertMsg(dblkno, "PSOSmall::writeBlock() bad disk block for"
	       " writing d=%u, l=%u, this=0x%p\n", dblkno, lblkno, this);

    // FIXME: change disk routines to handle correct token
    rc = llpso->writeBlock(dblkno, buffer, local);

    lock.release();
    return rc;
}

// block-cache integration
SysStatus
PSOSmall::writeBlockCache(BlockCacheEntry *block, uval32 lblkno)
{
    uval32 dblkno;

    // lock this PSO
    lock.acquire();

    if (!block->getBlockNumber()) {
	dblkno = getDblk(lblkno, 1);
	
	// If there isn't a block already there, allocate one
	if (!dblkno) {
	    tassertMsg(0, "PSOSmall::writeBlockCache() This shouldn't happen, "
		       "no block! %u %u\n", dblkno, lblkno);
	    lock.release();
	    return -1;
	}

	KFS_DPRINTF(DebugMask::PSO_SMALL_RW,
		    "PSOSmall::writeBlockCache: writing block %u %u\n",
		    dblkno, lblkno);
	passertMsg(dblkno, "PSOSmall::writeBlock() bad disk block for"
		   " writing d=%u, l=%u, this=0x%p\n",
		   dblkno, lblkno, this);

	globals->blkCache->updateBlockNumber(block, dblkno);
    }

    llpso->writeBlockCache(block, lblkno);

    lock.release();
    return 0;
}

/*
 * freeBlocks()
 *
 *   Deallocates the blocks associated with file offsets spanning [from, to].
 *
 * note: there used to be code in this function to accept MAX_INT into
 *       the to variable and then use that as an indicator to truncate
 *       the file, thus avoiding the recursion currently in this
 *       function.
 */
sval
PSOSmall::freeBlocks(uval32 fromLBlk, uval32 toLBlk)
{
    sval rc;

    // lock & deallocate
    lock.acquire();
    rc = locked_freeBlocks(fromLBlk, toLBlk);
    lock.release();

    return rc;
}

/*
 * locked_freeBlocks()
 *
 *   Deallocates blocks assuming that the lock for this PSO is already held.
 */
sval
PSOSmall::locked_freeBlocks(uval32 fromLBlk, uval32 toLBlk)
{
    _ASSERT_HELD(lock);

    // beginning larger than maximum file size?
    if (fromLBlk >= PSO_SMALL_MAXBLK) {
	return 0;
    }

    // ending larger than maximum file size?
    if (toLBlk >= PSO_SMALL_MAXBLK) {
	toLBlk = PSO_SMALL_MAXBLK-1;
    }

    // check if a truncate() was requested
    if (toLBlk == PSO_SMALL_MAXBLK-1) {
	KFS_DPRINTF(DebugMask::PSO_SMALL,
		    "PSOSMall::freeBlocks() truncating\n");
	truncate(fromLBlk);
    }
    else {
	KFS_DPRINTF(DebugMask::PSO_SMALL,
		    "PSOSMall::freeBlocks() one-by-one! from %u to %u\n",
		    fromLBlk, toLBlk);
	for (sval i = toLBlk; i >= (sval32)fromLBlk; i--) {
	    freeDblk(i);
	}
    }

    return 0;
}

/*
 * unlink()
 *
 *   Destroys this object and all sub-objects and free's their data.
 */
void
PSOSmall::unlink()
{
    // lock & unlink
    lock.acquire();
    locked_unlink();
    lock.release();
}

/*
 * locked_unlink()
 *
 *   Does an unlink of the PSO assuming that the lock is already held.
 */
void
PSOSmall::locked_unlink()
{
    _ASSERT_HELD(lock);

    KFS_DPRINTF(DebugMask::PSO_SMALL,
		"PSOSmall::unlink from %u to MAX=%lu\n", 0,
		PSO_SMALL_MAXBLK * OS_BLOCK_SIZE);
    // free all the blocks in this object
    locked_freeBlocks(0, (uval32)PSO_SMALL_MAXBLK);

    // don't try to flush anything anymore
    markClean();

    // free the RecordMap entry
    getRecordMap()->freeRecord(&id);
}

/*
 * flush()
 *
 *   Flushes all of this object's metadata to disk.  Because there is
 *   a pointer to the subobject in this object's metadata, it is
 *   important to flush it first.
 */
void
PSOSmall::flush()
{
    // lock this PSO
    lock.acquire();

    // do the actual flush
    locked_flush();

    // unlock this PSO
    lock.release();
}

/*
 * locked_flush()
 *
 *   Does a flush, assuming that the locks for this PSO are already held.
 */
void
PSOSmall::locked_flush()
{
    _ASSERT_HELD(lock);

    // flush this object if it is dirty
    if (isDirty()) {
	markClean();
	// write back meta-data block number to record entry
	getRecordMap()->setRecord(&id, recordBuf);
    }
}

/*
 * special()
 *
 *   Does nothing.  Must be declared because of virtual tag in class PSOBase.
 */
sval
PSOSmall::special(sval operation, void *buf)
{
    tassertMsg(0, "PSOSmallO::special() called\n");
    return -1;
}

/*
 * clone()
 *
 *   Creates a new PSOSmall from the given "object related state" map entry.
 */
/* virtual */ ServerObject *
PSOSmall::clone(ObjTokenID *otokID, RecordMapBase *r)
{
    KFS_DPRINTF(DebugMask::PSO_SMALL, "PSOSmall::clone\n");
    ServerObject *pso = new PSOSmall(otokID, r, llpso, globals);

    return pso;
}

/*
 * locationAlloc()
 *
 *   Allocates space for a PSOSmall and returns its location.
 */
SysStatus
PSOSmall::locationAlloc(ObjTokenID *otokID, RecordMapBase *recordMap)
{
    memset(recordBuf, 0, KFS_RECORD_SIZE);
    recordMap->setRecord(otokID, recordBuf);

    return 0;
}

