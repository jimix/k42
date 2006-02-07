#ifndef __LSODIR_EMB_H_
#define __LSODIR_EMB_H_
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
 * $Id: LSODirEmb.H,v 1.2 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "LSOBasicDir.H"
#include <fslib/direntk42.H>

#include "Byteorder.H"
#include "LocalRecordMap.H"

/*
 * The LSODirEmb class is the directory implementation which "embedds" inode
 * information in directory blocks, instead of using blocks from the global
 * RecordMap.
 */
class LSODirEmb : public LSOBasicDir {
protected:
    LocalRecordMap *localRecordMap;

    class LSODirEmbStruct : public LSOBasicStruct {
    private:
	uval32 recordMapBlk;
	uval64 recordMapID;
    public:
	uval32 getRecordMapBlk() {
	    return TE32_TO_CPU(recordMapBlk);
	}
	void setRecordMapBlk(uval32 blkno) {
	    recordMapBlk = CPU_TO_TE32(blkno);
	}
	ObjTokenID getRecordMapID() {
	    ObjTokenID tmp;
	    tmp.id = TE64_TO_CPU(recordMapID);
	    return tmp;
	}
	void setRecordMapID(ObjTokenID id) {
	    recordMapID = CPU_TO_TE64(id.id);
	}
    };

    virtual void locked_flush();

public:
    DEFINE_GLOBAL_NEW(LSODirEmb);

    LSODirEmb(KFSGlobals *g) : LSOBasicDir(g), localRecordMap(NULL) { }

    LSODirEmb(ObjTokenID *otokID, RecordMapBase *r, KFSGlobals *g) :
	LSOBasicDir(otokID, r, g) { }

    virtual ~LSODirEmb();

    virtual void init();

    virtual SysStatus deleteFile();
    virtual SysStatus createRecord(ObjTokenID *newTokID, PsoType type);

    virtual ServerObject *clone(ObjTokenID *otokID, RecordMapBase *r);
    virtual SysStatusUval locationAlloc(ObjTokenID *otokID, RecordMapBase *r);
    
    virtual RecordMapBase *getLocalRecordMap() {
	return localRecordMap;
    }
};

#endif /* #ifndef __LSODIR_EMB_H_ */
