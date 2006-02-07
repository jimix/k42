#ifndef __LSOBASE_H_
#define __LSOBASE_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LSOBase.H,v 1.2 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/

#include "PSOBase.H"

/*
 * The LSOBase class is the base set of interfaces which every
 * Logical Server Object (LSO) must support.
 */
class LSOBase : public ServerObject {
public:
    LSOBase(KFSGlobals *g):ServerObject(g) { }
    LSOBase(ObjTokenID *id, RecordMapBase *r, KFSGlobals *g) 
	: ServerObject(id, r, g) { }

    // delete the file
    virtual void unlink() = 0;

    // flush any cached metadata to disk
    virtual void flush() = 0;

    // return the associated PSO which holds the file data
    virtual PSOBase *getPSO() = 0;
};

#endif /* #ifndef __LSOBASE_H_ */
