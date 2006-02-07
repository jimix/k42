#ifndef __LSOBASIC_DIR_H_
#define __LSOBASIC_DIR_H_
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
 * $Id: LSOBasicDir.H,v 1.27 2004/03/07 00:42:39 lbsoares Exp $
 *****************************************************************************/

#include "LSOBasic.H"
#include <fslib/direntk42.H>

#include "Byteorder.H"

/*
 * The LSOBasicDir class maintains all of the interfaces to logical
 * server objects that are specifically used by directories.
 */
class LSOBasicDir : public LSOBasic {
public:
    /* DirEntry structure holds the header for a directory entry */

    struct DirEntry {
    private:
	uval32 otokID;                              /* the object token id */
	uval16 reclen __attribute__ ((packed));     /* length of record  */
    public:
	uval8  nameLength __attribute__ ((packed)); /* length of the name */
	/* replaced by arbitrary length char string */
	char   name[256] __attribute__ ((packed));

	uval32 getOtokID() { return TE32_TO_CPU(otokID); }
	void setOtokID(uval32 v) { otokID = CPU_TO_TE32(v); }
	uval16 getReclen() { return TE16_TO_CPU(reclen); }
	void setReclen(uval16 rl) { reclen = CPU_TO_TE16(rl); }
    };

protected:
    // Optimization for directory lookups. Depends on locality for performance
    // increase, but presents the same worst case overhead, as without this.
    uval32 startLBlkno;

    sval matchEntry(const char *, sval, DirEntry **, DirEntry **,
		    BlockCacheEntry **, uval &);
    sval findHole(char *, sval, DirEntry **);

    // implementations that assume the lock is held
    sval locked_deleteEntry(char *, sval, ObjToken *otok = NULL);
    sval locked_createEntry(char *fname, sval length, ObjToken *otok,
			    sval mode, sval uid);
    sval locked_matchIndex(uval, uval *, DirEntry *entry=NULL);
    sval locked_matchDir(const char *, sval, ObjTokenID *);

public:
    DEFINE_GLOBAL_NEW(LSOBasicDir);

    LSOBasicDir(KFSGlobals *g) : LSOBasic(g) { }

    LSOBasicDir(ObjTokenID otokID, FSFileKFS *f, KFSGlobals *g) :
	LSOBasic(otokID, f, g) {
	startLBlkno = 0;
    }

    virtual void init() { LSOBasic::init(); }

    virtual sval initAttribute(uval mode, uval uid, uval gid);

    sval getDents(uval &, struct direntk42 *, uval);
    sval matchDir(const char *, sval, ObjTokenID *);
    sval matchIndex(uval, uval *, DirEntry *entry = NULL);
    sval deleteEntry(char *, sval, ObjToken *);
    sval updateEntry(char *, sval, ObjToken *);
    sval createEntry(char *fname, sval length, ObjToken *otok,
		     sval mode, sval uid);
    sval createDir(char *, sval, sval, sval, ObjToken *);
    sval deleteDir(char *, sval);

    virtual ServerObject *clone(ObjTokenID, FSFileKFS *f);

    /*
     * GetPhysRecLen()
     *
     *   Determines appropriate record length of DirEntry and does proper
     *   memory alignment too!
     */
    static uval GetPhysRecLen(uval lengthString) {
	uval recLen = sizeof(struct DirEntry) - 256; // FIXME: HARD-CODED :(
	recLen += lengthString;
	recLen = ALIGN_UP(recLen, sizeof(uval32));
	return recLen;
    }
    static uval GetMemRecLen(uval lengthString);
};

#endif /* #ifndef __LSOBASIC_DIR_H_ */
