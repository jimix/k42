/*
 * K42 File System Checker
 *
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: fsck.C,v 1.16 2004/02/23 21:17:38 dilma Exp $
 */

/*
 * The intent of this program is to *show* inconsistencies on a K42
 * File System partition. Right now, we have no intention of fixing
 * the inconsistencies. This will be mainly used for debugging
 * purposes. (And maybe in a remote and distant future, we'll
 * implement the fixing part...)
 */

/*
 * We try to copy the behaviour from e2fsck from e2fsprogrs.
 * It is devided into 5 passes (phases):
 *
 * Pass 1 - Makes tests on each inode (correctness of st_mode, st_size
 *          and st_blocks fields). Also tries to gather information for
 *          next passes.
 *
 * Pass 2 - Makes tests on each directory (mainly checking if the dir
 *          entries look consistent).
 *
 * Pass 3 - Tests connectivity, i.e., if every inode is accessible by
 *          the root directory.
 *
 * Pass 4 - Kills off the files/directories which are not connected to
 *          the file system and adds puts them in /lost+found
 *
 * Pass 5 - Checks block and inode bitmaps with on-disk bitmaps
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_MAX 1023

// turn on debugging
//#define KFS_DEBUG

#include "fs.H"
#include "Disk.H"
#include "FileDisk.H"

#include "DataBlock.H"
#include "ServerObject.H"
#include "SuperBlock.H"
#include "ORSMap.H"
#include "PSOTypes.H"
#include "LSOBasic.H"
#include "LSOBasicDir.H"
#include "PSOBasicRW.H"
#include "PSORecordMap.H"
#include "KFSGlobals.H"

// WARNING!!!! This is hard-coded to avoid using the ORSMap!
// If the SuperBlockStruct ever changes this *will* break!
#define PSO_RMAP_LOCATION 11

uval8 *sb_blockmap = NULL;    //[OS_BLOCK_SIZE*BlockMap::numBlocks];
uval8 *dir_offsets = NULL;
uval8 *inode_offsets = NULL;
uval8 *connected_inode_offsets = NULL;

SysStatus
checkBitInBitmap(uval8 *bitmap, uval bit)
{
    return (!!(bitmap[bit/8] & (1 << (bit % 8))));
}

void
setBitInBitmap(uval8 *bitmap, uval bit)
{
    bitmap[bit/8] |= 1 << (bit % 8);
}

/*
 * initORSMapBitMap
 *
 *   Allocate a bitmap with for the ORSMap. Already set the bits which
 *   are supposed to be "free" so we can prohibit any object for
 *   claiming that they live there.
 */
SysStatus
initORSMapBitmap(KFSGlobals *g, SuperBlock *sb, uval8 **orsBitmap)
{
    uval orsblocks, i;
    DataBlock *orsmapData;
    uval64 *orsmapDataBuf;
    uval64 *firstFree; // index of the first free entry

    for (orsblocks = 0; sb->getORSmapBlk(orsblocks); orsblocks++);
    if (!orsblocks) {
	err_printf("initORSMapBitmap: Ooops! The superblock says "
		   "the ORSMap is empty!\n");
	tassertMsg(0, "?");
	return -1;
    }
    *orsBitmap = (uval8 *)malloc((orsblocks*ENTRIES_PER_ORSMAPBLOCK)/8);
    // ORSMap entry 0 is reserved
    setBitInBitmap(*orsBitmap, 0);
    for (i = 0; i < orsblocks; i++) {
	orsmapData = new DataBlock(g->disk_ar, sb->getORSmapBlk(i), g);
	orsmapDataBuf = (uval64 *)orsmapData->getBuf();

	firstFree = orsmapDataBuf;
	while (*firstFree < ENTRIES_PER_ORSMAPBLOCK) {
	    //KFS_DPRINTF(DebugMask::FSCK, "firstFree=%llu\n", *firstFree);
	    // check if this entry has been already accessed
	    if (checkBitInBitmap(*orsBitmap,
				 i*ENTRIES_PER_ORSMAPBLOCK + *firstFree)) {
		err_printf("initORSMapBitmap: Ooops! ORSMap entry already "
			   "used?! for lblkno=%lu, dblk=%u, entry=%llu, "
			   "id=%llu\n",
			   i, sb->getORSmapBlk(i), *firstFree,
			   i*ENTRIES_PER_ORSMAPBLOCK+*firstFree);
		return -1;
	    }
	    // mark this free entry as "used"
	    setBitInBitmap(*orsBitmap, i*ENTRIES_PER_ORSMAPBLOCK + *firstFree);

	    *firstFree = ((ORSMapEntry *)
			  (orsmapDataBuf + 1 + *firstFree))->location;
	}
    }

    return 0;
}

SysStatus
checkORSMapBitmap(SuperBlock *sb, uval8 *orsBitmap)
{
    uval orsblocks, i;
    sval rc = 0;

    for (orsblocks = 0; sb->getORSmapBlk(orsblocks); orsblocks++) {
	for (i = 0; i < ENTRIES_PER_ORSMAPBLOCK; i++) {
	    if (!checkBitInBitmap(orsBitmap,
				  orsblocks*ENTRIES_PER_ORSMAPBLOCK + i)) {
		err_printf("checkORSMapBitmap: Ooops! Leaked ORSMap entry?! "
			   "for lblkno=%lu, dblk=%u, entry=%lu, id=%lu\n",
			   orsblocks, sb->getORSmapBlk(orsblocks), i,
			   orsblocks*ENTRIES_PER_ORSMAPBLOCK+i);
		rc = -1; // continue to catch all the inconsistencies
	    }
	}
    }
    return rc;
}

/*
 * checkMetaDataInodes
 *
 *   Sweep the inode map checking to see if each inode's st_mode are
 *   legal (right now only regular files and dirs are
 *   implemented). Also get the LSO's PSOs and sum up the blocks they
 *   are using to compare against st_size and st_blocks.  Also go
 *   ahead an make sure all ObjTokenIDs are unique (and therefore no
 *   entry in the ORSMap is shared).
 *
 *   This basically implements Pass 1, the strategy here is:
 *
 *   * For every inode in the FS, we have a PSORecordMap which contain
 *     it's meta-data. So, sweep the PSORecordMap, and for each entry:
 *
 *      * Check if the st_ino (which is the otokID) matches the
 *        entry.location of the PSORecordMap entry
 *
 *      * Check if st_mode is valid (S_IFREG, S_IFDIR)
 *
 *      * Sweep all blocks in the PSO (if any). Compare the number of data
 *        blocks to st_size and st_blocks
 *
 *   * During the sweep, keep track of the ORSMap bitmap and the
 *     SuperBlock BlockMap, and check them for consistency at the end of
 *     the sweep. Also collect the offset to directories which are used
 *     in phases 2 and 3.
 */
SysStatus
checkMetaDataInodes(KFSGlobals *g, SuperBlock *sb)
{
    DataBlock *psoRmapMetaData, *psoRmapData, *psoBasicRWMetaData;
    uval8 *psoRmapMetaDataBuf, *psoRmapDataBuf, *psoBasicRWMetaDataBuf;
    uval8 *orsBitmap;
    uval offset, j, lblkno, totalInodes = 0, inodeSize;
    uval8 *psoRmapBitmap;   // has [PSO_RMAP_BITMAP_SIZE] entries
    uval64 *psoRmapDblk;    // has [PSO_RMAP_NUM_BLOCKS] entries
    uval32 *psoBasicRWDblk; // has [RW_MAXBLK] entries
    ObjTokenID psoBasicRWotokID, orsID, psoRmapSubID;
    KFSStat *stat;
    ORSMapEntry orsEntry;

    // init some stuff
    orsID.dPart = psoBasicRWotokID.dPart = 0; // ??
    if (initORSMapBitmap(g, sb, &orsBitmap) == -1) {
	err_printf("checkMetaDataInodes: Ooops! Error while initializing the "
		   "ORSMap\n");
	return -1;
    }

    // Get the PSOs DataBlock
    psoRmapMetaData = new DataBlock(g->disk_ar, PSO_RMAP_LOCATION, g);
    psoRmapMetaDataBuf = (uval8 *)psoRmapMetaData->getBuf();
    // mark them used in the SuperBlock blockmap
    setBitInBitmap(sb_blockmap, PSO_RMAP_LOCATION);

    // Check if the ORSMap has correct values for these two "global" PSORecordMaps
    orsID.id = 1;
    if (g->kfsORSMap->getEntry(orsID, &orsEntry)) {
	err_printf("checkMetaDataInodes: Ooops! Trying to get this "
		   "PSORecordMap's ORSMap entry for PSO=%u, orsID=%u\n", 1, 1);
	return -1;
    }
    // Check if this the ORSMap for this PSORecordMap points to the correct
    // location
    if (orsEntry.location != PSO_RMAP_LOCATION) {
	err_printf("checkMetaDataInodes: Oops! Incorrect PSORecordMap entry "
		   "for global PSORecordMap"
		   "ORSMapEntry.location is %llu and this PSORecordMap "
		   "is located at %u\n",
		   orsEntry.location, PSO_RMAP_LOCATION);
	return -1;
    }
    // Mark this ORSMap entry as used
    setBitInBitmap(orsBitmap, orsID.id);

    for (uval i = 0; psoRmapMetaDataBuf; i++) {
	psoRmapBitmap = (uval8 *)psoRmapMetaDataBuf;
	psoRmapDblk = (uval64 *)(psoRmapMetaDataBuf + 
				 sizeof(uval32) * PSO_RMAP_NUM_BLOCKS);
	psoRmapSubID = *(ObjTokenID *)(psoRmapMetaDataBuf + 
				       (sizeof(uval32) + sizeof(uval64)) * 
				       PSO_RMAP_NUM_BLOCKS);
	
	for (j = 0; j < PSO_RMAP_BITS; j++) {
	    if (checkBitInBitmap(psoRmapBitmap, j)) {
		// Found an used record! New inode!
		inodeSize = 0;
		totalInodes++;

		// Search for the j-th record in the pso records
		lblkno = j / (OS_BLOCK_SIZE / PSO_RMAP_RECORD_SIZE);
		offset = (j % (OS_BLOCK_SIZE / PSO_RMAP_RECORD_SIZE))
		    * PSO_RMAP_RECORD_SIZE;

		if (!psoRmapDblk[lblkno]) {
		    err_printf("checkMetaDataInodes: Ooops! Trying to get "
			       "data block for inode=%lu, lblkno=%lu, "
			       "offset=%lu\n", totalInodes, lblkno, offset);
		    return -1;
		}
		// We can't check this block in the bitmap, because
		// PSORecordMap blocks are shared by many LSOs, but we
		// still need to set them as used
		setBitInBitmap(sb_blockmap, psoRmapDblk[lblkno]);

		psoRmapData = new DataBlock(g->disk_ar, psoRmapDblk[lblkno], g);
		psoRmapDataBuf = (uval8 *)psoRmapData->getBuf() + offset;
		psoBasicRWotokID.id = ((ObjTokenID *)psoRmapDataBuf)->id;
		stat = (KFSStat *)(psoRmapDataBuf + sizeof(ObjTokenID));

		KFS_DPRINTF(DebugMask::FSCK,
			    "Info for inode %lu: st_ino: %llu\n"
			    "                 st_mode: %llo\n"
			    "                st_nlink: %llu\n"
			    "                 st_size: %llu\n"
			    "               st_blocks: %u\n"
			    "                  PSO ID: %llu\n",
			    totalInodes, stat->st_ino, stat->st_mode,
			    stat->st_nlink, stat->st_size, stat->st_blocks,
			    psoBasicRWotokID.id);

		// In KFS st_ino corresponds to this inode's LSO ID.
		// Check the ORSMap to see if ORSMapEntry.location
		// points to this PSORecordMap record.
		orsID.id = stat->st_ino;
		if (g->kfsORSMap->getEntry(orsID, &orsEntry)) {
		    err_printf("checkMetaDataInodes: Ooops! Trying to get this"
			       " LSO's ORSMap entry for inode=%lu, orsID=%llu\n",
			       totalInodes, orsID.id);
		    return -1;
		}

		// Check if this the ORSMap for this LSO points to this
		// PSORecordMap
		if (orsEntry.location
		    != (j + i*PSO_RMAP_BITS)*PSO_RMAP_RECORD_SIZE) {
		    err_printf("checkMetaDataInodes: Oops!"
			       "Inconsistency between ORSMap and PSORecordMap "
			       "for inode=%lu, st_ino=%llu, lblkno=%lu, "
			       "offset=%lu!\n ORSMapEntry.location is %llu and "
			       "this inode is at %lu\n",
			       totalInodes, stat->st_ino, lblkno, offset,
			       orsEntry.location,
			       (j + i*OS_BLOCK_SIZE*PSO_RMAP_NUM_BLOCKS));
		    return -1;
		}

		// Check if this ORSMap entry has no users, and mark it used
		if (checkBitInBitmap(orsBitmap, orsID.id)) {
		    err_printf("checkMetaDataInodes: Oops! ORSMap entry already "
			       "used for inode=%lu, st_ino(=orsID.id)=%llu, "
			       "lblkno=%lu, offset=%lu, entry.location=%llu\n",
			       totalInodes, stat->st_ino, lblkno, offset,
			       orsEntry.location);
		    return -1;
		}
		setBitInBitmap(orsBitmap, orsID.id);
		// We can't check this block in the bitmap, because
		// PSORecordMap blocks are shared by many LSOs, but we
		// still need to set them as used
		setBitInBitmap(sb_blockmap,
			       sb->getORSmapBlk(orsID.id
						/ENTRIES_PER_ORSMAPBLOCK));

		// Check if we have a sane mode (we currently only support
		// regular and dir files).
		if (!S_ISDIR(stat->st_mode) && !S_ISREG(stat->st_mode)) {
		    err_printf("checkMetaDataInodes: Ooops! "
			       "Unknow file mode for inode=%lu, st_ino=%llu, "
			       "lblkno=%lu, offset=%lu\n",
			       totalInodes, stat->st_ino, lblkno, offset);
		    return -1;
		}

		// Keep this for phase 2 if it's a directory inode
		if (S_ISDIR(stat->st_mode)) {
		    setBitInBitmap(dir_offsets, j + i*PSO_RMAP_BITS);
		}
		// Keep all inodes in another array
		setBitInBitmap(inode_offsets, stat->st_ino);

		while (psoBasicRWotokID.id) {
		    // Now, time to get the PSOBasicRW (with psoBasicRWotokID->id)
		    if (g->kfsORSMap->getEntry(psoBasicRWotokID, &orsEntry)) {
			err_printf("checkMetaDataInodes: Ooops! Trying to get "
				   "this PSO's ORSMap entry for inode=%lu, "
				   "psoBasicRWotokID=%llu\n",
				   totalInodes, psoBasicRWotokID.id);
			return -1;
		    }

		    // Check if this ORMap entry has no users, and mark it used
		    if (checkBitInBitmap(orsBitmap, psoBasicRWotokID.id)) {
			err_printf("checkMetaDataInodes: Oops! ORSMap entry "
				   "already used for inode=%lu, "
				   "psoBasicRWotokID.id=%u, "
				   "entry.location=%llu\n",
				   totalInodes, psoBasicRWotokID.id,
				   orsEntry.location);
			return -1;
		    }
		    setBitInBitmap(orsBitmap, psoBasicRWotokID.id);

		    KFS_DPRINTF(DebugMask::FSCK,
				"Trying to access meta-data for PSO at "
				"block=%llu\n", orsEntry.location);
		    if (checkBitInBitmap(sb_blockmap, orsEntry.location)) {
			err_printf("checkMetaDataInodes: Ooops! "
				   "Trying to access already accessed block "
				   "number for inode=%lu, block=%llu\n",
				   totalInodes, orsEntry.location);
			return -1;
		    }
		    setBitInBitmap(sb_blockmap, orsEntry.location);

		    psoBasicRWMetaData    = new DataBlock(g->disk_ar,
							  orsEntry.location, g);
		    psoBasicRWMetaDataBuf = (uval8 *)psoBasicRWMetaData->getBuf();
		    psoBasicRWotokID.id   = ((ObjTokenID *)
					     psoBasicRWMetaDataBuf)->id;
		    psoBasicRWDblk        = (uval32 *)
			(psoBasicRWMetaDataBuf + sizeof(ObjTokenID));

		    for (uval k = 0; k < RW_MAXBLK; k++) {

			// check which (if any) actual disk blocks are
			// used by this PSO
			if (psoBasicRWDblk[k]) {
			    // check if this block has been checked by
			    // another object
			    KFS_DPRINTF(DebugMask::FSCK,
					"Trying to access DataPSO at "
					"block=%u, lblkno=%lu\n",
					psoBasicRWDblk[k], k);
			    if (checkBitInBitmap(sb_blockmap,
						 psoBasicRWDblk[k])) {
				err_printf("checkMetaDataInodes: Ooops! "
					   "Trying to access already accessed "
					   "block number for inode=%lu, "
					   "block=%u\n",
					   totalInodes, psoBasicRWDblk[k]);
				return -1;
			    }
			    setBitInBitmap(sb_blockmap, psoBasicRWDblk[k]);
			    inodeSize += OS_BLOCK_SIZE;
			}
		    }

		    delete psoBasicRWMetaData;
		}

		// Check inode size and blocks used for consistency
		// Actually, st_size can be greater than or equal to
		// the collected/accounted inodeSize. This is mainly
		// due to sparse files (which might have a size
		// greater than actual blocks), or files which have
		// preallocated/reserved blocks (extents).
		if (ALIGN_UP(stat->st_size, OS_BLOCK_SIZE) < inodeSize) {
		    err_printf("checkMetaDataInodes: Ooops! Wrong file size for "
			       "inode=%lu, st_size=%llu, size accounted=%lu\n",
			       totalInodes, stat->st_size, inodeSize);
		    return -1;
		}

		if (stat->st_blocks != ALIGN_UP(stat->st_size, OS_BLOCK_SIZE)/OS_SECTOR_SIZE) {
		    err_printf("checkMetaDataInodes: Ooops! Wrong file blocks for"
			       " inode=%lu, st_blocks=%u, blocks accounted=%llu\n",
			       totalInodes, stat->st_blocks,
			       ALIGN_UP(stat->st_size, OS_BLOCK_SIZE)/OS_SECTOR_SIZE);
		    return -1;
		}

		delete psoRmapData;
	    }
	}
	// Check if the RecordMap has a subObject
	if (psoRmapSubID.id) {
	    if (g->kfsORSMap->getEntry(psoRmapSubID, &orsEntry)) {
		err_printf("checkMetaDataInodes: Ooops! Trying to get this child "
			   "PSORecordMap's ORSMap entry for PSO=%llu\n",
			   psoRmapSubID.id);
		return -1;
	    }

	    // Mark this ORSMap entry as used
	    setBitInBitmap(orsBitmap, psoRmapSubID.id);

	    delete psoRmapMetaData;
	    psoRmapMetaData = new DataBlock(g->disk_ar, orsEntry.location, g);
	    psoRmapMetaDataBuf = (uval8 *)psoRmapMetaData->getBuf();

	    setBitInBitmap(sb_blockmap, orsEntry.location);
	}
	else {
	    delete psoRmapMetaData;
	    psoRmapMetaData = NULL;
	    psoRmapMetaDataBuf = NULL;
	}
    }


    // check if the ORSMap was all used up
    if (checkORSMapBitmap(sb, orsBitmap) == -1) {
	err_printf("checkMetaDataInodes: Ooops! ORSMap seems to be broken!\n");
	return -1;
    }

    return 0;
}

/*
 * checkDirContentConsistency
 *
 *   Sweep the dir_offsets array collected in phase 1 indicating which
 *   inodes are directories. The goal of this phase is to check if the
 *   directory entries are sane. The following tests are performed:
 *
 *    * Check if the nameLength field is greater than one and less than
 *      PATH_MAX.
 *
 *    * Check if reclen is at least DIRENTRY_LEN + nameLength.
 *
 *    * Check if reclen doesn't point past the end of the block.
 *
 *    * Check if nameLength is at most DIRENTRY__LEN - reclen.
 *
 *    * Check if the inode number is in a permitted range.
 *
 *    * The first entry should be '.' and the inode should be the same as
 *      the directory.
 *
 *    * The second entry should be '..'.
 */
SysStatus
checkDirContentConsistency(SuperBlock *sb, KFSGlobals *g)
{
    DataBlock *psoRmapMetaData, *psoRmapData, *psoBasicRWMetaData;
    uval8 *psoRmapMetaDataBuf, *psoRmapDataBuf, *psoBasicRWMetaDataBuf;
    uval64 *psoRmapDblk;    // has [PSO_RMAP_NUM_BLOCKS] entries
    uval32 *psoBasicRWDblk; // has [RW_MAXBLK] entries
    uval32 j, offset, lblkno;
    sval rc = 0;
    ObjTokenID psoBasicRWotokID, psoRmapSubID;
    KFSStat *stat;
    ORSMapEntry orsEntry;
    uval8 *psoRmapBitmap;   // has [PSO_RMAP_BITMAP_SIZE] entries
    char entryName[PATH_MAX + 1];
    uval entryNumber;

    psoRmapMetaData = new DataBlock(g->disk_ar, PSO_RMAP_LOCATION, g);
    psoRmapMetaDataBuf = (uval8 *)psoRmapMetaData->getBuf();

    for (uval i = 0; psoRmapMetaDataBuf; i++) {
	psoRmapBitmap = (uval8 *)psoRmapMetaDataBuf;
	psoRmapDblk = (uval64 *)(psoRmapMetaDataBuf
				 + sizeof(uval32) * PSO_RMAP_NUM_BLOCKS);
	psoRmapSubID = *(ObjTokenID *)(psoRmapMetaDataBuf + 
				       (sizeof(uval32) + sizeof(uval64)) * 
				       PSO_RMAP_NUM_BLOCKS);

	for (j = 0; j < PSO_RMAP_BITS; j++) {
	    if (checkBitInBitmap(dir_offsets, j + i*PSO_RMAP_BITS)) {
		// Search for the j-th record in the pso records
		lblkno = j / (OS_BLOCK_SIZE / PSO_RMAP_RECORD_SIZE);
		offset = (j % (OS_BLOCK_SIZE / PSO_RMAP_RECORD_SIZE))
		    * PSO_RMAP_RECORD_SIZE;

		psoRmapData = new DataBlock(g->disk_ar, psoRmapDblk[lblkno], g);
		psoRmapDataBuf = (uval8 *)psoRmapData->getBuf() + offset;
		psoBasicRWotokID.id = ((ObjTokenID *)psoRmapDataBuf)->id;
		stat = (KFSStat *)(psoRmapDataBuf + sizeof(ObjTokenID));

		KFS_DPRINTF(DebugMask::FSCK,
			    "Dir info for inode: st_ino: %llu\n", stat->st_ino);

		entryNumber = 0;

		while (psoBasicRWotokID.id) {
		    // Now, time to get the PSOBasicRW (with psoBasicRWotokID->id)
		    if (g->kfsORSMap->getEntry(psoBasicRWotokID, &orsEntry)) {
			err_printf("checkMetaDataInodes: Ooops! Trying to get "
				   "this PSO's ORSMap entry for "
				   "psoBasicRWotokID=%llu\n",
				   psoBasicRWotokID.id);
			return -1;
		    }

		    psoBasicRWMetaData    = new DataBlock(g->disk_ar,
							  orsEntry.location, g);
		    psoBasicRWMetaDataBuf = (uval8 *)psoBasicRWMetaData->getBuf();
		    psoBasicRWotokID.id   = ((ObjTokenID *)
					     psoBasicRWMetaDataBuf)->id;
		    psoBasicRWDblk        = (uval32 *)
			(psoBasicRWMetaDataBuf + sizeof(ObjTokenID));

		    for (uval k = 0; k < RW_MAXBLK; k++) {
			if (psoBasicRWDblk[k]) {
			    DataBlock *dirDataBlock = new DataBlock(
				g->disk_ar, psoBasicRWDblk[k], g);
			    char *dirDataBuf = dirDataBlock->getBuf();
			    DirEntry *entry;
			    offset = 0;

			    entry = (DirEntry *) dirDataBuf;

			    while (entry->reclen) {

				// Check if nameLength looks consistent
				if (entry->nameLength < 1
				    || entry->nameLength > PATH_MAX) {
				    err_printf("checkDirContentConsistency: "
					       "Ooops! Wrong name length=%u\n",
					       entry->nameLength);
				    return -1;
				}

				// we need this as entry->name is not
				// null terminated
				memcpy(entryName, entry->name, entry->nameLength);
				entryName[entry->nameLength] = '\0';

				KFS_DPRINTF(DebugMask::FSCK,
					    "\tentry name: \"%s\"\n"
					    "\t     otokID: %llu\n"
					    "\t     reclen: %u\n"
					    "\t     length: %u\n",
					    entryName, entry->otokID,
					    entry->reclen, entry->nameLength);

				// check if reclen is at least the size of this
				// record
				if (entry->reclen <
				    LSOBasicDir::GetPhysRecLen(
					entry->nameLength)) {
				    err_printf("checkDirContentConsistency: "
					       "Ooops! Reclen=%u smaller than "
					       "GetPhysRecLen(nameLength)=%lu\n",
					       entry->reclen, 
					       LSOBasicDir::GetPhysRecLen(
						   entry->nameLength));
				    return -1;
				}

				// check if reclen doesn't point past this block
				if (entry->reclen > OS_BLOCK_SIZE - offset) {
				    err_printf("checkDirContentConsistency: "
					       "Ooops! Reclen=%u larger than "
					       "OS_BLOCK_SIZE - offset=%ld\n",
					       entry->reclen,
					       OS_BLOCK_SIZE - offset);
				    return -1;
				}

				// check if name length looks consistent
				if (entry->nameLength >= entry->reclen) {
				    err_printf("checkDirContentConsistency: "
					       "Ooops! Name length=%u "
					       "larger reclen=%u\n",
					       entry->nameLength, entry->reclen);

				    return -1;
				}

				// check if this entry points to a valid inode
				// number
				if (entry->otokID < 0
				    || entry->otokID > 2*PSO_RMAP_BITS ||
				    !checkBitInBitmap(inode_offsets,
						      entry->otokID)) {
				    err_printf("checkDirContentConsistency: "
					       "Ooops! Invalid inode number "
					       "%llu, max=%lu\n",
					       entry->otokID, 2*PSO_RMAP_BITS);
				    return -1;
				}

				// special checks for '.'
				if (entryNumber == 0) {
				    if (entry->nameLength != 1
					|| memcmp(entry->name, ".",
						  entry->nameLength) ||
					entry->otokID != stat->st_ino) {
					err_printf("checkDirContentConsistency: "
						   "Ooops! Invalid first entry!"
						   "nameLength=%u, name=%s, "
						   "otokID=%llu, st_ino=%llu\n",
						   entry->nameLength,
						   entry->name, entry->otokID,
						   stat->st_ino);
					return -1;
				    }
				} else if (entryNumber == 1) {
				    if (entry->nameLength != 2
					|| memcmp(entry->name, "..",
						  entry->nameLength)) {
					err_printf("checkDirContentConsistency: "
						   "Ooops! Invalid second entry!"
						   "nameLength=%u, name=%s, "
						   "otokID=%llu, st_ino=%llu\n",
						   entry->nameLength,
						   entry->name, entry->otokID,
						   stat->st_ino);
					return -1;
				    }
				}

				entryNumber++;

				offset += entry->reclen;
				entry = (DirEntry *)(dirDataBuf + offset);
			    }

			    delete dirDataBlock;
			}
		    }

		    delete psoBasicRWMetaData;
		}

		delete psoRmapData;
	    }
	}
	// Check if the RecordMap has a subObject
	if (psoRmapSubID.id) {
	    if (g->kfsORSMap->getEntry(psoRmapSubID, &orsEntry)) {
		err_printf("checkDirContentConsistency: Ooops! Trying to get this child "
			   "PSORecordMap's ORSMap entry for PSO=%llu\n",
			   psoRmapSubID.id);
		return -1;
	    }

	    delete psoRmapMetaData;
	    psoRmapMetaData = new DataBlock(g->disk_ar, orsEntry.location, g);
	    psoRmapMetaDataBuf = (uval8 *)psoRmapMetaData->getBuf();

	    setBitInBitmap(sb_blockmap, orsEntry.location);
	}
	else {
	    delete psoRmapMetaData;
	    psoRmapMetaData = NULL;
	    psoRmapMetaDataBuf = NULL;
	}

    }

    return rc;
}

/*
 * recursiveSweepTree()
 *
 *   Recursively "touches" all the file and directories under this
 *   directory, changing the connected_inode_offsets as we go along.
 */
static sval
recursiveSweepDir(LSOBasicDir *dir, KFSGlobals *g)
{
    int i = 0;
    uval offset;
    char dirEntryBuf[4096];
    DirEntry *entry = (DirEntry *)dirEntryBuf;
    ObjToken otok(g);
    ObjTokenID otokID;
    KFSStat stat;
    LSOBasic *lso;
    LSOBasicDir *subDir;

    // loop through all the entries in the directory
    while (dir->matchIndex(i, &offset, entry) >= 0) {
	otokID.id = entry->otokID;
	otokID.dPart = 0; // ????
        otok.setID(otokID);
        lso = (LSOBasic *)otok.getObj();
	tassertMsg(lso != NULL, "?");
        lso->getAttribute(&stat);

	setBitInBitmap(connected_inode_offsets, entry->otokID);

        if (S_ISDIR(stat.st_mode)
	    && memcmp(entry->name, ".", entry->nameLength)
	    && memcmp(entry->name, "..", entry->nameLength)) {
            subDir = (LSOBasicDir *)otok.getObj();
	    tassertMsg(subDir != NULL, "?");
            recursiveSweepDir(subDir, g);
        }

        i = offset;
    }

    return 0;
}

/*
 * sweepTree
 *
 *   Calls recursive
 */
SysStatus
sweepTree(SuperBlock *sb, KFSGlobals *g)
{
    sval rc = 0;
    LSOBasicDir *root;
    ObjToken rootTok(g);

    setBitInBitmap(connected_inode_offsets, sb->getRootLSO().id);
    rootTok.setID(sb->getRootLSO());
    root = (LSOBasicDir *)rootTok.getObj();
    tassertMsg(root != NULL, "?");

    if (recursiveSweepDir(root, g) == -1) {
	err_printf("sweepTree: Ooops! Something went wrong...\n");
	rc= -1;
    }

    return rc;
}

/*
 * compareInodeBitmaps
 */
SysStatus
compareInodeBitmaps(uval8 *inode_offsets, uval8 *connected_inode_offsets)
{
    sval rc = 0;

    if (memcmp(inode_offsets, connected_inode_offsets, 2*PSO_RMAP_BITMAP_SIZE)) {
	// Oops, got a difference!
	err_printf("compareInodeBitmaps: Ooops! Found inconsistencies, "
		   "starting sweep...\n");

	for (uval i = 0; i < 2*PSO_RMAP_BITS; i++) {
	    if (checkBitInBitmap(inode_offsets, i)
		!= checkBitInBitmap(connected_inode_offsets, i)) {
		err_printf("\tBit %lu is %lu in the inodemap and should be %lu\n",
			  i, checkBitInBitmap(inode_offsets, i),
			   checkBitInBitmap(connected_inode_offsets, i));
		rc = -1;
	    }
	}
    }

    return rc;
}

/*
 * compareSuperblockBitmap
 *
 *   Check to see if the "collected" blockmap (during sweeping inodes
 *   meta-data) is the same as the SuperBlock's
 *
 */
SysStatus
compareSuperblockBitmap(uval8 *sb_blockmap, SuperBlock *sb)
{
    uval8 *orig_blockmap;
    sval rc = 0;

    orig_blockmap = (uval8 *)malloc(SUPER_BLOCK_BMAP_SIZE);

    sb->copyBitMap(orig_blockmap);

    if (memcmp(orig_blockmap, sb_blockmap, SUPER_BLOCK_BMAP_SIZE)) {
	// Oops, got a difference!
	err_printf("compareSuperblockBitmap: Ooops! Found inconsistencies, "
		   "starting sweep...\n");

	for (uval i = 0; i < BlockMap::numBits; i++) {
	    if (checkBitInBitmap(orig_blockmap, i)
		!= checkBitInBitmap(sb_blockmap, i)) {
		err_printf("\tBit %lu is %lu in the SB and should be %lu\n",
			  i, checkBitInBitmap(orig_blockmap, i),
			   checkBitInBitmap(sb_blockmap, i));
		rc = -1;
	    }
	}
    }

    return rc;
}

int
main(int argc, char **argv)
{
    Disk *disk;
    uval nBlocks, bSize;

    if (argc != 2) {
	err_printf("Usage: %s <diskname>\n", argv[0]);
        return -1;
    }

    err_printf("\tWelcome to fsck.kfs!\n\n");

    KFS_DPRINTF(DebugMask::FSCK,
		"Starting %s. Reading disk... ", argv[0]);

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
	printf("%s: open(%s) failed: %s\n", argv[0], argv[1], strerror(error));
	return -1;
    }
    disk = new FileDisk(fd);

    disk->readCapacity(nBlocks, bSize);

    KFS_DPRINTF(DebugMask::FSCK,
		"Disk %s opened succesfully!\n"
		"Number of blocks=%lu, block size=%lu\n",
		argv[1], nBlocks, bSize);

    KFS_DPRINTF(DebugMask::FSCK,
		"\nTrying to read SuperBlock and the BlockMap... ");
    SuperBlock *sb;
    KFSGlobals *g = new KFSGlobals();
    if (!(sb = initFS(disk, g))) {
	err_printf("\n%s: error while reading the SuperBlock and BlockMap! "
		   "Bailing out!\n", argv[0]);
	return -1;
    }
    KFS_DPRINTF(DebugMask::FSCK, "success.\n");

    sb_blockmap = (uval8 *)malloc(OS_BLOCK_SIZE*BlockMap::numBlocks);
    dir_offsets = (uval8 *)malloc(2*PSO_RMAP_BITMAP_SIZE);
    inode_offsets = (uval8 *)malloc(2*PSO_RMAP_BITMAP_SIZE);
    connected_inode_offsets = (uval8 *)malloc(2*PSO_RMAP_BITMAP_SIZE);
    memset(sb_blockmap, 0, OS_BLOCK_SIZE*BlockMap::numBlocks);
    memset(dir_offsets, 0, 2*PSO_RMAP_BITMAP_SIZE);
    memset(inode_offsets, 0, 2*PSO_RMAP_BITMAP_SIZE);
    memset(connected_inode_offsets, 0, 2*PSO_RMAP_BITMAP_SIZE);

    // mark the "reserved blocks" as used
    for (int i = 0; i <= NUM_RESERVED; i++) {
	sb_blockmap[i / 8] |= 1 << (i % 8);
    }

    err_printf("PHASE 1: Checking inodes meta-data\n");
    if (checkMetaDataInodes(g, sb) == -1) {
	err_printf("\n%s: error while reading inode's meta-data from "
		   "PSORecordMap!\n", argv[0]);
	return -1;
    }
    KFS_DPRINTF(DebugMask::FSCK, "\n");

    // Other fscks do this at the very end, because they correct
    // errors as they check the file system. Since we don't do any
    // file system correction, we might as well do it now.
    err_printf("PHASE 5: Comparing Superblock blockmap with collected "
	       "blockmap\n");
    if (compareSuperblockBitmap(sb_blockmap, sb) == -1) {
	err_printf("\n%s: error while comparing blockmaps!\n", argv[0]);
	return -1;
    }
    KFS_DPRINTF(DebugMask::FSCK, "\n");

    err_printf("PHASE 2: Cheking to see if directory entries are ok!\n");
    if (checkDirContentConsistency(sb, g) == -1) {
	err_printf("\n%s: error while sweeping directory entries!\n", argv[0]);
	return -1;
    }
    KFS_DPRINTF(DebugMask::FSCK, "\n");

    err_printf("PHASE 3: Checking for inode connectivity\n");
    if (sweepTree(sb, g) == -1) {
	err_printf("\n%s: error while sweeping the file system tree!\n",
		   argv[0]);
	return -1;
    }
    if (compareInodeBitmaps(inode_offsets, connected_inode_offsets) == -1) {
	err_printf("\n%s: error while chekcking inode connectivity!\n",
		   argv[0]);
	return -1;
    }
    KFS_DPRINTF(DebugMask::FSCK, "\n");

    err_printf("\nHumm... seems that this disk passed all tests, everything "
	       "is clean.\n");
    if (sb_blockmap) {
	free(sb_blockmap);
    }
    if (dir_offsets) {
	free(dir_offsets);
    }
    return 0;
}
