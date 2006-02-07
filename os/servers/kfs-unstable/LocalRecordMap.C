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
 * $Id: LocalRecordMap.C,v 1.2 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "LocalRecordMap.H"
#include "KFSGlobals.H"
#include "PSOTypes.H"
#include "ObjToken.H"
#include "SuperBlock.H"
#include "FSFileKFS.H"
#include "GlobalRecordMap.H"

void
LocalRecordMap::init()
{
    RecordMap::init();

    //    if (globalID.id) {
    //	// If we are using this RecordMap, we can safely assume that the global
    //	// one is of type "GlobalRecordMap"
    //	GlobalRecordMap *globalRecordMap = (GlobalRecordMap *)globals->recordMap;
    //	// Register itself onto the Global RecordMap hash table
    //	globalRecordMap->registerLocalRecordMap(globalID, this);
    //    }
}

void
LocalRecordMap::unregisterFromGlobal()
{
    //    if (globalID.id) {
    //	// If we are using this RecordMap, we can safely assume that the global
    //	// one is of type "GlobalRecordMap"
    //	GlobalRecordMap *globalRecordMap = (GlobalRecordMap *)globals->recordMap;
    //	// Register itself onto the Global RecordMap hash table
    //	globalRecordMap->unregisterLocalRecordMap(globalID);
    //    }
    //    globalID.id = 0;
}

LocalRecordMap::~LocalRecordMap()
{
    unregisterFromGlobal();
}

/*
 * allocRecord()
 *
 *   Allocates the requested number of records in a sequential run.
 *   It returns an offset to the start of the first record.
 */
SysStatus
LocalRecordMap::allocRecord(PsoType type, ObjTokenID *offset)
{
    sval rc;

    rc = internal_allocRecord(type, offset);

    //    err_printf("LocalRecordMap::allocrecord(0x%p) for type %d got id %ld\n",
    //	       this, (uval32) type, (uval) offset->id);

    // We have to "prepend" this ID with the RecordMap ID
    offset->id = localIDtoObjTokenID(offset->id);

    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::RECORD_MAP,
		    "LocalRecordMap::allocRecord() Problem allocating record"
		    " type=%u\n", type);
	// release the entries we've allocated
	freeRecord(offset);
	return rc;
    }

    KFS_DPRINTF(DebugMask::RECORD_MAP, "AllocRecord for type %d got id %ld\n",
		(uval32) type, (uval) offset->id);

    //    err_printf("LocalRecordMap::allocrecord(0x%p) for type %d got id %ld\n",
    //	       this, (uval32) type, (uval) offset->id);

    // write back the ServerObject type for this record
    setRecordType(offset, type);

    // Get location for this ServerObject
    // figure out what the location should be based on the type
    rc = globals->soAlloc->locationAlloc(type, offset, this);
    if (_FAILURE(rc)) {
	//        err_printf("LocalRecordMap::allocRecord() Problem allocating ServerObject"
	//		   " type=%u\n", type);
	passertMsg(0, "here\n");
        // release the entries we've allocated
        freeRecord(offset);
        return rc;
    }

    return rc;
}

/*
 * freeRecord()
 *
 *   Deallocates the requested record.
 */
sval
LocalRecordMap::freeRecord(ObjTokenID *offset)
{
    sval rc;
    ObjTokenID localID;

    localID.id = objTokenIDToLocalID(offset->id);
    //    err_printf("LocalRecordMap(0x%p)::freeRecord(%u) got id %u\n",
    //	       this, offset->id, localID.id);

    // RACE! If we wait for this id to be removed from the hash during the
    // ServerObject destructor, the same id can be allocated to another object
    // and ObjToken::getObj() might return a stale (and incorrect) object.
    // So we do it here, before physically deallocating this id.
    KFSHashEntry<ServerObject*> *entry;
    uval ret = soHash.findLock(offset->id, &entry);
    if (ret) {
	soHash.removeUnlock(entry);
    }

    rc = internal_freeRecord(&localID);

    // RACE! If we keep this object's ID unchanged, it will be removed
    // again in the ServerObject destructor. There is a very subtle
    // race where a new object allocates this ID (due to the fact that
    // we've just freed this entry), and adds itself to the
    // soHash. Then this object calls the ServerObject destructor
    // removing this id (which is owned by *another* object, and
    // should not be removed!). I think setting this to zero is the
    // "right thing to do", because this ID is not available neither
    // as an in-memory object, or a "physical" ORSMap entry.
    offset->id = 0;

    return 0;
}

/*
 * getRecord()
 *
 *   Returns a record of the given size from the given offset
 */
sval
LocalRecordMap::getRecord(ObjTokenID *offset, char *rec)
{
    ObjTokenID localID = { objTokenIDToLocalID(offset->id) };
    return RecordMap::getRecord(&localID, rec);
}

/*
 * setRecord()
 *
 *   Sets a record of the given size at the given offset
 */
sval
LocalRecordMap::setRecord(ObjTokenID *offset, char *rec)
{
    ObjTokenID localID = {objTokenIDToLocalID(offset->id)};
    return RecordMap::setRecord(&localID, rec);
}

/*
 * getRecordType()
 *
 *   Returns the type of a record at the given offset
 */
PsoType
LocalRecordMap::getRecordType(ObjTokenID *offset)
{
    ObjTokenID localID = {objTokenIDToLocalID(offset->id)};
    return RecordMap::getRecordType(&localID);
}

/*
 * setRecordType()
 *
 *   Sets the type of a record at the given offset
 */
sval
LocalRecordMap::setRecordType(ObjTokenID *offset, PsoType type)
{
    ObjTokenID localID = {objTokenIDToLocalID(offset->id)};
    return RecordMap::setRecordType(&localID, type);
}

/*
 * unlink()
 */
/* virtual */ void
LocalRecordMap::unlink()
{
    unregisterFromGlobal();
    RecordMap::unlink();
}
