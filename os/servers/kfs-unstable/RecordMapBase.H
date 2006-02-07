#ifndef __RECORD_MAP_BASE_H_
#define __RECORD_MAP_BASE_H_
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
 * $Id: RecordMapBase.H,v 1.3 2004/05/06 19:52:49 lbsoares Exp $
 *****************************************************************************/

#include "ObjToken.H"
#include "PSOTypes.H"
class KFSGlobals;
class ServerObject;

#define RMAP_RECORD_SIZE           (128)
// reserve 4 bytes for the PsoType
#define KFS_RECORD_SIZE            (124)

// Number of 32-bit words in a page
#define RMAP_WORDS_PAGE      (4096/sizeof(uval32))
// Number of records (bits) per page
#define RMAP_BITS_PAGE       (4096*8)

#define RMAP_NONE  0x00
#define RMAP_DIRTY 0x01

/*
 * The RecordMap class was a PSO used to actually store the data of
 * fixed sized record PSOs (PSONmRec).  It is kept as an array of
 * PSO_RMAP_RECORD_SIZE records which can be allocated from an
 * internally stored and managed bitmap.
 */
class RecordMapBase {
public:
    //    RecordMapBase() {};
    //    virtual ~RecordMapBase() {};

    virtual void init()=0;

    // returns an offset to a sequential run of 64-byte records
    virtual SysStatus allocRecord(PsoType, ObjTokenID *id)=0;
    virtual sval freeRecord(ObjTokenID *id)=0;

    // get a record of the given size at the given location
    virtual sval getRecord(ObjTokenID *id, char *)=0;
    virtual sval setRecord(ObjTokenID *id, char *)=0;

    virtual sval setRecordType(ObjTokenID *id, PsoType)=0;
    virtual PsoType getRecordType(ObjTokenID *id)=0;

    // flush a dirty PSO to disk
    virtual uval32 flush()=0;

    // mark the PSO as dirty
    virtual void markDirty()=0;

    // creates a new PSO with the information from the ORSMapEntry
    // static SysStatusUval locationAlloc(KFSGlobals *globals)=0;
    virtual void unlink()=0;

    virtual RecordMapBase *getLocalRecordMap(ObjTokenID *id)=0;

    virtual ServerObject *getObj(ObjTokenID *otokID)=0;

    virtual SysStatus removeObj(ObjTokenID *otokID)=0;

    virtual SysStatus sync()=0;
};

#endif /* #ifndef __RECORD_MAP_BASE_H_ */
