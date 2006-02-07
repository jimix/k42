/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br).
 *
 * $Id: LSOBasic.C,v 1.73 2005/08/04 21:00:41 dilma Exp $
 *****************************************************************************/

#include "kfsIncs.H"

#ifndef KFS_TOOLS
#include "FSFileKFS.H"
#include "ServerFileBlockKFS.H"
#endif // #ifndef KFS_TOOLS

#include <time.h>
#include <utime.h>
#include <sys/stat.h>
#include <string.h>

#include "LSOBasic.H"
#include "LSOBasicSymlink.H"
#include "PSOTypes.H"
#include "KFSGlobals.H"

/*
 * LSOBasic()
 *
 *   Standard constructors for the basic LSO class
 */
LSOBasic::LSOBasic(KFSGlobals *g) : LSOBase(g), data(g) {}

LSOBasic::LSOBasic(ObjTokenID otokID, FSFileKFS *f, KFSGlobals *g) :
    LSOBase(otokID, f, g), data(g) { }

void
LSOBasic::init()
{
    // read the LSO record
    globals->recordMap->getRecord(id, lsoBufData);
    lsoBuf = (LSOBasicStruct*) &lsoBufData;

    // set the data token
    data.setID(lsoBuf->getDataID());
}

LSOBasic::~LSOBasic()
{
    KFS_DPRINTF(DebugMask::LSO_BASIC,
		"LSOBasic::~LSOBasic() id=%u IN\n", id.id);

    PSOBase *obj = (PSOBase *)data.gobj();

    if (obj) {
	// FIXME: we still have the race betweeing the server object
	// going away and a flush() starting on it
	ServerObject::DirtySONode *dNode = obj->getDirtyNode();
	if (dNode) {
	    dNode->invalidate();
	    obj->setDirtyNode(NULL);
	}
	delete obj;
    }

    KFS_DPRINTF(DebugMask::LSO_BASIC,
		"LSOBasic::~LSOBasic() id=%u OUT\n", id.id);
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
    uval oldMode = lsoBuf->statGetMode();

    lsoBuf->statSetMode((mode & LSOStat::SIALLUGO)
			| (oldMode & ~LSOStat::SIALLUGO));
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

    if(utbuf == NULL) {
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

    tassertMsg(lsoBuf != NULL, "look\n");
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

    if (dirtyNode) {
	dirtyNode->invalidate();
	dirtyNode = NULL;
    }

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
    PSOBase *dataPSO = (PSOBase *)data.getObj(fsfile);
    if (dataPSO != NULL) {
	// FIXME: it seems the code below should be in the unlink method
	// (it would be more elegant)
	ServerObject::DirtySONode *dn = dataPSO->getDirtyNode();
	// FIXME: race condition
	if (dn) {
	    dn->invalidate();
	    dataPSO->setDirtyNode(NULL);
	}
        dataPSO->unlink();
    }

    // free the record
    globals->recordMap->freeRecord(&id);
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
    ObjTokenID otokID = {0};
    sval rc = 0;

    lock.acquire();

    // We used to allocate a record entry for this file's PSO.
    // Now we do a lazy allocation, so that it is only initialized
    // during the very first write.
    data.setID(otokID);

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
LSOBasic::readBlock(uval32 lblkno, char *buffer,
		    PSOBase::AsyncOpInfo *cont /* = NULL */,
		    uval isPhysAddr /* = 0 */)
{
    PSOBase *pso;
    sval ret = 0;
    lock.acquire();

    // get the data pso and read the block
    pso = (PSOBase *)data.getObj(fsfile);
    if (pso == NULL) {
	// no PSO has been allocated for this LSO yet. Return empty page.
	if (isPhysAddr) { // can't memset a phys addr (only for K42)
#ifndef KFS_TOOLS // so we don't have to include FSFileKFS for tools ...
	    ret = FSFileKFS::PageNotFound();
#endif // #ifndef KFS_TOOLS
	} else {
	    memset(buffer, 0, OS_BLOCK_SIZE);
	}
#ifndef KFS_TOOLS // so we don't include ServerFileBlockKFS for tools ...
	// trigger continuation
	ServerFileBlockKFS::CompleteIORequest(cont->obj,
					      cont->addr, cont->offset,
					      ret);
#endif // #ifndef KFS_TOOLS
    } else {
	ret = pso->readBlock(lblkno, buffer, cont, isPhysAddr);
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
LSOBasic::writeBlock(uval64 newSize, uval32 lblkno, char *buffer,
		     PSOBase::AsyncOpInfo *cont /* = NULL */) 
{
    sval rc = 0;
    
    AutoLock<BLock> al(lock);

    // get the data pso and read the block
    PSOBase *pso = (PSOBase *)data.getObj(fsfile);

    // Check if we need to allocate a PSO for data
    if (pso == NULL) {
	ObjTokenID otokID = {0};

	// allocate a data object
	rc = globals->recordMap->allocRecord(OT_PRIM_UNIX, &otokID);
	if (_FAILURE(rc)) {
	    KFS_DPRINTF(DebugMask::LSO_BASIC,
			"LSOBasic::writeBlock() problem creating recordMap"
			" entry\n");
#ifndef KFS_TOOLS // so we don't include ServerFileBlockKFS for tools ...
	    // trigger continuation
	    ServerFileBlockKFS::CompleteIORequest(cont->obj,
						  cont->addr, cont->offset,
						  rc);
#endif // #ifndef KFS_TOOLS
	    return rc;
	}

	data.setID(otokID);
	pso = (PSOBase *)data.getObj(fsfile);
    }

    // write and update the access times
    rc = pso->writeBlock(lblkno, buffer, cont);
    _IF_FAILURE_RET(rc);

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

    PSOBase *dataPSO;

    if (isDirty()) {
	markClean();

	// write back this object's id
	lsoBuf->setDataID(data.getID());

	// then flush the object related state
	globals->recordMap->setRecord(id, lsoBufData);
    }

    // flush the data to disk (but don't pull it off disk to flush it!)
    dataPSO = (PSOBase *)data.gobj();
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
LSOBasic::clone(ObjTokenID otokID, FSFileKFS *f)
{
    LSOBasic *lso = new LSOBasic(otokID, f, globals);
    lso->init();

    return lso;
}

/*
 * locationAlloc()
 *
 *   Allocates space for a PSOBasicRW and returns its location.
 */
SysStatus
LSOBasic::locationAlloc(ObjTokenID _id)
{
    // zero out new buffer
    memset(lsoBufData, 0, KFS_RECORD_SIZE);
    globals->recordMap->setRecord(_id, lsoBufData);
    //map->flush();

    return 0;
}
