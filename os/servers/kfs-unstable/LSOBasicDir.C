/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br).
 *
 * $Id: LSOBasicDir.C,v 1.5 2004/05/06 19:52:49 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"

#include <time.h>
#include <sys/stat.h>

#include "LSOBasicDir.H"
#include "PSOTypes.H"
#include "KFSGlobals.H"

/*
 * initAttribute()
 *
 *   The actual initialization is done in LSOBasic. We just set
 *   directory specific initializations here.
 */
sval
LSOBasicDir::initAttribute(uval mode, uval uid, uval gid)
{
    ObjTokenID otokID;
    sval rc = 0;

    lock.acquire();

    // allocate a data object
    rc = getRecordMap()->allocRecord(OT_PRIM_UNIX_META, &otokID);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::LSO_BASIC, "LSOBasic::initAttribute() problem "
		    "creating ORSMap entry\n");
	return rc;
    }

    // see if we need to allocate PSO
    if (!dataPSO) {
	if (otokID.id) {
	    dataPSO = (PSOBase *)recordMap->getObj(&otokID);
	}
    } else {
	dataPSO->setID(&otokID);
    }

    lsoBuf->statSetDev(0);
    lsoBuf->statSetIno(id.id);
    lsoBuf->statSetMode(mode);
    //lsoBuf->setMode(type | (mode & ~S_IFMT));

    lsoBuf->statSetNlink(0);
    lsoBuf->statSetUid(uid);
    lsoBuf->statSetGid(gid);

    lsoBuf->statSetRdev(0);
    lsoBuf->statSetSize(0);
    lsoBuf->statSetBlocks(0);
    lsoBuf->statSetBlksize(0);

    uval64 t = time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);
    lsoBuf->statSetAtime(t);

    markDirty();
    lock.release();
    return rc;
}

/*
 * matchEntry()
 *
 *  Tries to locate the given file name in the data buffer.  If found,
 *  it sets the entry pointer to the requested entry, and the previous
 *  pointer to the entry directly before the requested entry.  If the
 *  requested entry is the first entry in the block, the previous
 *  pointer is set to NULL.
 */
sval
LSOBasicDir::matchEntry(const char *fname, sval length, DirEntry **entryp,
			DirEntry **prev, BlockCacheEntry **block, uval &i)
{
    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR,
		"LSOBasicDir::matchEntry IN\n");

    uval blocksInFile, offset;
    char *buf;

    tassertMsg(dataPSO != NULL, "?");

    blocksInFile = (lsoBuf->statGetSize() + OS_BLOCK_SIZE - 1) / OS_BLOCK_SIZE;
    tassertMsg(blocksInFile != 0, "?");

    if (startLBlkno >= blocksInFile) {
	startLBlkno = 0;
    }

    i = startLBlkno;

    DirEntry *de;

    do {
        *block = dataPSO->readBlockCache(i, PSO_LOCAL);
	buf = (*block)->getData();

	// loop until we either find the entry or finish the block
	offset = 0;
	*prev = NULL;
	de = (DirEntry *)buf;
	while (de->getReclen() > 0) {
	    if (de->getOtokID() && de->nameLength == length &&
		!memcmp(de->name, fname, length)) {
		// found the entry
		break;
	    }

	    offset += de->getReclen();
	    *prev = de;
	    de = (DirEntry *)(buf + offset);
	}

	// check if we found an entry
	if (de->getOtokID() && de->getReclen()) {
	    goto found;
	}

	*entryp = NULL;
	dataPSO->freeBlockCache(*block);
	*block = NULL;

	if (++i >= blocksInFile) {
	    i = 0;
	}
    } while (i != startLBlkno);

    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR, "LSOBasicDir::matchEntry OUT\n");

    // no go
    return -1;

  found:
    *entryp = de;
    startLBlkno = i;

    return 0;
}

/*
 * findHole()
 *
 *  Attempt to locate a hole in the given directory block buffer of
 *  the requested size.  If a hole of the requested size cannot be
 *  found, an error is returned.
 */
sval
LSOBasicDir::findHole(char *buf, sval length, DirEntry **entryp)
{
    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR, "LSOBasicDir::findHole IN\n");

    sval entryLen;
    uval ptrLen, offset = 0;
    DirEntry *ptr;

    entryLen = GetPhysRecLen(length);

    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR_ENTRIES,
		"finding hole for entrylen=%lu\n", entryLen);

    // while we haven't hit the end of the block
    ptr = (DirEntry *)(buf + offset);
    while (ptr->getReclen() > 0) {
        // figure out the actual size of this entry
	if (ptr->getOtokID()) {
	    ptrLen = GetPhysRecLen(ptr->nameLength);
	} else {
	    ptrLen = 0;
	}

        KFS_DPRINTF(DebugMask::LSO_BASIC_DIR_ENTRIES,
		    "\tI:looking at %s, reclen=%u, physlen=%lu\n",
                    ptr->name, ptr->getReclen(), ptrLen);

        // check if there is space in between this record and the next
	if ((int)(ptr->getReclen() - ptrLen) >= entryLen) {
	    (*entryp) = (DirEntry *) ((char *)ptr + ptrLen);

	    (*entryp)->setReclen(ptr->getReclen() - ptrLen);
	    (*entryp)->nameLength = length;
	    if (ptr->getOtokID()) {
		ptr->setReclen(ptrLen);
	    }
	    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR,
			"LSOBasicDir::findHole OUT\n");
	    return 0;
	}
        offset += ptr->getReclen();
        ptr = (DirEntry *)(buf + offset);
    }

    // check if there is space at the end...
    if ((OS_BLOCK_SIZE - offset) > entryLen + GetPhysRecLen(0)) {
        // set the return entry
	ptr->setReclen(entryLen);
	ptr->nameLength = length;
        (*entryp) = ptr;

	// set next one to be 0
	ptr = (DirEntry *)(((uval)ptr) + ptr->getReclen());
	ptr->setReclen(0);

	return 0;
    }

    // couldn't find the space
    (*entryp) = NULL;
    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR, "LSOBasicDir::findHole OUT\n");
    return -1;
}

/*
 * matchDir()
 *
 *  Attempts to locate the object token ID of the given name in this
 *  directory.
 */
sval
LSOBasicDir::matchDir(const char *fname, sval length, ObjTokenID *otokID)
{
    lock.acquire();
    sval rc = locked_matchDir(fname, length, otokID);
    lock.release();
    return rc;
}

/*
 * locked_matchDir()
 *
 *  Attempts to locate the object token ID of the given name in this
 *  directory. This assumes the lock is held.
 */
sval
LSOBasicDir::locked_matchDir(const char *fname, sval length, ObjTokenID *otokID)
{
    _ASSERT_HELD(lock);

    sval rc;
    uval i;
    DirEntry *entry, *prev;
    BlockCacheEntry *block;

    rc = matchEntry(fname, length, &entry, &prev, &block, i);

    if (entry != NULL) {
	otokID->id = entry->getOtokID();
        rc = 0;
	tassertMsg(dataPSO != NULL, "?");
	dataPSO->freeBlockCache(block);
    } else {
        rc = -ENOENT;
    }
    return rc;
}

/*
 * matchIndex()
 *
 *   Locate the directory entry at the requested index.
 */
sval
LSOBasicDir::matchIndex(uval pos, uval *offset, DirEntry *entry /* =NULL */)
{
    lock.acquire();
    sval rc = locked_matchIndex(pos, offset, entry);
    lock.release();
    return rc;
}

/*
 * locked_matchIndex()
 *
 *   Locate the directory entry at the requested index, assuming lock is held.
 */
sval
LSOBasicDir::locked_matchIndex(uval pos, uval *offset,
			       DirEntry *entry /* =NULL */)
{
    _ASSERT_HELD(lock);

    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR,
		"LSOBasicDir::locked_matchIndex IN\n");

    DirEntry *ptr = NULL;
    BlockCacheEntry *block;
    uval blocknr;
    tassertMsg(dataPSO != NULL, "?");

    // calculate this entry's location
    blocknr = pos >> OS_BLOCK_SHIFT;
    *offset = pos & ~OS_BLOCK_MASK;

    block = dataPSO->readBlockCache(blocknr, PSO_LOCAL);
    ptr = (DirEntry *)(block->getData() + *offset);

    // try to find a "real" entry (non-tail), updating offset as we go along
    while ((ptr->getReclen() && !ptr->getOtokID()) ||
	   (!ptr->getReclen()
	    && lsoBuf->statGetSize() > (++blocknr)*OS_BLOCK_SIZE)) {

	if (ptr->getReclen() && !ptr->getOtokID()) {
	    *offset += ptr->getReclen();
	    ptr = (DirEntry *)((char *)ptr + ptr->getReclen());
	} else {
	    dataPSO->freeBlockCache(block);
	    block = dataPSO->readBlockCache(blocknr, PSO_LOCAL);
	    *offset = blocknr*OS_BLOCK_SIZE;
	    ptr = (DirEntry *)block->getData();
	}
    }

    if (!ptr->getReclen() || !ptr->getOtokID()) {
	/* couldn't find an entry */
	*offset = 0;
	dataPSO->freeBlockCache(block);
	KFS_DPRINTF(DebugMask::LSO_BASIC_DIR,
		    "LSOBasicDir::locked_matchIndex no go OUT\n");
	return -1;
    }

    // copy in the correct entry
    if (entry) {
	memcpy(entry, ptr, GetPhysRecLen(ptr->nameLength));
    }

    *offset = (blocknr << OS_BLOCK_SHIFT) | (*offset + ptr->getReclen());

    // update acess time, caller must flush this LSO when appropriate
    lsoBuf->statSetAtime(time(NULL));

    dataPSO->freeBlockCache(block);
    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR,
		"LSOBasicDir::locked_matchIndex found it OUT\n");
    return 0;
}

/*
 * getDents()
 *
 *  Retrieve directory entries starting at the specified "cookie"
 */
SysStatusUval
LSOBasicDir::getDents(uval &cookie, struct direntk42 *buf, uval len)
{
    // FIXME: This (as method GetMemRecLen) doesn't compile for KFS_TOOLS
    // because
    // struct direntk42 is not defined (no dirent64 in the host).
    // If we ever need it in the tools, we can fix this in a better way ....
#ifndef KFS_TOOLS
    uval bufUsed = 0, offset = 0;
    struct direntk42 *prev;
    DirEntry *ptr = NULL;
    BlockCacheEntry *block;
    uval blocknr;

    lock.acquire();

    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR,
		"LSOBasicDir::getDents() IN cookie=%lu\n", cookie);

    // calculate this entry's block number
    blocknr = cookie >> OS_BLOCK_SHIFT;

    tassertMsg(dataPSO != NULL, "?");

    prev = NULL;

    block = dataPSO->readBlockCache(blocknr, PSO_LOCAL);

    while (bufUsed < len) {
	// Go to next block?
	if (blocknr != (cookie >> OS_BLOCK_SHIFT)) {
	    dataPSO->freeBlockCache(block);

	    // re-calculate this entry's block number
	    blocknr = cookie >> OS_BLOCK_SHIFT;

	    block = dataPSO->readBlockCache(blocknr, PSO_LOCAL);
	}

	// re-calculate this entry's location
	offset = cookie & ~OS_BLOCK_MASK;

        // locate the proper directory entry
	ptr = (DirEntry *)(block->getData() + offset);

	// try to find a "real" entry (non-tail), updating offset as we go along
	while ((ptr->getReclen() && !ptr->getOtokID()) ||
	       (!ptr->getReclen()
		&& lsoBuf->statGetSize() > (++blocknr)*OS_BLOCK_SIZE)) {

	    if (ptr->getReclen() && !ptr->getOtokID()) {
		offset += ptr->getReclen();
		ptr = (DirEntry *)((char *)ptr + ptr->getReclen());
	    } else {
		dataPSO->freeBlockCache(block);
		block = dataPSO->readBlockCache(blocknr, PSO_LOCAL);
		offset = blocknr * OS_BLOCK_SIZE;
		ptr = (DirEntry *)block->getData();
	    }
	}

	if (!ptr->getReclen() || !ptr->getOtokID()) {
	    /* couldn't find an entry */
            if (prev != NULL) {
                prev->d_off = 0;
            }
            break; // or just get out!
        }

        // make sure there is adequate space in the buffer
        if (GetMemRecLen(ptr->nameLength) + bufUsed > len) {
            break;
        }

	offset = (blocknr << OS_BLOCK_SHIFT) | (offset + ptr->getReclen());

        // get the requested information
        buf->d_ino = ptr->getOtokID();
	buf->d_reclen = GetMemRecLen(ptr->nameLength);
	buf->d_off = offset;
        strncpy(buf->d_name, (char *)&(ptr->name), ptr->nameLength);
	buf->d_name[ptr->nameLength] = '\0';

#if defined DT_UNKNOWN && defined _DIRENT_HAVE_D_TYPE
	buf->d_type	  = DT_UNKNOWN;
#endif /* #if defined DT_UNKNOWN && defined ... */
#if defined _DIRENT_HAVE_D_NAMLEN
	buf->d_namlen      = ptr->nameLength;
#endif /* #if defined _DIRENT_HAVE_D_NAMLEN */

        KFS_DPRINTF(DebugMask::LSO_BASIC_DIR_ENTRIES,
		    "\tCookie %lu -> %s (ino=%lu, d_reclen=%u, d_off=%lu) "
		    "ncookie=%lu\n", cookie, buf->d_name, (uval) buf->d_ino,
		    buf->d_reclen, (uval) buf->d_off, offset);

	cookie = offset;

	bufUsed += buf->d_reclen;
        prev = buf;
        buf = (struct direntk42 *)((char *)buf + buf->d_reclen);
    }

    // update acess time, caller must flush this LSO when appropriate
    lsoBuf->statSetAtime(time(NULL));

    dataPSO->freeBlockCache(block);

    lock.release();
    KFS_DPRINTF(DebugMask::LSO_BASIC_DIR,
		"LSOBasicDir::getDents() OUT cookie=%lu\n", cookie);
    return _SRETUVAL(bufUsed);
#else
    passertMsg(0, "shouldn't be here\n");
    return 0;
#endif // #ifndef KFS_TOOLS
}

/*
 * deleteEntry()
 *
 *   Removes the given entry from the directory.  Also locates the
 *   file, decreases its link count, and returns the new value.
 */
sval
LSOBasicDir::deleteEntry(char *fname, sval length)
{
    sval rc;

    // lock and delete and entry
    lock.acquire();
    rc = locked_deleteEntry(fname, length);
    lock.release();

    return rc;
}

/*
 * locked_deleteEntry()
 *
 *   Deletes an entry assuming that the lock is held.  Also updates
 *   the link count of the associated file.
 */
sval
LSOBasicDir::locked_deleteEntry(char *fname, sval length)
{
    _ASSERT_HELD(lock);

    sval rc, i;
    uval j;
    DirEntry *entry, *prev;
    BlockCacheEntry *block = NULL;
    ObjTokenID otokID;
    LSOBasic *lso;
    tassertMsg(dataPSO != NULL, "?");

    // locate the directory entry, and the previous entry
    rc = matchEntry(fname, length, &entry, &prev, &block, j);
    i = j;

    // check that we found the entry
    if (entry == NULL) {
        // no such entry!
        return -1;
    }

    otokID.id = entry->getOtokID();

    // remove the entry
    if (prev) {
        // just expand the previous entry
	prev->setReclen(prev->getReclen() + entry->getReclen());
    } else {
	// prev here is actually "next". Bad choice of name :-/
        prev = (DirEntry *)(block->getData() + entry->getReclen());

	// check if this was the only entry in this block
	if (prev->getReclen() == 0) {
            //	    uval offset;

	    // Set the tail manually now. Since the PSO returns empty pages for
	    // reads on inexistant/freed blocks, we'll be ok later too
	    entry->setReclen(0);

	    // loop here to check if previous blocks are empty and should
	    // also be freed
            // The following is commented out for now. Without this we are
	    // "leaking" unused data blocks, just like ext2.
/*
	    while (prev->getReclen() == 0) {
		offset = i*OS_BLOCK_SIZE;

		// free this block we aren't using anymore
		pso->freeBlocks(offset, offset + OS_BLOCK_SIZE - 1);

		// Was this the last block?
		if (lsoBuf->stat.getSize() == offset + OS_BLOCK_SIZE) {
		    // We must *only* update these if this block was the last.
		    // If it's not then we are creating a hole in the PSO,
                    //  which is
		    // ok, but we must *not* update these, or else we'll make
		    // some entries (those in the last block) inaccessible
		    lsoBuf->stat.setSize(lsoBuf->stat.getSize() - OS_BLOCK_SIZE);
		    lsoBuf->stat.setBlocks(lsoBuf->stat.getBlocks() - (OS_BLOCK_SIZE / OS_SECTOR_SIZE));
		}
		block->markClean();
		pso->freeBlockCache(block);
		// we can safely destroy 'i' and 'buf' here
		block = pso->readBlockCache(--i, PSO_LOCAL);
		prev = (DirEntry *)block->getData();
		}
	    i = -1;
*/
	}
	else {
	    // deleting first entry...
	    // just invalidate the sucker by zeroing its ID
	    entry->setOtokID(0);
	}
    }

    // decrease the link count and flush
    lso = (LSOBasic *)getLocalRecordMap()->getObj(&otokID);
    tassertMsg(lso != NULL, "?");
    lso->unlink();

    // update the directory attributes
    uval64 t = time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);

    markDirty();

    // have to write back the data block (only if the block wasn't freed)
    if (i >= 0) {
	dataPSO->writeBlockCache(block, i);
    }

    if (block) {
	dataPSO->freeBlockCache(block);
    }
    return 0;
}

sval
LSOBasicDir::updateEntry(char *fname, sval length, ObjTokenID *otokID)
{
    sval rc = 0;
    uval i;
    DirEntry *entry, *prev;
    BlockCacheEntry *block = NULL;
    ObjTokenID oldTokID;

    lock.acquire();
    tassertMsg(dataPSO != NULL, "?");

    // locate the directory entry, and the previous entry
    rc = matchEntry(fname, length, &entry, &prev, &block, i);

    // check that we found the entry
    if (entry == NULL) {
	tassertMsg(0, "?");
        // no such entry!
        lock.release();
        return -1;
    }

    // Subtle! We have to unlink the old link (the inode we are
    // releasing) _before_ updating the entry!
    oldTokID.id = entry->getOtokID();
    LSOBasic *lsoOld = (LSOBasic *)getLocalRecordMap()->getObj(otokID);
    tassertMsg(lsoOld != NULL, "?");
    lsoOld->unlink();

    // update the entry
    entry->setOtokID(otokID->id);

    // don't forget to update link count! (we're actually creating a
    // new link!)
    LSOBasic *lsoNew = (LSOBasic *)getLocalRecordMap()->getObj(otokID);
    tassertMsg(lsoNew != NULL, "?");
    lsoNew->link();
    //lsoNew->flush();

    // update the directory attributes
    uval64 t = time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);

    markDirty();

    // have to write back the data block
    dataPSO->writeBlockCache(block, i);

    lock.release();
    dataPSO->freeBlockCache(block);
    return rc;
}

/*
 * createEntry()
 *
 *   Creates an entry with the given filename and object token id.  It
 *   also increases the link count of the object token id by 1 when
 *   creating a hard link.
 */
sval
LSOBasicDir::createEntry(char *fname, sval length, ObjTokenID *otokID,
                         sval mode, sval uid)
{
    LSOBasic *lso;
    sval rc;

    // lock and create an entry
    lock.acquire();
    rc = locked_createEntry(fname, length, otokID, mode, uid);
    lock.release();

    _IF_FAILURE_RET(rc);

    // We don't need the dir lock to link the child
    RecordMapBase *rmap;
    if (memcmp(fname, ".", length) && memcmp(fname, "..", length)) {
	rmap = getLocalRecordMap();
    } else {
	if (!memcmp(fname, "..", length)) {
	    // Crap! Don't know how to deal with this case :-(
	    // The problem here is how do we get access to the "parent" LSO
	    // without having a reference to it's recordMap?
	    return rc;
	}
	rmap = getRecordMap();
    }

    lso = (LSOBasic *)rmap->getObj(otokID);
    tassertMsg(lso != NULL, "?");
    // increase link count
    lso->link();

    return rc;
}

/*
 * locked_createEntry()
 *
 *   Creates an entry assuming that the lock is held.
 */
sval
LSOBasicDir::locked_createEntry(char *fname, sval length, ObjTokenID *otokID,
				sval mode, sval uid)
{
    _ASSERT_HELD(lock);

    sval i, blocksInFile, rc;
    DirEntry *entry = NULL;
    BlockCacheEntry *block = NULL;

    tassertMsg(dataPSO != NULL, "?");

    // Add entry to directory listing.
    blocksInFile = (lsoBuf->statGetSize() + OS_BLOCK_SIZE - 1) / OS_BLOCK_SIZE;
    for (i = 0; i < blocksInFile; i++) {
        block = dataPSO->readBlockCache(i, PSO_LOCAL);

	// Find an empty spot that is big enough to record file name & info
	rc = findHole(block->getData(), length, &entry);
	if (entry != NULL) {
	    break;
	}
	dataPSO->freeBlockCache(block);
    }

    // if we couldn't find a hole, create a new block
    if (entry == NULL) {
	// reading the next (i-th) block, will allocate it.
	block = dataPSO->readBlockCache(i, PSO_LOCAL);

        lsoBuf->statSetSize(lsoBuf->statGetSize() + OS_BLOCK_SIZE);
        lsoBuf->statSetBlocks(lsoBuf->statGetBlocks()
			       + OS_BLOCK_SIZE / OS_SECTOR_SIZE);

        // set the pointer to the first "entry"
	entry = (DirEntry *)block->getData();
	entry->setReclen(GetPhysRecLen(length));

	// set a null entry
	entry = (DirEntry *)((char *)entry + entry->getReclen());
	entry->setOtokID(0);
	entry->nameLength = 0;
	entry->setReclen(0);

	// reset ptr
	entry = (DirEntry *)block->getData();
    }

    // Copy name length to directory entry
    entry->nameLength = length;
    // Copy name itself to directory entry
    memcpy(entry->name, fname, length);

    // Copy object id number to directory entry
    // note: copying only the id value of ObjTokenID.  Not copying
    //       otokID.dPart because disk position may change in future.
    entry->setOtokID(otokID->id);

    // update the directory attributes
    uval64 t = time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);

    markDirty();

    // write out the data
    dataPSO->writeBlockCache(block, i);
    dataPSO->freeBlockCache(block);
    return 0;
}

/*
 * createDir()
 *
 *   Creates a directory entry that points to a directory.
 */
sval
LSOBasicDir::createDir(char *fname, sval length, sval mode, sval uid,
                       ObjTokenID *dtokID)
{
    sval rc;
    LSOBasicDir *lso;

    lock.acquire();

    tassertMsg(S_ISDIR(lsoBuf->statGetMode()), "should be a dir.\n");

    // First, create an entry!
    rc = locked_createEntry(fname, length, dtokID,
			    S_IFDIR | (mode & ~S_IFMT), uid);
    if (_FAILURE(rc)) {
        lock.release();
        return rc;
    }

    // update the parent's time stamps
    uval64 t = time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);

    // update parent link count
    locked_link();

    markDirty();

    lock.release();

    // For here on, only touching the "child", so lock is dropped
    //    lso = (LSOBasicDir *)dtok->getObj(NULL);
    lso = (LSOBasicDir *)getLocalRecordMap()->getObj(dtokID);
    tassertMsg(lso != NULL, "?");

    // now create the '.' and '..' directories
    lso->createEntry(".", 1, dtokID, S_IFDIR | (mode & ~S_IFMT), uid);
    lso->createEntry("..", 2, &id, S_IFDIR | (mode & ~S_IFMT), uid);
    // Directories need an extra link
    lso->link();
    lso->link();

    return 0;
}

/*
 * deleteDir()
 *
 *   Removes a directory as well as the directory's name entry in its
 *   parent directory.
 */
sval
LSOBasicDir::deleteDir(char *fname, sval length)
{
    sval rc;
    uval offset;
    ObjTokenID otokID;
    LSOBasicDir *dir;

    lock.acquire();

    // Make sure the directory we are about to delete is "empty"
    // First, find the object for `fname` dir
    if ((rc = locked_matchDir(fname, length, &otokID))) {
	lock.release();
        return rc;
    }

    dir = (LSOBasicDir *)getLocalRecordMap()->getObj(&otokID);
    tassertMsg(dir != NULL, "?");

    // Second: check if there isn't any entries inside `dir`
    // We should skip '.' and '..'
    // Get '.'
    dir->matchIndex(0, &offset);
    // Get '..'
    dir->matchIndex(offset, &offset);

    if (!dir->matchIndex(offset, &offset)) {
        // If we get here, 'fname' is _not_ empty
        lock.release();
        return _SERROR(2347, 0, ENOTEMPTY);
    }

    // now delete as a file
    if ((rc = locked_deleteEntry(fname, length))) {
	passertMsg(0, "LSOBasicDir::deleteDir() deleteEntry() failed!\n");
    }

    // decrement the parent's link count
    locked_unlink();

    // we have to unlink the removed dir twice (for '.' and '..') so
    // deleteFile can be called (it was unlinked once in deleteEntry)
    dir->unlink();

    lock.release();
    return rc;
}

/*
 * createRecord()
 *
 *   Auxiliary method for file creation. This creates a new record of 
 *   type 'type' in the RecordMap and stores the ObjToken in 'newTok'
 */
SysStatus
LSOBasicDir::createRecord(ObjTokenID *newTokID, PsoType type)
{
    SysStatus rc;

    rc = getLocalRecordMap()->allocRecord(type, newTokID);
    if (_FAILURE(rc)) {
	tassertMsg(0, "FSFileKFS::createRecord() problem with record\n");
	return rc;
    } 

    return rc;
}

/*
 * deleteRecord()
 */
void
LSOBasicDir::deleteRecord(ObjTokenID *offset)
{
    getLocalRecordMap()->freeRecord(offset);
}

/* static */ uval
LSOBasicDir:: GetMemRecLen(uval lengthString)
{
    // FIXME: This (as method getDents) doesn't compile for KFS_TOOLs because
    // struct direntk42 is not defined (no dirent64 in the host).
    // If we ever need it in the tools, we can fix this in a better way ....
#ifndef KFS_TOOLS
    uval recLen = sizeof(struct direntk42) - 256; // FIXME: HARD-CODED :(
    recLen += lengthString + 1;
    recLen = ALIGN_UP(recLen, sizeof(uval32));
    return recLen;
#else
    passertMsg(0, "shouldn't be here\n");
    return 0;
#endif // #ifndef KFS_TOOLS
}

/*
 * clone()
 *
 *   Creates a new LSOBasicDir from the given RecordMap entry.
 */
/* virtual */ ServerObject *
LSOBasicDir::clone(ObjTokenID *otokID, RecordMapBase *r)
{
    LSOBasicDir *lso = new LSOBasicDir(otokID, r, globals);
    lso->init();

    return lso;
}
