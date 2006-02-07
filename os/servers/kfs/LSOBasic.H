#ifndef __LSO_BASIC_H__
#define __LSO_BASIC_H__
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
 * $Id: LSOBasic.H,v 1.37 2005/07/05 16:52:49 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 *
 *		  Hurricane File Manager : BasicLSO.h
 *			     Copyright 1993
 *
 *		    	   Authors: Orran Krieger
 *
 * 	This software is free for all non-commercial use, and anyone in this
 * category may freely copy, modify, and redistribute any portion of it,
 * provided they retain this disclaimer and author list.
 * 	The authors do not accept responsibility for any consequences of
 * using this program, nor do they in any way guarantee its correct operation.
 *****************************************************************************/

#include "KFSStat.H"
#include <RecordMap.H>
#include <ObjToken.H>
#include <LSOBase.H>

/*
 * The LSOBasic (Logical Server Object Basic) class does all file
 *  authentication and maintains the file length for large files
 */
class LSOBasic : public LSOBase {
public:
    /* What's left for misc usage in a Record */
    /* I'm hard coding this value now because K42 and Linux have different
     * values for KFSStat (96 Linux; 88 K42). I think this needs to be fixed
     * later on.
     * When KFSStat is the same, we should have the following define:
     * #define LSO_BASIC_MISC_SPACE  (KFS_RECORD_SIZE - sizeof(KFSStat))
     */
    static const uval LSO_RECORD_MISC_SPACE = 28;

protected:
    class LSOStat {
    private:
	KFSStat stat;
    public:
	static const uval SIALLUGO = (
	    S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO);
	uval64 statGetNumLinks() { return stat.getNlink(); }
	uval64 statGetMode() { return stat.getMode(); }
	uval64 statGetSize() { return stat.getSize(); }
	uval32 statGetBlocks() { return stat.getBlocks(); }
	void statSetDev(uval64 d) { stat.setDev(d); }
	void statSetRdev(uval64 r) { stat.setRdev(r); }
	void statSetIno(uval64 ino) { stat.setIno(ino); }
	void statSetSize(uval64 s) { stat.setSize(s); }
	void statSetBlocks(uval32 b) { stat.setBlocks(b); }
	void statSetBlksize(uval32 b) { stat.setBlksize(b); }
	void statSetMode(uval64 m) { stat.setMode(m); }
	void statSetNlink(uval64 n) { stat.setNlink(n); }
	void statSetUid(uval32 uid) { stat.setUid(uid); }
	void statSetGid(uval32 gid) { stat.setGid(gid); }
	void statSetCtime(uval64 t) { stat.setCtime(t); }
	void statSetMtime(uval64 t) { stat.setMtime(t); }
	void statSetAtime(uval64 t) { stat.setAtime(t); }
	void copyStatTo(KFSStat *status) {
#ifdef TE_TO_CPU_IS_NOP
	    memcpy((void*) status, (void*) &stat, sizeof(KFSStat));
#else
#ifdef KFS_DEBUG_ENDIAN
	    err_printf("in LSOBasicStruct::copyStatTo, TE_TO_CPU_IS_NOP "
		       "undef\n");
#endif // #ifdef KFS_DEBUG_ENDIAN
	    status->st_dev = stat.getDev();
	    status->st_ino = stat.getIno();
	    status->st_mode = stat.getMode();
	    status->st_nlink = stat.getNlink();
	    status->st_uid = stat.getUid();
	    status->st_gid = stat.getGid();
	    status->st_rdev = stat.getRdev();
	    status->st_size = stat.getSize();
	    status->st_blksize = stat.getBlksize();
	    status->st_blocks = stat.getBlocks();
	    status->atime = stat.getAtime();
	    status->mtime = stat.getMtime();
	    status->ctime = stat.getCtime();
#endif // #ifdef TE_TO_CPU_IS_NOP
	}
    };

    class LSOBasicStruct : public LSOStat {
    private:
	ObjTokenID dataID;
    public:
	ObjTokenID getDataID() {
 	    ObjTokenID tmp;
	    tmp.id = TE32_TO_CPU(dataID.id);
#ifdef KFS_DEBUG_ENDIAN
	    err_printf("LSOBasicStruct::getDataID returning id "
		       "(id 0x%x)\n", tmp.id);
#endif // #ifdef KFS_DEBUG_ENDIAN
	    return tmp;
	}
	void setDataID(ObjTokenID id) {
	    dataID.id = CPU_TO_TE32(id.id);
#ifdef KFS_DEBUG_ENDIAN
	    err_printf("In LSOBasicstruct::setDataID id is 0x%x\n", id.id);
	    err_printf("setDataID set id to ");
	    PRINT_TE32(dataID.id, 0);
#endif // #ifdef KFS_DEBUG_ENDIAN
	}
    };

    // the LSO data
    char lsoBufData[KFS_RECORD_SIZE];

    struct LSOBasicStruct *lsoBuf; // it'll point to lsoBufData

    ObjToken data;      // holds the actual file data

    // functions which assume that the PSO's lock is already held.
    virtual void locked_flush();

public:
    DEFINE_GLOBAL_NEW(LSOBasic);

    LSOBasic(KFSGlobals *g);

    LSOBasic(ObjTokenID, FSFileKFS *f, KFSGlobals *g);

    virtual ~LSOBasic();

    virtual void init();

    // set the ownership of the file
    void chown(uval32, uval32);
    void chmod(uval);
    void utime(const struct utimbuf *);

    void locked_link();
    void link() {
	lock.acquire();
	locked_link();
	lock.release();
    }
    void locked_unlink();
    void unlink() {
	lock.acquire();
	locked_unlink();
	lock.release();
    }
    virtual SysStatus deleteFile();
    virtual void getAttribute(KFSStat *stat);
    virtual SysStatus initAttribute(uval mode, uval uid, uval gid);

    sval getNumLinks() { return lsoBuf->statGetNumLinks(); }
    sval readBlock(uval32 lblkno, char *buffer,
		   PSOBase::AsyncOpInfo *cont = NULL,
		   uval isPhysAddr = 0);
    sval writeBlock(uval64 newSize, uval32 lblkno, char *buffer,
		    PSOBase::AsyncOpInfo *cont = NULL);

    virtual void flush();

    virtual ServerObject *clone(ObjTokenID, FSFileKFS *f);
    virtual SysStatusUval locationAlloc(ObjTokenID);

    /*
     * getPSO()
     *
     *   Returns the PSO which holds the data of the file.
     */
    PSOBase *getPSO() {
        return (PSOBase *)data.getObj(fsfile);
    }
};

#endif /* __LSO_BASIC_H__ */
