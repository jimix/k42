#ifndef __LSO_BASIC_SYMLINK_H__
#define __LSO_BASIC_SYMLINK_H__
/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LSOBasicSymlink.H,v 1.3 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/
#include "LSOBasic.H"

/*
 * The LSOBasicSymlink class maintains all of the interfaces to logical
 * server objects that are specifically used by symbolic links.
 */
class LSOBasicSymlink : public LSOBasic {
    /* We use whaat's left for our usage:
     * #define FAST_SYMLINK_SIZE   (KFS_RECORD_SIZE - sizeof(K42Stat))
     * I'm hard coding this value now because K42 and Linux have different
     * values for K42Stat. I think this needs to be fixed later on.
     */
    static const uval32 FAST_SYMLINK_SIZE = LSO_RECORD_MISC_SPACE;

    /* This figures out how many records we can address $\times$ the size 
       of the record */
    static const uval MED_SYMLINK_SIZE = ((FAST_SYMLINK_SIZE /
					   (sizeof(ObjTokenID)))
					  * KFS_RECORD_SIZE);

    class LSOBasicSymlinkStruct : public LSOStat {
    private:
	union {
	    char value[FAST_SYMLINK_SIZE];  // FAST
	    ObjTokenID ext[3];              // MEDIUM
	    uval32 blkno;                   // SLOW
	} u;
    public:
	char *getValuePtr() { return u.value; }
	ObjTokenID getObjTokenID(int i) {
	    tassertMsg(i < 3, "i %d\n", i);
	    ObjTokenID tmp;
	    tmp.id = TE64_TO_CPU(u.ext[i].id);
	    return tmp;
	}
	void setObjTokenID(ObjTokenID id, int i) {
	    tassertMsg(i < 3, "i %d\n", i);
	    u.ext[i].id = CPU_TO_TE64(id.id);
	}
	uval32 getBlkno() { return TE32_TO_CPU(u.blkno); }
	void setBlkno(uval32 b) { u.blkno = CPU_TO_TE32(b); }
    };

    // We want to override LSOBasic's flush() because it write-backs the
    // ObjtokenID field in an inconsistent way for symlinks
    void locked_flush();

public:
    DEFINE_GLOBAL_NEW(LSOBasicSymlink);

    LSOBasicSymlink(KFSGlobals *g) : LSOBasic(g) { }

    LSOBasicSymlink(ObjTokenID *otokID, RecordMapBase *r, KFSGlobals *g);

    ~LSOBasicSymlink() { }

    void init();
    
    SysStatus initAttribute(const char *linkValue, uval mode, uval uid,
			    uval gid);
    SysStatusUval readlink(char *buffer, uval buflen);

    void flush() {
	// lock and flush
	lock.acquire();
	locked_flush();
	lock.release();
    }

    virtual SysStatus deleteFile();

    virtual ServerObject *clone(ObjTokenID *otokID, RecordMapBase *r);
};

#endif /* !__LSO_BASIC_FILE_H__ */
