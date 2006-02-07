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
 * $Id: LSODirEmb.C,v 1.2 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"

#include "LSODirEmb.H"
#include "GlobalRecordMap.H"
#include "SuperBlock.H"

void
LSODirEmb::init()
{
    LSOBasicDir::init();
    LSODirEmbStruct *embData = (LSODirEmbStruct *)lsoBuf;
    ObjTokenID recordID = embData->getRecordMapID();//{embData->getRecordMapID()};
    localRecordMap = new LocalRecordMap(embData->getRecordMapBlk(), 
					recordID, globals);
    localRecordMap->init();
    //    err_printf("LSODirEmb::init() LocalRecordMapID = %u, 0x%p\n",
    //	       recordID.id, localRecordMap);
}

LSODirEmb::~LSODirEmb()
{
    delete localRecordMap;
}

/*
 * createRecord()
 *
 *   Auxiliary method for file creation. This creates a new record of 
 *   type 'type' in the RecordMap and stores the ObjToken in 'newTok'
 */
SysStatus
LSODirEmb::createRecord(ObjTokenID *newTokID, PsoType type)
{
    SysStatus rc;

    rc = localRecordMap->allocRecord(type, newTokID);
    if (_FAILURE(rc)) {
	tassertMsg(0, "LSODirEmb::createRecord() problem with record\n");
	return rc;
    }
    return rc;
}

/*
 * deleteFile()
 *
 *   Remove the file, freeing all used disk space.
 */
SysStatus
LSODirEmb::deleteFile()
{
    localRecordMap->unlink();

    lock.acquire();
    // have to delete our LocalRecordMap
    // If we are using this LSO, we can safely assume that the global
    // one is of type "GlobalRecordMap"
    //    GlobalRecordMap *globalRecordMap = (GlobalRecordMap *)globals->recordMap;

    // allocate a logical ID for the local recordMap
    //    globalRecordMap->freeLocalRecordMap(((LSODirEmbStruct *)lsoBuf)->
    //	getRecordMapID());
    lock.release();
    return LSOBasicDir::deleteFile();
}

/*
 * locked_flush()
 *
 *   Flushes the LSO assuming the lock is held.
 */
void
LSODirEmb::locked_flush()
{
    _ASSERT_HELD(lock);
    localRecordMap->flush();
    LSOBasicDir::locked_flush();
}

/*
 * clone()
 *
 *   Creates a new LSODirEmb from the given RecordMap entry.
 */
/* virtual */ ServerObject *
LSODirEmb::clone(ObjTokenID* otokID, RecordMapBase *r)
{
    LSODirEmb *lso = new LSODirEmb(otokID, r, globals);
    lso->init();

    return lso;
}

SysStatusUval
LSODirEmb::locationAlloc(ObjTokenID *localID, RecordMapBase *recordMap)
{
    uval32 blkno; // , globalID;
    //   SysStatus rc;
    // If we are using this LSO, we can safely assume that the global
    // one is of type "GlobalRecordMap"
    //    GlobalRecordMap *globalRecordMap = (GlobalRecordMap *)globals->recordMap;

    // zero out new buffer
    memset(lsoBufData, 0, KFS_RECORD_SIZE);
    LSODirEmbStruct *lsoBuf = (LSODirEmbStruct *)lsoBufData;

    //    // allocate a logical ID for the local recordMap
    //    rc = globalRecordMap->allocLocalRecordMap(&globalID);
    //    if (_FAILURE(rc)) {
    //	return rc;
    //    }
    //    lsoBuf->setRecordMapID(globalID);

    // allocate a physical block for the local recordMap
    blkno = LocalRecordMap::locationAlloc(globals); //globals->super->allocBlock();
    if (!blkno) {
	// FIXME! SuperBlock::allocBlock() should return some
	// error to pass up. Something like -ENOSPC
	return -1;
    }
    lsoBuf->setRecordMapBlk(blkno);

    recordMap->setRecord(localID, lsoBufData);

    return 0;
}
