#ifndef __LOCAL_RECORD_MAP_H_
#define __LOCAL_RECORD_MAP_H_
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
 * $Id: LocalRecordMap.H,v 1.2 2004/05/05 19:57:59 lbsoares Exp $
 *****************************************************************************/

#include "RecordMap.H"

class FSFileKFS;
class PSOBase;
class BlockCacheEntry;

/*
 * LocalRecordMap should act like a RecordMap "linked" or "tied to" a directory,
 * in such a way that we should be able to distribute KFS' meta-data in a
 * tree-like fashion, based on the name-space of the file system.
 *
 * The LocalRecordMap class was a PSO used to actually store the data of
 * fixed sized record PSOs (PSONmRec).  It is kept as an array of
 * PSO_RMAP_RECORD_SIZE records which can be allocated from an
 * internally stored and managed bitmap.
 */
class LocalRecordMap : public RecordMap {
protected:
    //    ObjTokenID globalID;

    static const uval HALF_BITS = 8 * sizeof(ObjTokenID)/2;

    uval64 objTokenIDToLocalID(uval64 id) {
	// We want just the lower half bits.
	//	uval32 mask = ((1 << HALF_BITS) - 1) << 16;
	//	return (id & ~mask);
	return id;
    }

    uval64 localIDtoObjTokenID(uval64 id) {
	//	return (id | (globalID.id << 16));
	return id;
    }

    void unregisterFromGlobal();

public:
    DEFINE_GLOBAL_NEW(LocalRecordMap);

    LocalRecordMap(uval32 blkno, ObjTokenID id, KFSGlobals *g) :
	RecordMap(blkno, g) {} // , globalID(id) {}

    virtual ~LocalRecordMap();

    virtual void init();

    // returns an offset to a sequential run of 64-byte records
    virtual SysStatus allocRecord(PsoType, ObjTokenID *);
    virtual sval freeRecord(ObjTokenID *);

    // get a record of the given size at the given location
    virtual sval getRecord(ObjTokenID *, char *);
    virtual sval setRecord(ObjTokenID *, char *);

    virtual sval setRecordType(ObjTokenID *, PsoType);
    virtual PsoType getRecordType(ObjTokenID *);

    // creates a new PSO with the information from the ORSMapEntry
    //    static SysStatusUval locationAlloc(KFSGlobals *globals);

    virtual void unlink();

    virtual RecordMapBase *getLocalRecordMap(ObjTokenID) { return this; }
};

#endif /* #ifndef __LOCAL_RECORD_MAP_H_ */
