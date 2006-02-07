/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: SuperBlock.C,v 1.83 2005/07/05 16:55:38 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 *
 *		  Hurricane File Manager : SuperBlock.C
 *			     Copyright 1990
 *
 *		    	   Authors: Orran Krieger
 *
 * 	This software is free for all non-commercial use, and anyone in this
 * category may freely copy, modify, and redistribute any portion of it,
 * provided they retain this disclaimer and author list.
 * 	The authors do not accept responsibility for any consequences of
 * using this program, nor do they in any way guarantee its correct operation.
 *****************************************************************************/

#include <kfsIncs.H>

// KFS header files
#include "Disk.H"
#include "ObjToken.H"
#include "SuperBlock.H"
#include "PSOTypes.H"
#include "RecordMap.H"
#include "KFSGlobals.H"
#include "LSOBasicDir.H"
#if defined (KFS_TOOLS) && defined(PLATFORM_OS_Darwin)
#include <sys/mount.h>
#else
#include <sys/vfs.h>  // for struct statfs
#endif

/*
 * Reads in superblock info & block bit maps from disk
 * ===================================================
*/
/* virtual */ SysStatus
SuperBlock::init(uval _format /* = 0 */)
{
    KFS_DPRINTF(DebugMask::SUPER_BLOCK, "Initializing the superblock\n");

    // initialize the lock
    lock.init();

    // Read superblock info from disk or prepare for formatting the disk
    sblock = globals->blkCache->getBlockRead(FIRST_SUPER_BLOCK);
    superblock = (SuperBlockStruct *)sblock->getData();

    if (_format == 1) {
	memset(superblock, 0, OS_BLOCK_SIZE);
    } else {
#ifdef KFS_DEBUG_ENDIAN
	err_printf("In SuperBlock::init()\n");
	superblock->print();
#endif // #ifdef KFS_DEBUG_ENDIAN
    }

    // Read in the block bitmap
    if (superblock->getNumBlocks()) {
	bmap = new BlockMap(llpso, globals, superblock->getNumBlocks());
	return bmap->init(superblock->blockMapBlocks);
    }

    return 0;
}

// initializes superblock, bitmaps, and ORS blocks
/* virtual */ SysStatus
SuperBlock::format(char *diskname)
{
    SysStatus rc;

    KFS_DPRINTF(DebugMask::SUPER_BLOCK, "In SuperBlock::format()\n");

    uval diskBlocks, diskBlockSize;
    uval32 numBlocks;

    // first zero superblock
    memset(sblock->getData(), 0, OS_BLOCK_SIZE);

    // initialize basic information
    strcpy(superblock->fsname, diskname);
    superblock->setVersion(KFS_VERSION);

    // find out capacity of drive & initialize superblock->num_blocks
    rc = globals->disk_ar->readCapacity(diskBlocks, diskBlockSize);
    _IF_FAILURE_RET(rc);

    numBlocks = (uval32)diskBlocks * diskBlockSize / OS_BLOCK_SIZE;
    superblock->setNumBlocks(numBlocks);

    KFS_DPRINTF(DebugMask::SUPER_BLOCK,
		"\tthere can be 0x%x OS_BLOCK_SIZE blocks on disk\n",
		numBlocks);

    bmap = new BlockMap(llpso, globals, numBlocks);

    // set up the blockmap array
    for (uval i = 0; i < bmap->numBlocks; i++) {
	superblock->blockMapBlocks[i] = CPU_TO_TE32((i + BLKBIT_BLOCK));
#ifdef KFS_DEBUG_ENDIAN
	err_printf("Set blockMapBlocks[%ld] ", i);
	PRINT_TE32(superblock->blockMapBlocks[i], 0);
	err_printf(" CPU 0x%x orig 0x%x\n",
		   TE32_TO_CPU(superblock->blockMapBlocks[i]),
		   (uval32)(i + BLKBIT_BLOCK));
#endif // #ifdef KFS_DEBUG_ENDIAN
    }

    bmap->init(superblock->blockMapBlocks);
    err_printf("bmap->numBlocks %ld\n", bmap->numBlocks);

    rc = bmap->format(bmap->numBlocks + BLKBIT_BLOCK, numBlocks);
    tassertWrn(_SUCCESS(rc), "bmap->format failed with rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    superblock->setFreeBlocks(numBlocks - (bmap->numBlocks + BLKBIT_BLOCK));

    // allocate block for RecordMap meta-data
    setRecMap((uval32)(RecordMap::locationAlloc(globals)));

    dirty |= SUPER_BLOCK_DIRTY;

#ifdef KFS_DEBUG_ENDIAN
    err_printf("In SuperBlockFormat:\n");
    superblock->print();
#endif // #if 1 // throw this away after debuging

    return 0;
};

/* virtual */ void
SuperBlock::sync()
{
    lock.acquire();

    KFS_DPRINTF(DebugMask::SUPER_BLOCK,
		"FS:writing out superblock and block maps\n");
    if (dirty & SUPER_BLOCK_DIRTY) {
	globals->blkCache->markDirty(sblock);

	bmap->sync();

	dirty &= ~SUPER_BLOCK_DIRTY;
    }

    // flush dirty objects
    flushDirtySOList();

    // flush record map, if dirty
    if (globals->recordMap->isDirty()) {
	globals->recordMap->flush();
    }

    globals->blkCache->sync();

#ifdef KFS_DEBUG_ENDIAN
    superblock->print();
#endif // #ifdef KFS_DEBUG_ENDIAN

    lock.release();
}

/*
 * return 0 if there is no block available
 */
/* virtual */ SysStatus
SuperBlock::allocBlock()
{
    KFS_DPRINTF(DebugMask::SUPER_BLOCK,"In SuperBlock::allocBlock\n");

    uval32 blkno;

    lock.acquire();

    if (superblock->getFreeBlocks() == 0) {
	passertMsg(0, "SuperBlock::allocBlock() no more blocks?\n");
	lock.release();
	return _SERROR(2607, 0, ENOSPC);
    }

    blkno = bmap->allocOne();
    KFS_DPRINTF(DebugMask::SUPER_BLOCK,
		"SB allocating block %d\n", blkno);

    if (blkno > 0) {
	uval32 fb = superblock->getFreeBlocks();
	superblock->setFreeBlocks(fb - 1);
	dirty |= SUPER_BLOCK_DIRTY;
    } else {
        passertMsg(0, "SuperBlock::allocBlock: invalid blkno %d\n", blkno);
    }

    lock.release();

    return blkno;
}

SysStatus
SuperBlock::allocExtent(uval32 &plen)
{
    KFS_DPRINTF(DebugMask::SUPER_BLOCK,
		"In SuperBlock::allocExtent (plen %ld)\n", (uval)plen);

    sval32 blkno;

    lock.acquire();

    // endianess conversion not really needed here
    if (superblock->getFreeBlocks() == 0) {
	tassertMsg(0, "SuperBlock::allocBlock() no more blocks?\n");
	lock.release();
	return _SERROR(2608, 0, ENOSPC);
    }

    blkno = bmap->alloc(plen);

    if (blkno > 0 && plen > 0) {
	uval32 fb = superblock->getFreeBlocks();
        superblock->setFreeBlocks(fb - plen);
	dirty |= SUPER_BLOCK_DIRTY;
    }
    else {
        passertMsg(0, "SuperBlock::allocExtent: invalid blkno %d or len %d\n",
		   blkno, plen);
    }

    lock.release();
    return blkno;
}

SysStatus
SuperBlock::freeBlock(uval32 blkno)
{
    lock.acquire();

    if ((blkno == 0) || (blkno > superblock->getNumBlocks())) {
	lock.release();
	err_printf("SB attempt free block %u, numBlocks %u\n", blkno,
                   superblock->getNumBlocks());
	passertMsg(0, "failed free block\n");
	return -1;
    }

    KFS_DPRINTF(DebugMask::SUPER_BLOCK, "SB freeing block %d\n", blkno);
    bmap->free(blkno, 1);
    superblock->setFreeBlocks(superblock->getFreeBlocks() + 1);
    dirty |= SUPER_BLOCK_DIRTY;

    globals->blkCache->forget(blkno);

    lock.release();

    return 0;
};

void
SuperBlock::display()
{
    err_printf("name        = %s\n", superblock->fsname);
    err_printf("num_blocks  = %6u\n", superblock->getNumBlocks());
    err_printf("free_blocks = %6u\n", superblock->getFreeBlocks());
    err_printf("type        = %04x\n", superblock->getType());
    err_printf("state       = %04x\n", superblock->getState());
}

SysStatus
SuperBlock::statfs(struct statfs *buf)
{
    buf->f_bsize  = OS_BLOCK_SIZE;
    buf->f_blocks = superblock->getNumBlocks();
    buf->f_bfree  = superblock->getFreeBlocks();
    buf->f_bavail = buf->f_bfree;

#ifdef SWEEP_BIT_MAP
    bmap->sweepBitMap();

    globals->recordMap->sweepBitMap();

#endif //#ifdef SWEEP_BIT_MAP

    return 0;
}

/*
 * copyBitMap()
 *
 *   This assumes that there is enough space in `bm` BMAP_BLOCK*OS_BLOCK_SIZE.
 *   Used in fsck.kfs.
 */
SysStatus
SuperBlock::copyBitMap(uval8 *bm)
{
    return bmap->copyBitMap(bm);
}

SysStatus
BlockMap::copyBitMap(uval8 *bm)
{
    for (uval i = 0; i < numBlocks; i++) {
	memcpy(bm + i*OS_BLOCK_SIZE, block[i]->getData(), OS_BLOCK_SIZE);
    }
    return 0;
}

SysStatus
BlockMap::sweepBitMap()
{
    uval32 *buf;
    uval32 i, blkno, j, count = 0;

    err_printf("BlockMap::sweepBitMap: Uuuhhuuu  starting sweep!\n");

    for (i = 0; i < numBlocks; i++) {
	buf = (uval32 *)block[i]->getData();

	for (j = 0; j < (OS_BLOCK_SIZE / sizeof(uval32)); j++) {

	    for (blkno = 0; blkno < 32; blkno++) {
		if ((TE32_TO_CPU(buf[j]) & (1 << blkno))) {
		    err_printf("blkno=%lu, blkno=%u, j=%u, i=%u, buf[%u]=%u\n",
			       blkno + j * 32 + i * OS_BLOCK_SIZE * 8, blkno,
			       j, i, j, TE32_TO_CPU(buf[j]));
		    count++;
		}
	    }
	}
    }
    err_printf("BlockMap::sweepBitMap: finished count %u accounted for!\n",
	       count);
    return 0;
}

/*
 * BlockMap::init()
 *
 *   Initialize the blockmap by reading in the data blocks
 */
SysStatus
BlockMap::init(uval32 *blocks)
{
    KFS_DPRINTF(DebugMask::SUPER_BLOCK, "Initializing the blockMap\n");
    dirty = new uval[numBlocks];
    block = new BlockCacheEntry*[numBlocks];

    for (uval i = 0; i < numBlocks; i++) {
#ifdef KFS_DEBUG_ENDIAN
	if (i < 4) { // for debuging only
	    err_printf("In BlockMap::init i is %ld, blocks[i] is 0x%x\n",
		       i, TE32_TO_CPU(blocks[i]));
	}
#endif // #ifdef KFS_DEBUG_ENDIAN
	block[i] = globals->blkCache->getBlockRead(TE32_TO_CPU(blocks[i]));
    }

    lastAlloc = 0;
    return 0;
}

/*
 * BlockMap::sync()
 *
 *   Sync the blockmap by writing out the data blocks
 */
SysStatus
BlockMap::sync()
{
    for (uval i = 0; i < numBlocks; i++) {
	if (dirty[i] & SUPER_BLOCK_DIRTY) {
	    globals->blkCache->markDirty(block[i]);
	    dirty[i] &= ~SUPER_BLOCK_DIRTY;
	}
    }

    return 0;
}

/*
 * BlockMap::format()
 *
 *   Format the bitmap as if the disk were empty.  Allocates the first
 *   'numReserved' blocks and allocates any blocks past 'blkCount'.
 *
 *   note: 'numReserved' must be less than OS_BLOCK_SIZE
 */
SysStatus
BlockMap::format(uval numReserved, uval blkCount)
{
    char *buf;

    for (uval i = 0; i < numBlocks; i++) {
        // zero the block
        buf = block[i]->getData();

	// FIXME: Livio and Dilma should revisit the logic below, it seems
	// suspicions. For now let's make sure that we don't execute this
	if (blkCount < (i + 1) * OS_BLOCK_SIZE) {
	    tassertWrn(0, "It seems we don't have blkCount (0x%lx, blocks available in the disk) large enough "
		       " to set up a block map with numBlocks (0x%lx) blocks in the mapping. (i %ld)\n",
		       blkCount, numBlocks, i);
	    return _SERROR(2802,0, 0);
	}
	passertMsg(blkCount >= (i + 1) * OS_BLOCK_SIZE,
		   "look at implementation");
        // allocate any blocks that physically don't exist
        if (blkCount < (i + 1) * OS_BLOCK_SIZE) {
	    passertMsg(0,
		       "fix initialization to be deal with endian issues\n");
            if (blkCount < i * OS_BLOCK_SIZE) {
                memset(buf, 1, OS_BLOCK_SIZE);
            } else {
                memset(buf, 0, blkCount % OS_BLOCK_SIZE);

                buf += blkCount % OS_BLOCK_SIZE;
                memset(buf, 1, OS_BLOCK_SIZE - blkCount % OS_BLOCK_SIZE);
            }
        } else {
            memset(buf, 0, OS_BLOCK_SIZE);
        }

        if (numReserved) {
            int chunks = numReserved / 32;
            int blocks = numReserved % 32;

            if (chunks) {
                memset(buf, 0xFF, chunks * 4);
            }
            for (; blocks; blocks--) {
		uval32 bufv = TE32_TO_CPU(((uval32 *)buf)[chunks]);
		bufv |= 1 << (blocks - 1);
		((uval32 *)buf)[chunks] = CPU_TO_TE32(bufv);
            }

            numReserved = 0;
        }
	dirty[i] |= SUPER_BLOCK_DIRTY;
    }

    return 0;
}

/*
 * BlockMap::alloc()
 *
 *   Allocates up to 'len' sequential blocks, setting 'len' to the
 *   actual number of blocks allocated, and returning the starting
 *   block.  If no blocks are allocated len is unchanged and a '0' is
 *   returned.
 */
uval32
BlockMap::alloc(uval32 &len)
{
    // FIXME: there are bugs in this code. Try executing it with
    // first block all FF..F, second one wth buf[0] also FFFFFFFF
    // and buf[1] being 0x3FFFFFFF, len = 3. The problem is with the
    // code if (offset + 1 ...) deciding to update i, j
#ifdef THIS_HAS_BEEN_FIXED
    // When fixing this, fix endian issues in this method
    passertMsg(0, "make sure endian issues in this are fixed\n");
    uval32 *buf;
    uval32 blkno, offset;
    uval i, j, k;

    // for now, just try and find a free block and go from there
    // FIXME: could do some hints to make this alot better
    i = lastAlloc / (OS_BLOCK_SIZE * 8);
    passertMsg(i >= 0 && i < numBlocks, "BlockMap::alloc(%u) trying to "
	       "access invalid block=%lu\n", len, i);

    do {
        buf = (uval32 *)block[i]->getData();

        for (j = 0; j < (OS_BLOCK_SIZE / sizeof(uval32)); j++) {
            if (buf[j] != 0xFFFFFFFF) {
                break;
            }
        }

        if (j != (OS_BLOCK_SIZE / sizeof(uval32))) {
            break;
        }

        i = (i + 1) % numBlocks;
    } while (i != (lastAlloc / (OS_BLOCK_SIZE * 8)));

    // check if no free blocks
    if (j == (OS_BLOCK_SIZE / sizeof(uval32)) &&
        i == (lastAlloc / (OS_BLOCK_SIZE * 8))) {
	passertMsg(0, "?");
        return 0;
    }

    // calculate the starting block
    for (blkno = 0; blkno < 32; blkno++) {
	if (!(buf[j] & (1 << blkno))) {
            // allocate the block
            buf[j] |= 1 << blkno;
	    break;
        }
    }
    blkno += j * 32 + i * OS_BLOCK_SIZE * 8;

    dirty[i] |= SUPER_BLOCK_DIRTY;

    // update the 'lastAlloc' block
    lastAlloc = (blkno + k) % numBits;

    // 'k' holds the number of sequential blocks
    len = k;
    return blkno;
#else
    passertMsg(0, "broken");
    return 0;
#endif // #ifdef THIS_HAS_BEEN_FIXED
}

/*
 * BlockMap::allocOne()
 *
 *   Allocates only one block, and returns the block number on
 *   success, and 0 otherwise.
 */
uval32
BlockMap::allocOne()
{
    uval32 *buf;
    uval32 blkno;
    uval i, j;

    // for now, just try and find a free block and go from there
    // FIXME: could do some hints to make this alot better
    blkno = i = lastAlloc / (OS_BLOCK_SIZE * 8);
    passertMsg(i >= 0 && i < numBlocks,
               "BlockMap::allocOne() trying to acces invalid "
	       "block=%lu numBlocks=%lu\n", i, numBlocks);

    do {
        buf = (uval32 *)block[i]->getData();

        for (j = 0; j < (OS_BLOCK_SIZE / sizeof(uval32)); j++) {
            if (buf[j] != 0xFFFFFFFF) {
                break;
            }
        }

        if (j != (OS_BLOCK_SIZE / sizeof(uval32))) {
            break;
        }

        i = (i + 1) % numBlocks;
    } while (i != blkno);

    // check if no free blocks
    if (j == (OS_BLOCK_SIZE / sizeof(uval32)) && i == blkno) {
	passertMsg(0, "Oops?\n");
        return 0;
    }

    // calculate the starting block
    for (blkno = 0; blkno < 32; blkno++) {
	uval32 bufv = TE32_TO_CPU(buf[j]);
	if (!(bufv & (1 << blkno))) {
            // allocate the block
            bufv |= 1 << blkno;
            buf[j] = CPU_TO_TE32(bufv);
	    break;
        }
    }
    blkno += j * 32 + i * OS_BLOCK_SIZE * 8;

    dirty[i] |= SUPER_BLOCK_DIRTY;

    // update the 'lastAlloc' block
    lastAlloc = (blkno + 1) % numBits;

    passertMsg(blkno != 0, "Oops?\n");

    return blkno;
}

/*
 * BlockMap::free()
 *
 *   Frees 'len' sequential blocks.
 */
SysStatus
BlockMap::free(uval blkno, uval len)
{
    uval32 *buf;
    uval index, length, mask, block_i, block_max, mask_i, mask_max;

    index = blkno;
    length = len;

    block_i = blkno / (OS_BLOCK_SIZE * 8);
    block_max = (blkno + len - 1) / (OS_BLOCK_SIZE * 8);
    mask_i = (blkno % (OS_BLOCK_SIZE * 8)) / 32;

    passertMsg(block_i >= 0 && block_i < numBlocks, "BlockMap::free(%lu, "
	       "%lu) trying to access invalid block_i=%lu\n", blkno, len,
	       block_i);
    passertMsg(block_max >= 0 && block_max < numBlocks, "BlockMap::free(%lu, "
	       "%lu) trying to acces invalid block_max=%lu\n",
               blkno, len, block_max);
    passertMsg(mask_i >= 0 && mask_i < OS_BLOCK_SIZE*8/32,
               "BlockMap::free(%lu, %lu) trying to acces invalid mask_i=%lu\n",
               blkno, len, mask_i);

    // loop freeing the appropriate blocks
    for (uval i = block_i; i <= block_max; i++) {
        buf = (uval32 *)block[i]->getData();

        if (block_i == block_max) {
            mask_max = ((blkno + len - 1) % (OS_BLOCK_SIZE * 8)) / 32;
        } else {
            mask_max = (OS_BLOCK_SIZE * 8) / 32 - 1;
        }

        for (uval j = mask_i; j <= mask_max; j++) {
            // create the appropriate mask
            if ((index % 32) == 0 && (length >= 32)) {
                mask = 0xFFFFFFFF;
            } else if (length < 32) {
		mask = 0;
		for (uval k = index % 32; k < index % 32 + length; k++) {
		    mask |= 1 << k;
		}
	    } else {
		mask = 0;
		for (uval k = index % 32; k < 32; k++) {
		    mask |= 1 << k;
		}
	    }

            // free the blocks
	    uval32 bufv = TE32_TO_CPU(buf[j]);
            bufv &= ~mask;
            buf[j] = CPU_TO_TE32(bufv);

            // update the index and length
            if (length < 32) {
                index += length;
                length = 0;
            } else {
                mask = 32 - (index % 32);
                index += mask;
                length -= mask;
            }
        }
	dirty[i] |= SUPER_BLOCK_DIRTY;

        mask_i = 0;
    }

    // The following scheme for updating lastAlloc is trying to
    // work well with the current implementation for hash table for blocks
    // (KFSHash.H version 1.12), meaning that it's trying to reuse block
    // numbers as much as possible (thereby keeping the hash table dense).
    if (lastAlloc > blkno) {
	lastAlloc = blkno;
    }

    return 0;
}

SysStatus
SuperBlock::createRootDirectory(void)
{
    KFS_DPRINTF(DebugMask::SUPER_BLOCK, "In SuperBlock::createRootDirectory\n");

    passertMsg(getRecMap() != 0, "?");

    ObjToken rootTok(globals);
    ObjTokenID rootTokID;
    LSOBasicDir *lso;

    if (globals->recordMap->allocRecord(OT_LSO_BASIC_DIR, &rootTokID) < 0) {
	err_printf("KFS::formatKFS() Error allocating OT_LSO_BASIC_DIR\n");
    }
    setRootLSO(rootTokID);
    rootTok.setID(rootTokID);

    lso = (LSOBasicDir *)rootTok.getObj(NULL);
    lso->initAttribute(S_IFDIR | (0755 & ~S_IFMT), 0, 0);

    // now create the '.' and '..' directories
    lso->createEntry(".", 1, &rootTok, (0755 & ~S_IFMT), 0);
    lso->createEntry("..", 2, &rootTok, (0755 & ~S_IFMT), 0);

    lso->flush();

    return 0;
}

void
SuperBlockStruct::print()
{
#define SB_PRINT64(name, v) err_printf("\t " name " CPU sees bits: 0x%llx", v); \
                            PRINT_TE64(v, 1);
#define SB_PRINT32(name, v) err_printf("\t " name " CPU sees bits: 0x%x", v); \
                            PRINT_TE32(v, 1);
#define SB_PRINT16(name, v) err_printf("\t " name " CPU sees bits: 0x%x", v); \
                            PRINT_TE16(v, 1);

    err_printf("What we found in SuperBlock:\n");
    err_printf("\t fsname: %s\n", fsname);
    err_printf("\t addr for version %p\n", &version);
    SB_PRINT32("version", version);
    err_printf("\t addr for numBlocks %p\n", &numBlocks);
    SB_PRINT32("numBlocks", numBlocks);
    err_printf("\t addr for freeBlocks %p\n", &freeBlocks);
    SB_PRINT32("freeBlocks", freeBlocks);
    err_printf("\t addr for type %p\n", &type);
    SB_PRINT16("type", type);
    err_printf("\t addr for state %p\n", &state);
    SB_PRINT16("state", state);
    err_printf("\t addr for recMapBlkno %p\n", &recMapBlkno);
    SB_PRINT32("recMap", recMapBlkno);
    passertMsg(sizeof(ObjTokenID) == sizeof(uval64), "?");
    err_printf("\t addr for rootLSO.id %p\n", &rootLSO.id);
    err_printf("\t rootLSO.id: CPU 0x%x ", TE32_TO_CPU(rootLSO.id));
    PRINT_TE32(rootLSO.id, 1);
    err_printf("\t addr for generation %p\n", &generation);
    err_printf("\t generation: %d\n", (int) generation);
    err_printf("\t addr for blockMapBlocks[0] %p\n", &blockMapBlocks[0]);
    SB_PRINT32("blockMapBlocks[0]", blockMapBlocks[0]);
}
