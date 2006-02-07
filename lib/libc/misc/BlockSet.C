/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockSet.C,v 1.3 2002/10/10 13:08:19 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides a hierachical bitmap for disk-block
 * allocation management.
 **************************************************************************/

#include <sys/sysIncs.H>
#include "BlockSet.H"

//Represents set of blocks represented by device
BlockSet::BlockSet(uval numBlocks) {
    lock.init();
    mapLevel[0] = &L1[0];
    mapLevel[1] = &L2[0];
    mapLevel[2] = &L3[0];
    mapLevel[3] = &L4[0];

    //err_printf("Device has %ld free blocks\n", numBlocks);
    uval level = 0;
    while (level<4) {
	uval bitsFree = 0;
	for (uval i = 0; i< (uval)(1<<(6*(3-level))); ++i) {
	    mapLevel[level][i].setAll();
	    if (64*i <= numBlocks) {
		//Number of bits on upper end of mask to be set to 1
		uval numBits = numBlocks - 64*i;
		if (numBits>=64) {
		    bitsFree += 64;
		    mapLevel[level][i].clearAll();
		} else {
		    mapLevel[level][i].setAll();
		    mapLevel[level][i].clearFirstBits(numBits);
		    bitsFree += numBits;
		}
	    }
	}
	//err_printf("Level %ld : %8ld/%8ld/%8d free vecs\n", level,
	//            numBlocks,bitsFree,1<<(6*(3-level)));
	++level;

	// numBlocks will tell us how many non-full BitVecs
	// to have on the parent level
	if (numBlocks % 64) {
	    numBlocks+=64;
	}
	numBlocks /= 64;
    };
}


//Retrieve a set of 64 blocks represented by the given bitmap.
//SetID identifies the set of 64 represented by "bv"
// -- setID == 1 -> bv represents blocks 64-127
uval
BlockSet::getBlockSet(uval &setID, AtomicBitVec64* &bv) {
    setID = 0;
    AutoLock<LockType> al(&lock);
    uval level = 3;
    while (level > 0) {
	bv = &mapLevel[level][setID];

	//Find the lowest-order 0 bit
	uval tmp;

	if (level == 1) {
	    tmp = bv->setFindFirstUnSet();
	} else {
	    tmp = bv->findFirstUnSet();
	}

	//64 == operation failed --- no unset bits found
	if (tmp == 64) {
	    // set a bit in the parent to indicate this bitmap is full,
	    // continue search in parent's level
	    passertMsg(level!=3,
		       "Out of space: %ld %lx\n",tmp,(uval)bv->getBits());
	    bv = parent(level, setID);
	    bv->setBit(setID % 64);

	    ++level;
	    setID = setID/64;

	} else {
	    setID = setID * 64 + tmp;
	    --level;
	    bv = &mapLevel[level][setID];
	}
    }
    err_printf("get BlockSet: %ld\n",setID);
    return 1;
}

//clearSet indicates if the specified blockSetID is to be cleared after
// being released.  This allows the caller to set all bits in the set
// to mark it as busy, while it is being freed.
void
BlockSet::putBlockSet(AtomicBitVec64 *target, uval clearSet)
{
    uval level = 0;

    uval orig = blockSetID(target);
    uval curr = orig;
    AutoLock<LockType> al(&lock);
    while (level<3) {
	AtomicBitVec64* bv = parent(level,curr);
	uval bitmapFull = bv->findFirstUnSet()==64;
	bv->clearBit(curr%64);

	if (!bitmapFull) {
	    //We were not the first to clear a bit, we're done
	    //parent's bit for this bitmap is clear
	    break;
	}

	//We cleared the last bit
	curr = curr/64;
	++level;
    }
    if (clearSet) {
	L1[orig].clearAll();
    }
}

