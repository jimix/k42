/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br).
 *
 * $Id: LSOBasic.C,v 1.6 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"

#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <string.h>

#include "LSOBasic.H"
#include "LSOBasicSymlink.H"
#include "PSOTypes.H"
#include "KFSGlobals.H"
#include "FSFileKFS.H"

/*
 * LSOBasic()
 *
 *   Standard constructors for the basic LSO class
 */
LSOBasic::LSOBasic(KFSGlobals *g) : LSOBase(g), dataPSO(NULL) {}

LSOBasic::LSOBasic(ObjTokenID *otokID, RecordMapBase *r, KFSGlobals *g) :
    LSOBase(otokID, r, g), dataPSO(NULL) { }

void
LSOBasic::init()
{
    // read the LSO record
    getRecordMap()->getRecord(&id, lsoBufData);
    lsoBuf = (LSOBasicStruct*) &lsoBufData;

    // set the data token
    ObjTokenID dataID = lsoBuf->getDataID();
    dataPSO = (PSOBase *)recordMap->getObj(&dataID);
    //    passertMsg(0, "Hey there!\n");
}

LSOBasic::~LSOBasic()
{
    KFS_DPRINTF(DebugMask::LSO_BASIC,
		"LSOBasic::~LSOBasic() id=%llu IN\n", id.id);

    if (dataPSO) {
	delete dataPSO;
    }

    KFS_DPRINTF(DebugMask::LSO_BASIC,
		"LSOBasic::~LSOBasic() id=%llu OUT\n", id.id);
}

/*
 * chown()
 *
 *   Set the ownership of the file.
 */
void
LSOBasic::chown(uval32 uid, uval32 gid)
{
    lock.acquire();

    if (uid != uval32(~0)) {
	lsoBuf->statSetUid(uid);
    }
    if (gid != uval32(~0)) {
	lsoBuf->statSetGid(gid);
    }
    lsoBuf->statSetCtime(time(NULL));

    markDirty();

    lock.release();
}

/*
 * chmod()
 *
 *   Change the mode of the file.
 */
void
LSOBasic::chmod(uval mode)
{
    lock.acquire();
    lsoBuf->statSetMode(mode);
    lsoBuf->statSetCtime(time(NULL));

    markDirty();

    lock.release();
}

/*
 * utime()
 *
 *  Sets the access and modification time of the file.  If the
 *  requested utbuf is NULL, then the times are set to the current
 *  time.
 */
void
LSOBasic::utime(const struct utimbuf *utbuf)
{
    lock.acquire();

    if (utbuf == NULL) {
	uval64 t = time(NULL);
	lsoBuf->statSetCtime(t);
	lsoBuf->statSetMtime(t);
	lsoBuf->statSetAtime(t);
    } else {
	lsoBuf->statSetAtime(utbuf->actime);
	lsoBuf->statSetMtime(utbuf->modtime);
	lsoBuf->statSetCtime(time(NULL));
    }
    markDirty();

    lock.release();
}

/*
 * link()
 *
 *   Increase the link count on this file.
 */
void
LSOBasic::locked_link()
{
    _ASSERT_HELD(lock);

    // just increase the link count
    lsoBuf->statSetNlink(getNumLinks() + 1);

    // update the modification times
    uval64 t = time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);

    markDirty();
}

/*
 * unlink()
 *
 *   Decrease the link count on this file.
 */
void
LSOBasic::locked_unlink()
{
    _ASSERT_HELD(lock);

    // just decrease the link count!
    lsoBuf->statSetNlink(getNumLinks() - 1);

    // update the change time
    lsoBuf->statSetCtime(time(NULL));

    markDirty();
}

/*
 * deleteFile()
 *
 *   Remove the file, freeing all used disk space.
 */
SysStatus
LSOBasic::deleteFile()
{
    lock.acquire();

    KFS_DPRINTF(DebugMask::LSO_BASIC,
		"LSOBasic::deleteFile() IN this=0x%p\n", this);

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
    if (dataPSO != NULL) {
        dataPSO->unlink();
    }

    // free the record
    getRecordMap()->freeRecord(&id);
    //recPSO->flush();

    KFS_DPRINTF(DebugMask::LSO_BASIC,
		"LSOBasic::deleteFile() OUT this=0x%p\n", this);

    lock.release();
    return 0;
}

/*
 * getAttribute()
 *
 *   Retrieve the attributes of this file and fill them into the given
 *   status buffer.
 */
void
LSOBasic::getAttribute(KFSStat *status)
{
    lock.acquire();

    lsoBuf->copyStatTo(status);

    lock.release();
}

/*
 * initAttribute()
 *
 *   Initialize the attributes of the file given the uid and mode of
 *   the file.
 */
SysStatus
LSOBasic::initAttribute(uval mode, uval uid, uval gid)
{
    //    ObjTokenID otokID = {0};
    sval rc = 0;

    lock.acquire();

    // We used to allocate a record entry for this file's PSO.
    // Now we do a lazy allocation, so that it is only initialized
    // during the very first write.
    //    dataPSO->setID(&otokID);

    lsoBuf->statSetIno(id.id);
    lsoBuf->statSetMode(mode);
    //lsoBuf->statSetMode(type | (mode & ~S_IFMT));

    lsoBuf->statSetNlink(0);
    lsoBuf->statSetUid(uid);
    lsoBuf->statSetGid(gid);

    lsoBuf->statSetRdev(0);
    lsoBuf->statSetSize(0);
    lsoBuf->statSetBlksize(OS_BLOCK_SIZE);
    lsoBuf->statSetBlocks(0);

    uval64 t = time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);
    lsoBuf->statSetAtime(t);

    markDirty();
    lock.release();
    return rc;
}

/*
 * readBlock()
 *
 *   Read the logical block requested and return it in the given
 *   buffer.  Also update the access times for this file.
 */
sval
LSOBasic::readBlock(uval32 lblkno, char *buffer, uval local,
		    uval isPhysAddr /* = 0 */)
{
    sval ret = 0;
    lock.acquire();

    if (dataPSO == NULL) {
	// no PSO has been allocated for this LSO yet. Return empty page.
	if (isPhysAddr) { // can't memset a phys addr (only for K42)
#ifndef KFS_TOOLS // so we don't have to include FSFileKFS for tools ...
	    ret = FSFileKFS::PageNotFound();
#endif // #ifndef KFS_TOOLS
	} else {
	    memset(buffer, 0, OS_BLOCK_SIZE);
	}
    } else {
	ret = dataPSO->readBlock(lblkno, buffer, local, isPhysAddr);
    }

    // update the access times
    lsoBuf->statSetAtime(time(NULL));

    markDirty();

    lock.release();

    return ret;
}

/*
 * writeBlock()
 *
 *   Write the logical block requested from the given buffer.  Also
 *   update the access times for this file, and possibly the file
 *   size.
 */
sval
LSOBasic::writeBlock(uval64 newSize, uval32 lblkno, char *buffer, uval local)
{
    sval rc = 0;

    lock.acquire();

    // Check if we need to allocate a PSO for data
    if (dataPSO == NULL) {
	ObjTokenID otokID = {0};

	// allocate a data object
	rc = getRecordMap()->allocRecord(OT_PRIM_UNIX, &otokID);
	if (_FAILURE(rc)) {
	    KFS_DPRINTF(DebugMask::LSO_BASIC,
			"LSOBasic::writeBlock() problem creating recordMap"
			" entry\n");
	    return rc;
	}

	dataPSO = (PSOBase *)recordMap->getObj(&otokID);
    }

    // write and update the access times
    rc = dataPSO->writeBlock(lblkno, buffer, local);
    if (_FAILURE(rc)) {
	lock.release();
	return rc;
    }

    uval64 t = time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);

    markDirty();

    // possibly update the file size
    if (newSize > (uval64)lsoBuf->statGetSize()) {
	lsoBuf->statSetSize(newSize);
	lsoBuf->statSetBlocks(ALIGN_UP(newSize,
				   OS_BLOCK_SIZE) / OS_SECTOR_SIZE);
    }

    lock.release();
    return rc;
}

/*
 * flush()
 *
 *   Flushes the logical server object to disk.
 */
void
LSOBasic::flush()
{
    // lock and flush
    lock.acquire();
    locked_flush();
    lock.release();
}

/*
 * locked_flush()
 *
 *   Flushes the LSO assuming the lock is held.
 */
void
LSOBasic::locked_flush()
{
    _ASSERT_HELD(lock);

    if (isDirty()) {
	markClean();

	// write back this object's id
	if (dataPSO) {
	    lsoBuf->setDataID(dataPSO->getID());
	}

	// then flush the object related state
	getRecordMap()->setRecord(&id, lsoBufData);
    }

    // flush the data to disk (but don't pull it off disk to flush it!)
    if (dataPSO != NULL) {
        dataPSO->flush();
    }
}

/*
 * clone()
 *
 *   Creates a new LSOBasic from the given RecordMap entry.
 */
/* virtual */ ServerObject *
LSOBasic::clone(ObjTokenID *otokID, RecordMapBase *r)
{
    LSOBasic *lso = new LSOBasic(otokID, r, globals);
    lso->init();

    return lso;
}

/*
 * locationAlloc()
 *
 *   Allocates space for a PSOBasicRW and returns its location.
 */
SysStatus
LSOBasic::locationAlloc(ObjTokenID *id, RecordMapBase *recordMap)
{
    // zero out new buffer
    memset(lsoBufData, 0, KFS_RECORD_SIZE);
    recordMap->setRecord(id, lsoBufData);

    return 0;
}
