#ifndef __PSOSMALL_META_H_
#define __PSOSMALL_META_H_
/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: PSOSmallMeta.H,v 1.2 2004/05/05 19:57:59 lbsoares Exp $
 **************************************************************************/

#include "PSOSmall.H"
#include "KFSHash.H"

/*
 * A basic physical server object (PSO) for read/write meta-data. This is an
 * extension os regular PSOSmall. The main difference here is that it contains
 * a hash for faster look-up of logical block numbers.
 */

class PSOSmallMeta : public PSOSmall {
protected:
    KFSHash<uval32,uval32> blockMap;

    uval32 getDblk(uval32, uval8);
    void freeDblk(uval32 lblkno);
    void truncate(uval32 lblkno);
public:
    DEFINE_GLOBAL_NEW(PSOSmallMeta);

    PSOSmallMeta(PSOBase *p, KFSGlobals *g):PSOSmall(p, g), blockMap() { }

    PSOSmallMeta(ObjTokenID *otokID, RecordMapBase *r, PSOBase *p,
		 KFSGlobals *g) : PSOSmall(otokID, r, p, g), blockMap() { }

    ~PSOSmallMeta() {
	blockMap.removeAll();
	blockMap.destroy();
    }

    // creates a new PSO with the information from the ORSMapEntry
    ServerObject *clone(ObjTokenID *otokID, RecordMapBase *r);
};

#endif /* #ifndef __PSOSMALL_H_ */
