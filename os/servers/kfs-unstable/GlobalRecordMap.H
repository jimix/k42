#ifndef __GLOBAL_RECORD_MAP_H_
#define __GLOBAL_RECORD_MAP_H_
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
 * $Id: GlobalRecordMap.H,v 1.3 2004/05/06 19:52:49 lbsoares Exp $
 *****************************************************************************/

#include "RecordMap.H"
#include "LocalRecordMap.H"
#include "Byteorder.H"

// The RecordMapBlock is divided into:
//   localLocalRecordMap           - block for underlying RecordMap
//   psoID                         - ObjTokenID for underlying PSO
//   bitmap[4096-4]                - free-block bitmap mapping

// Max. number of blocks used for the bitmap
#define RMAP_GLOBAL_BITMAP_BLOCKS   ((4096-(sizeof(ObjTokenID) + sizeof(uval32))) /  \
				     sizeof(uval32))

class GlobalRecordMap : public RecordMapBase {
protected:
    KFSGlobals *globals;
    BLock lock;
    FSFileKFS *fsfilePSO;  // FSFileKFS used for the PSO
    PSOBase *pso;          // underlying PSO, used to actually keep the data
    RecordMapBase *localRecordMap;
    uval32 flags;
    uval32 blkno;
    KFSHash<uval32, RecordMapBase *> localHash;

    // the actual metadata storage
    BlockCacheEntry *dataBlock;

    // This structure should occupy one (4k) block 
    struct GlobalRecordMapStruct {
    private:
	// Block number for the underlying LocalRecordMap
	uval32 localRecordMapBlk;
	// ID for underlying PSO (that keeps the data for the records)
	ObjTokenID psoID;
	// mapping for the blocks which contain the free-record bitmap
	uval32 bitmap[RMAP_GLOBAL_BITMAP_BLOCKS] __attribute__ ((packed));

    public:
	uval32 getLocalRecordMapBlk() {
	    return TE32_TO_CPU(localRecordMapBlk);
	}
	void setLocalRecordMapBlk(uval32 blkno) {
	    localRecordMapBlk = CPU_TO_TE32(blkno);
	}
	ObjTokenID getPSOID() {
	    ObjTokenID retID = { TE64_TO_CPU(psoID.id) };
	    return retID;
	}
	void setPSOID(ObjTokenID id) {
	    psoID.id = CPU_TO_TE64(id.id);
	}
	uval32 getBitmap(int i) {
	    return TE32_TO_CPU(bitmap[i]);
	}
	void setBitmap(int i, uval32 v) { bitmap[i] = CPU_TO_TE32(v); }
    };

    struct GlobalRecordMapStruct *data;

    static const uval HALF_BITS = 8 * sizeof(ObjTokenID)/2;

    uval64 objTokenIDToGlobalID(uval64 id) {
	// We want just the upper half bits.
	return (id >> HALF_BITS);
    }

    uval64 globalIDToObjTokenID(uval64 id) {
	// put these bits in the upper half
	return (id << HALF_BITS);
    }

    // Internal version of {alloc,free}Record
    virtual SysStatus internal_allocLocalRecordMap(uval32 *offset);
    virtual SysStatus internal_freeLocalRecordMap(uval32 offset);

    // functions which assume that the PSO's lock is already held.
    virtual uval32 locked_flush();
    virtual void locked_markDirty();

public:
    DEFINE_GLOBAL_NEW(GlobalRecordMap);

    GlobalRecordMap(uval32 blkno, KFSGlobals *g);
    virtual ~GlobalRecordMap();

    virtual void init();

    virtual SysStatus allocLocalRecordMap(uval32 *offset);
    virtual SysStatus freeLocalRecordMap(uval32 offset);

    virtual SysStatusUval getLocalRecordMap(uval offset);
    virtual SysStatus setLocalRecordMap(uval offset, uval32 blkno);

    virtual void registerLocalRecordMap(ObjTokenID *id, RecordMapBase *local);
    virtual void unregisterLocalRecordMap(ObjTokenID *id);
    virtual RecordMapBase *getLocalRecordMap(ObjTokenID *id);

    // flush a dirty PSO to disk
    virtual uval32 flush();

    // mark the PSO as dirty
    virtual void markDirty();

    static SysStatusUval locationAlloc(KFSGlobals *globals);
    virtual void unlink();

    // All of the "RecordMapBase" interfaces should just get routed to the
    // "local" LocalRecordMap
    virtual SysStatus allocRecord(PsoType type, ObjTokenID *otokID) {
	return localRecordMap->allocRecord(type, otokID);
    }
    virtual SysStatus freeRecord(ObjTokenID *otokID) {
	return localRecordMap->freeRecord(otokID);
    }
    virtual SysStatus getRecord(ObjTokenID *otokID, char* rec) {
	return localRecordMap->getRecord(otokID, rec);
    }
    virtual SysStatus setRecord(ObjTokenID *otokID, char *rec) {
	return localRecordMap->setRecord(otokID, rec);
    }
    virtual SysStatus setRecordType(ObjTokenID *otokID, PsoType type) {
	return localRecordMap->setRecordType(otokID, type);
    }
    virtual PsoType getRecordType(ObjTokenID *otokID) {
	return localRecordMap->getRecordType(otokID);
    }

    virtual ServerObject *getObj(ObjTokenID *otokID) {
	return localRecordMap->getObj(otokID);
    }

    virtual SysStatus removeObj(ObjTokenID *otokID) {
	return localRecordMap->removeObj(otokID);
    }

    virtual SysStatus sync() {
	return localRecordMap->sync();
    }
};

#endif /* #ifndef __RECORD_MAP_H_ */
