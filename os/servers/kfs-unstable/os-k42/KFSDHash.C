/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSDHash.C,v 1.1 2004/02/11 23:03:58 lbsoares Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "KFSDHash.H"
#include <cobj/CObjRootSingleRep.H>

/* virtual */ SysStatus
KFSDHash::findOrAddAndLock(uval32 k,  HashData **d, AllocateStatus *st)
{
    HashTable::AllocateStatus astat =
	hashTable.findOrAllocateAndLock(k, d);
    if (astat == HashTable::FOUND) {
	*st = FOUND;
    } else {
	*st = NOT_FOUND;
    }
    return 0;
}

/* virtual */ SysStatus
KFSDHash::findAndLock(uval32 k, HashData **d)
{
    *d = hashTable.findAndLock(k);
    return 0;
}
/* virtual */ SysStatus
KFSDHash::removeData(uval key)
{
    hashTable.doEmpty(key, 0);
    return 0;
}

/* static */ SysStatus
KFSDHash::Create(KFSDHashRef &ref)
{
    KFSDHash* obj = new KFSDHash();
    if (obj) {
	ref = (KFSDHashRef) CObjRootSingleRep::Create(obj);
	return 0;
    } else {
	return _SERROR(2715, 0, ENOMEM);
    }
}

//TEMPLATEDHASHTABLE(TestHashData,AllocGlobal,TestHashData,AllocGlobal)
//template DHashTable<HashData,AllocGlobal>;
//template MasterDHashTable<HashData,HashData,AllocGlobal,AllocGlobal>;





