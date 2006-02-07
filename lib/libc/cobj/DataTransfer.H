#ifndef __DATA_TRANSFER_H_
#define __DATA_TRANSFER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DataTransfer.H,v 1.6 2001/10/16 05:31:48 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Definitions for supporting cobj data transfer
 * **************************************************************************/

#include <misc/BitVec.H>

class CObjRoot;

#define DTTYPE_NUMBITS	(64)
typedef uval DTType;

class DTTypeSet {
    BitVec64 vec;
public:
    DTTypeSet()					{ /*vec.clearAll();*/ }
    void reset()				{ vec.clearAll(); }
    void addType(DTType dtt)			{ vec.setBit(dtt); }
    void removeTypeID(DTType dtt)		{ vec.clearBit(dtt); }
    DTType matchBest(const DTTypeSet &other)
    {
	BitVec64 intersection = vec;

	intersection.applyMask(other.vec);
	return (DTType)intersection.findFirstOne();
    }
};


class DataTransferObject {
public:
    DEFINE_NOOP_NEW(DataTransferObject);
    // make sure all destructors are virtual and called properly
    // let compiler generate the destructors, only declare it here
    // the important consequence of this is that delete gets passed the
    // correct size even if the pointer is of a base class type
    virtual ~DataTransferObject() { /* empty body */ }

    static DTType negotiate(CObjRoot *oldRoot, CObjRoot *newRoot);
};

#endif /* #ifndef __DATA_TRANSFER_H_ */
