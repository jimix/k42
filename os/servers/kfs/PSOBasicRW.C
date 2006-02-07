/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: PSOBasicRW.C,v 1.68 2005/04/15 17:39:38 dilma Exp $
 *****************************************************************************/

#include <kfsIncs.H>

#include "SuperBlock.H"
#include "ObjToken.H"
#include "PSOBase.H"
#include "KFSDebug.H"

#include "PSOBasicRW.H"
#include "PSOTypes.H"
#include "KFSGlobals.H"
#include "RecordMap.H"

#ifndef KFS_TOOLS
#include "FSFileKFS.H"
#include "ServerFileBlockKFS.H"
#endif // #ifndef KFS_TOOLS

/*
 * PSOBasicRW()
 *
 *   Constructs the PSOBasicRW class given its "object related state"
 */
PSOBasicRW::PSOBasicRW(ObjTokenID otokID, FSFileKFS *f, PSOBase *p,
		       KFSGlobals *g) : PSOBase(otokID, f, g), llpso(p),
					flags(PSO_BASICRW_NONE), subObj(g)
{
    char recordBuf[KFS_RECORD_SIZE];

    // read the PSO record
    g->recordMap->getRecord(id, recordBuf);

    // the first uval32 from the buffer contains the meta-data block number
    blkno = *(uval32 *)recordBuf;

    KFS_DPRINTF(DebugMask::PSO_BASIC_RW, "PSOBasicRW() blkno=%u\n", blkno);
    data = g->blkCache->getBlockRead(blkno);

    PSOBasicRWBlock *ptr = (PSOBasicRWBlock *)data->getData();

    // set the appropriate metadata
    // Notice that subObjID is a pointer to the data buffer, therefore
    // if we write to it, the buffer itself will be written to
    subObjID = &ptr->id;
    subObj.setID(*subObjID);
    dblk = ptr->dBlk;
}

PSOBasicRW::~PSOBasicRW()
{
    KFS_DPRINTF(DebugMask::PSO_BASIC_RW,
		"PSOBasicRW::~PSOBasicRW() id=%u IN\n", id.id);

    PSOBase *pso = (PSOBase *)subObj.gobj();
    if (pso) {
	delete pso;
    }

    if (data) {
	globals->blkCache->freeBlock(data);
    }

    KFS_DPRINTF(DebugMask::PSO_BASIC_RW,
		"PSOBasicRW::~PSOBasicRW() id=%u OUT\n", id.id);
}

/*
 * getDblk()
 *
 *   Determines the mapping for this logical block number. If the mapping is
 *   non-existant, then allocate the block, if create is 1.
 *
 *   returns the physical block number for the logical number.
 */
/* virtual */ uval32
PSOBasicRW::getDblk(uval32 lblkno, uval8 create/*=0*/)
{
    _ASSERT_HELD(lock);

    sval rc;
    uval32 dblkno;
    PSOBasicRW *pso;

    // If can handle it myself
    if (lblkno < RW_MAXBLK) {
	dblkno = dblk[lblkno];

        // If there isn't a block already there, allocate one
        if (dblkno == 0 && create) {
	    rc = dblk[lblkno] = dblkno = globals->super->allocBlock();
	    if (_FAILURE(rc)) {
		dblk[lblkno] = 0;
		return 0;
	    }
	    locked_markDirty(lblkno);
        }
	return dblkno;
    }

    /* pass on to sub-object */
    KFS_DPRINTF(DebugMask::PSO_BASIC_RW,
		"PSOBasicRWObj::getDblk: forwarding subobj\n");

    pso = (PSOBasicRW *)subObj.getObj(fsfile);
    if (pso == NULL && create) {
	// need to get a new PSO to extend the file!  Since allocRecord writes
	// the entry to subObjID it will be written out to the data buffer,
	// because subObjID is a pointer to the correct place in the data buffer.
        rc = globals->recordMap->allocRecord(OT_BASIC_RW, subObjID);
	if (_FAILURE(rc)) {
	    return 0;
	}
        subObj.setID(*subObjID);

	// We have to make sure the subObjID goes to disk. It'll hopefully get
	// correctly flushed later. Warning: lockedMarkDirty() won't work here.
        flags |= PSO_BASICRW_DIRTY;

        pso = (PSOBasicRW *)subObj.getObj(fsfile);
        if (pso == NULL) {
	    passertMsg(0, "?");
            // this is a problem...
            return 0;
        }
    }

    return pso->getDblk(lblkno - RW_MAXBLK, create);
}

/*
 * readBlock()
 *
 *   Reads the logical block specified from the disk and returns it.
 */
sval
PSOBasicRW::readBlock(uval32 lblkno, char *buffer,
		      PSOBase::AsyncOpInfo *cont /* = NULL */,
		      uval isPhysAddr /* = 0 */)
{
    // FIXME: this code is essentially the same we hae in
    //        PSOSmall::readBlock. We could factor it out
    //        on PSOBase, but them PSOBase wouldn't be a pure
    //        interface anymore ...

    sval rc;
    uval32 dblkno;

    AutoLock<BLock> al(lock);    // lock this PSO

    dblkno = getDblk(lblkno);
    
    // If got disk block zero, page is not on disk. It's a new page.
    if (!dblkno) {
	if (isPhysAddr) {
#ifndef KFS_TOOLS
	    rc = FSFileKFS::PageNotFound();
#endif //#ifndef KFS_TOOLS
	} else {
	    memset(buffer, 0, OS_BLOCK_SIZE);
	    rc = 0;
	}
#ifndef KFS_TOOLS // so we don't include ServerFileBlockKFS for tools ...
	// trigger continuation
	ServerFileBlockKFS::CompleteIORequest(cont->obj,
					      cont->addr, cont->offset,
					      rc);
#endif // #ifndef KFS_TOOLS
	return rc;
    }

    // Else disk block not zero. Issue read block to disk object.
    // FIXME: change disk routines to handle correct token
    rc = llpso->readBlock(dblkno, buffer, cont);

    // FIXME dilma : we should just propagate this error, but the code
    //               doesn't check for errors everywhere, so to make
    //               debugging easier, let's catch problems here
    passertMsg(_SUCCESS(rc), "disk block read failed\n");

    return rc;
}

/*
 * readBlockPhys()
 *
 *   Reads the logical block specified from the disk and returns it. The
 *   difference from readBlockPhys() is that it doesn't zero the buffer
 *   if the block is not found
 */
sval
PSOBasicRW::readBlockPhys(uval32 lblkno, char *buffer,
			  PSOBase::AsyncOpInfo *cont /* = NULL */)
{
    sval rc;
    uval32 dblkno;

    // lock this PSO
    AutoLock<BLock> al(lock);

    dblkno = getDblk(lblkno);
    
    // If got disk block zero, page is not on disk. It's a new page.
    if (!dblkno) {
	passertMsg(0, "for debugging");
	// FIXME: return proper SERROR!
	// FIXME: trigger continuation
	return -1;
    }
    // Else disk block not zero. Issue read block to disk object.
    // FIXME: change disk routines to handle correct token
    rc = llpso->readBlock(dblkno, buffer, cont);

    // FIXME dilma : we should just propagate this error, but the code
    //               doesn't check for errors everywhere, so to make
    //               debugging easier, let's catch problems here
    passertMsg(_SUCCESS(rc), "disk block read failed\n");

    return rc;
}

// block-cache integration stuff
BlockCacheEntry *
PSOBasicRW::readBlockCache(uval32 b)
{
    BlockCacheEntry* block;
    uval32 dblkno;

    lock.acquire();
    dblkno = getDblk(b);
    block = llpso->readBlockCache(dblkno);
    lock.release();
    return block;
}

/*
 * writeBlock()
 *
 *   Writes the logical block specified to the disk.
 */
SysStatus
PSOBasicRW::writeBlock(uval32 lblkno, char *buffer,
		       PSOBase::AsyncOpInfo *cont /* = NULL */)
{
    sval rc;
    uval32 dblkno;

    AutoLock<BLock> al(lock);    // lock this PSO

    dblkno = getDblk(lblkno, 1);
    
    // If got block zero, something's wrong; getDblk should've done allocation
    if (!dblkno) {
	passertMsg(0, "deal with this\n");
	// FIXME: return proper SERROR!
	// FIXME: trigger continuation with operation result
	return -1;
    }
    // Else disk block not zero. Issue write block to disk object.
    // FIXME: change disk routines to handle correct token
    rc = llpso->writeBlock(dblkno, buffer, cont);

    // FIXME dilma : we should just propagate this error, but the code
    //               doesn't check for errors everywhere, so to make
    //               debugging easier, let's catch problems here
    passertMsg(_SUCCESS(rc), "disk block read failed\n");

    return rc;
}

// block-cache integration
/* virtual */ SysStatus
PSOBasicRW::writeBlockCache(BlockCacheEntry *block, uval32 lblkno)
{
    // Check if the page is mapped to a disk location
    if (!block->getBlockNumber()) {
	uval32 dblkno;

	// lock this PSO
	lock.acquire();

	dblkno = getDblk(lblkno, 1);
    
	// If got block zero, something's wrong; getDblk should've done allocation
	if (!dblkno) {
	    lock.release();
	    // FIXME: return proper SERROR!
	    return -1;
	}

	globals->blkCache->updateBlockNumber(block, dblkno);
	lock.release();
    }
    llpso->writeBlockCache(block, lblkno);
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
/* virtual */ sval
PSOBasicRW::freeBlocks(uval32 from, uval32 to)
{
    sval rc;

    // lock & deallocate
    lock.acquire();
    rc = locked_freeBlocks(from, to);
    lock.release();

    return rc;
}

/*
 * locked_freeBlocks()
 *
 *   Deallocates blocks assuming that the lock for this PSO is already held.
 */
sval
PSOBasicRW::locked_freeBlocks(uval32 fromLBlk, uval32 toLBlk, uval inUnlink /* =0 */)
{
    _ASSERT_HELD(lock);

    uval rc;
    uval32 startLBlk;

    // grab the logical block numbers based on the file offset
    startLBlk = fromLBlk;

    // do handling of this PSO
    if (fromLBlk < RW_MAXBLK) {
	while ((fromLBlk < RW_MAXBLK) && (fromLBlk <= toLBlk)) {
	    if (dblk[fromLBlk]) {
                // free the block
                rc = globals->super->freeBlock(dblk[fromLBlk]);
		tassertMsg(rc == 0, "woops, free block failed fromLBlk = %d"
			   " rc 0x%lx\n", fromLBlk, rc);
		dblk[fromLBlk] = 0;

                // mark this PSO dirty
                locked_markDirty(fromLBlk);
	    }
	    fromLBlk++;
	}
    }

    // now handle sub objects
    if (fromLBlk <= toLBlk) {
        PSOBase *pso = (PSOBase *)subObj.getObj(fsfile);
        if (pso == NULL) {
            KFS_DPRINTF(DebugMask::PSO_BASIC_RW,
			"Want to free blocks %d to %d, but no sub-object\n",
                        fromLBlk, toLBlk);
            return 0;
        }

        pso->freeBlocks(0, toLBlk - RW_MAXBLK);
    }

    // FIXME: should have a block count in the PSO for this
    // now delete this pso if it has nothing in it, and no sub-objects
    if (((fromLBlk - startLBlk) == RW_MAXBLK)
	&& !subObj.hasObj() && !inUnlink) {
        locked_unlink();
    }

    return 0;
}

/*
 * unlink()
 *
 *   Destroys this object and all sub-objects and free's their data.
 */
void
PSOBasicRW::unlink()
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
PSOBasicRW::locked_unlink()
{
    _ASSERT_HELD(lock);

    PSOBase *pso;

    // don't try to flush anything anymore
    flags &= ~PSO_BASICRW_DIRTY;

    // This block doesn't have to get written out. Clean it.
    data->markClean();

    // free all the blocks in this object
    locked_freeBlocks(0, RW_MAXBLK - 1, 1);

    // unlink the sub-object
    pso = (PSOBase *)subObj.getObj(fsfile);
    if (pso != NULL) {
        pso->unlink();
    }

    KFS_DPRINTF(DebugMask::PSO_BASIC_RW,
		"PSOBasicRW::DeleteFile: deleting file %u\n", id.id);

    // This block doesn't have to get written out. Clean it.
    data->markClean();

    // free the metadata block
    globals->super->freeBlock(blkno);

    // free the RecordMap entry
    globals->recordMap->freeRecord(&id);

    // don't try to flush anything anymore
    flags &= ~PSO_BASICRW_DIRTY;
}

/*
 * flush()
 *
 *   Flushes all of this object's metadata to disk.  Because there is
 *   a pointer to the subobject in this object's metadata, it is
 *   important to flush it first.
 */
void
PSOBasicRW::flush()
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
PSOBasicRW::locked_flush()
{
    _ASSERT_HELD(lock);

    PSOBase *pso;

    // flush the sub-object
    pso = (PSOBase *)subObj.gobj();
    if (pso != NULL) {
        pso->flush();
    }

    // flush this object if it is dirty
    if (flags & PSO_BASICRW_DIRTY) {
	// write back meta-data block number to record entry
	char *entry[KFS_RECORD_SIZE];

	// place the meta-data block number as first uval32 of the buffer
	((uval32 *)entry)[0] = blkno;
	globals->recordMap->setRecord(id, (char *)entry);

	// set meta-data block dirty
	globals->blkCache->markDirty(data);
	flags &= ~PSO_BASICRW_DIRTY;
    }
}

/*
 * markDirty()
 *
 *   Mark the PSO holding this logical block number as dirty.
 */
void
PSOBasicRW::markDirty(uval32 lblkno)
{
    // lock this PSO
    lock.acquire();

    // do the actual marking
    locked_markDirty(lblkno);

    // unlock this PSO
    lock.release();
}

/*
 * locked_markDirty()
 *
 *   Mark the PSO dirty assuming that the lock is already held.
 */
void
PSOBasicRW::locked_markDirty(uval32 lblkno)
{
    _ASSERT_HELD(lock);

    PSOBasicRW *pso;

    if (lblkno < RW_MAXBLK) {
        flags |= PSO_BASICRW_DIRTY;
	dirtyNode = globals->super->addDirtySO(this);
    } else {
        pso = (PSOBasicRW *)subObj.getObj(fsfile);
	tassertMsg(pso != NULL, "marking lblkno %u dirty... no sub-object\n",
		   lblkno);
        pso->markDirty(lblkno - RW_MAXBLK);
    }
}

/*
 * special()
 *
 *   Does nothing.  Must be declared because of virtual tag in class PSOBase.
 */
sval
PSOBasicRW::special(sval operation, void *buf)
{
    tassertMsg(0, "PSOBasicRWO::special() called\n");
    return -1;
}

/*
 * clone()
 *
 *   Creates a new PSOBasicRW from the given "object related state" map entry.
 */
/* virtual */ ServerObject *
PSOBasicRW::clone(ObjTokenID otokID, FSFileKFS *f)
{
    ServerObject *pso = new PSOBasicRW(otokID, f, llpso, globals);
    return pso;
}

/*
 * locationAlloc()
 *
 *   Allocates space for a PSOBasicRW and returns its location.
 */
SysStatus
PSOBasicRW::locationAlloc(ObjTokenID _id)
{
    uval bno;
    char entry[KFS_RECORD_SIZE];
    BlockCacheEntry *buf;

    // allocate a block on the disk for the metadata
    bno = globals->super->allocBlock();
    
    if (_FAILURE((sval)bno)) {
	tassertMsg(0, "PSOBasicRW::locationAlloc() failed to alloc block\n");
	return (sval)bno;
    }

    buf = globals->blkCache->getBlock(bno);

    // initialize the metadata
    memset(buf->getData(), 0, OS_BLOCK_SIZE);

    globals->blkCache->markDirty(buf);

    globals->blkCache->freeBlock(buf);

    // place the meta-data block number as first uval32 of the buffer
    ((uval32 *)entry)[0] = bno;
    globals->recordMap->setRecord(_id, (char *)entry);
    
    return 0;
}
