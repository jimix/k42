/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LSOBasicSymlink.C,v 1.4 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"

#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <string.h>

#include "LSOBasicSymlink.H"
#include "SuperBlock.H"

LSOBasicSymlink::LSOBasicSymlink(ObjTokenID *otokID, RecordMapBase *r,
				 KFSGlobals *g) : LSOBasic(otokID, r, g) { }

void
LSOBasicSymlink::init()
{
    getRecordMap()->getRecord(&id, lsoBufData);
    lsoBuf = (LSOBasicStruct*) &lsoBufData;
}

SysStatus
LSOBasicSymlink::initAttribute(const char *linkValue, uval mode,
			       uval uid, uval gid)
{
    sval rc = 0;
    LSOBasicSymlinkStruct *symlinkStruct = (LSOBasicSymlinkStruct *)lsoBuf;

    lock.acquire();
    
    symlinkStruct->statSetDev(0);
    symlinkStruct->statSetIno(id.id);
    symlinkStruct->statSetMode(mode);

    symlinkStruct->statSetNlink(0);
    symlinkStruct->statSetUid(uid);
    symlinkStruct->statSetGid(gid);

    symlinkStruct->statSetRdev(0);
    symlinkStruct->statSetSize(strlen(linkValue));
    symlinkStruct->statSetBlksize(OS_BLOCK_SIZE);
    symlinkStruct->statSetBlocks(0);

    uval64 t = time(NULL);
    symlinkStruct->statSetCtime(t);
    symlinkStruct->statSetMtime(t);
    symlinkStruct->statSetAtime(t);

    uval64 sz = symlinkStruct->statGetSize();
    if (sz <= FAST_SYMLINK_SIZE) {
	memcpy(symlinkStruct->getValuePtr(), linkValue, sz);
    } else if (sz <= MED_SYMLINK_SIZE) {
	// We need a new record to extend this link.
	// FIMXE! Linux kernel does not support 64-bit division in a 32-bit
	//       machine. That's why 'sz' is cast as uval in the following line
	uval nrec = ((uval)sz + KFS_RECORD_SIZE) / KFS_RECORD_SIZE;
	
	for (uval i = 0; i < nrec; i++) {
	    char cutValue[KFS_RECORD_SIZE];
	    uval size;
	    ObjTokenID tmp;
	    rc = getRecordMap()->allocRecord(OT_SYMLINK_EXT, &tmp);
	    passertMsg(_SUCCESS(rc),
		       "LSOBasicSymlinkStruct::initAttribute?\n");
	    symlinkStruct->setObjTokenID(tmp, i);

	    // Determine if we're at the "end" of the string
	    if (sz < KFS_RECORD_SIZE * (i + 1)) {
		size = sz - KFS_RECORD_SIZE * i;
	    } else {
		size = KFS_RECORD_SIZE;
	    }

	    memcpy(cutValue, linkValue + KFS_RECORD_SIZE*i, size);

	    ObjTokenID tmpID = symlinkStruct->getObjTokenID(i);
	    getRecordMap()->setRecord(&tmpID, cutValue);
	}
    } else if (sz <= (uval) OS_BLOCK_SIZE) {
	// First allocate a block;
	symlinkStruct->setBlkno(globals->super->allocBlock());

	// get the block from the BlockCache
	BlockCacheEntry *blockEntry =
	    globals->blkCache->getBlock(symlinkStruct->getBlkno());
	blockEntry->markClean();

	// copy the value into it, and release it
	memcpy(blockEntry->getData(), linkValue, sz);
	globals->blkCache->markDirty(blockEntry);
	globals->blkCache->freeBlock(blockEntry);
    } else {
	// Oops, symlink value too long!
	rc = _SERROR(2768, 0, ENAMETOOLONG);
    }
    markDirty();
    lock.release();
    return rc;
}

SysStatusUval
LSOBasicSymlink::readlink(char *buffer, uval buflen)
{
    LSOBasicSymlinkStruct *symlinkStruct = (LSOBasicSymlinkStruct *)lsoBuf;
    uval linkSize = symlinkStruct->statGetSize();

    // We set 'buflen' to designate how many bytes we want to copy into
    // the buffer. Should be something like MIN(buflen, linkSize)
    if (buflen > linkSize) {
	buflen = linkSize;
    }
    // else, the given buffer is not big enough. Just truncate.

    if (linkSize <= FAST_SYMLINK_SIZE) {
	memcpy(buffer, symlinkStruct->getValuePtr(), buflen);
//	buffer[linkSize] = 0;
    } else if (linkSize <= MED_SYMLINK_SIZE) {
	// Must get the records and "glue" then together.
	uval nrec = (buflen + KFS_RECORD_SIZE) / KFS_RECORD_SIZE;

	for (uval i = 0; i < nrec; i++) {
	    char cutValue[KFS_RECORD_SIZE];
	    uval size;
	    ObjTokenID tmpID = symlinkStruct->getObjTokenID(i);
	    getRecordMap()->getRecord(&tmpID, cutValue);

	    // Determine if we're at the "end" of the string
	    if (linkSize < KFS_RECORD_SIZE * (i + 1)) {
		size = buflen - KFS_RECORD_SIZE * i;
	    } else {
		size = KFS_RECORD_SIZE;
	    }
	    
	    memcpy(buffer + KFS_RECORD_SIZE*i, cutValue, size);
	}
//	buffer[linkSize] = 0;
    } else if (linkSize <= OS_BLOCK_SIZE) {
	// get the block from the BlockCache
	BlockCacheEntry *blockEntry =
	    globals->blkCache->getBlockRead(symlinkStruct->getBlkno());

	// copy the value from it, and release it
	memcpy(buffer, blockEntry->getData(), buflen);
	globals->blkCache->freeBlock(blockEntry);
	
    } else {
	passertMsg(0, "LSOBasicSymlinkStruct::readlink() Something's obviously "
		   "wrong! linkSize=%lu\n", linkSize);
    }

    return linkSize;
}

/*
 * locked_flush()
 *
 *   Flushes the LSO assuming the lock is held.
 *
 *  We want to override LSOBasic's flush() because it write-backs the
 *  ObjtokenID field in an inconsistent way for symlinks
 */
void
LSOBasicSymlink::locked_flush()
{
    _ASSERT_HELD(lock);

    if (isDirty()) {
	markClean();

	// then flush the object related state
	getRecordMap()->setRecord(&id, lsoBufData);
    }
}

/*
 * deleteFile()
 *
 *   Remove the file, freeing all used disk space.
 *
 *  We want to override LSOBasic's deleteFile() because we need to correctly
 *  release all records allocated for symlink content. Additionally, we have no
 *  underlying PSO, so we don't need to free it.
 */
/* virtual */ SysStatus
LSOBasicSymlink::deleteFile()
{
    LSOBasicSymlinkStruct *symlinkStruct = (LSOBasicSymlinkStruct *)lsoBuf;
    uval linkSize = symlinkStruct->statGetSize();

    lock.acquire();

    // no more flushing out
    markClean(); 
    // make sure there are no outstanding links
    if (getNumLinks() != 0) {
	tassertMsg(0, "look at this %p, stat.st_nlink %ld\n", this,
		   (uval) getNumLinks());
        lock.release();
        return -1;
    }

    // free the data
    if (linkSize > FAST_SYMLINK_SIZE) {
	if (linkSize <= MED_SYMLINK_SIZE) {
	    // Must free all the used records
	    uval nrec = (linkSize + KFS_RECORD_SIZE) / KFS_RECORD_SIZE;
	    
	    for (uval i = 0; i < nrec; i++) {
		ObjTokenID tmp = symlinkStruct->getObjTokenID(i);
		getRecordMap()->freeRecord(&tmp);
	    }
	} else if (linkSize <= OS_BLOCK_SIZE) {
	    globals->super->freeBlock(symlinkStruct->getBlkno());
	}
    }

    // free the record
    getRecordMap()->freeRecord(&id);

    lock.release();
    return 0;
}

/*
 * clone()
 *
 *   Creates a new LSOBasicSymlink from the given RecordMap entry.
 */
/* virtual */ ServerObject *
LSOBasicSymlink::clone(ObjTokenID *otokID, RecordMapBase *r)
{
    LSOBasicSymlink *lso = new LSOBasicSymlink(otokID, r, globals);
    lso->init();

    return lso;
}
