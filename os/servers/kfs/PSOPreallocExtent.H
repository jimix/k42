#ifndef __PSO_PREALLOC_EXTENT_H_
#define __PSO_PREALLOC_EXTENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: PSOPreallocExtent.H,v 1.7 2005/04/15 17:39:38 dilma Exp $
 *****************************************************************************/

#include "SuperBlock.H"
#include "ObjToken.H"
#include "PSOBase.H"
#include "KFSDebug.H"
#include "PSOBasicRW.H"

/*
 * This class (PSOPreallocExtent) implements a basic physical server
 * object (PSO) for read/write data where object data is equivalent to
 * a Unix indirect block. The main difference from this PSO and
 * PSOBasicRW is that we try to prealloc a number of blocks at a time
 * (an extent).
 */

class PSOPreallocExtent : public PSOBasicRW {
public:
    DEFINE_GLOBAL_NEW(PSOPreallocExtent);

    // constructors
    PSOPreallocExtent(PSOBase *p, KFSGlobals *g):PSOBasicRW(p, g){ }

    PSOPreallocExtent(ObjTokenID _id, FSFileKFS *f, PSOBase *p,
		      KFSGlobals *g) :
	PSOBasicRW(_id, f, p, g){ }

    ~PSOPreallocExtent() { }

    // handle the reading and writing of data
    sval writeBlock(uval32 lblkno, char *buffer,
		    PSOBase::AsyncOpInfo *cont = NULL);

    // creates a new PSO with the information from the ORSMapEntry
    virtual ServerObject *clone(ObjTokenID, FSFileKFS *f);
};

#endif /* #ifndef __PSOBASIC_RW_H_ */
