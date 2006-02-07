#ifndef __KFS_DHASH_H_
#define __KFS_DHASH_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSDHash.H,v 1.1 2004/02/11 23:03:58 lbsoares Exp $
 *****************************************************************************/

#include <misc/DHashTable.H>

// FIXME: we may want to template HashData also to specify what type of
// pointer we'll be storing. For now let's just make it a Hash Table for
// uval32

class HashData  {
    uval32 key;
    void *data;
    BLock lck;
    static const uval INVALID = uval32(~0);
public:
    void* getData() { return data; }
    void setData(void *d) { data = d; }
    uval isEmpty() { return key == INVALID; }
    uval getKey()  { return key; }
    void setKey(uval32 k)  { key = k; }
    void lock() { lck.acquire(); }
    void unlock() { lck. release(); }
    uval isLocked() { return lck.isLocked(); }
    VPNum firstReplica() { return Scheduler::VPLimit; }
    VPNum nextReplica(VPNum vp) {
	passertMsg(0, "look");
	return 0;
    }
    void print() {
	err_printf("this %p, key %u, data %p\n", this, key, data);
    }
    void clearEmpty() {
	passertMsg(0, "what?");
    }
    void setEmpty(DHashTableBase::OpArg dummy) {
	key = INVALID;
    }
    void init() { lck.init(); }
    static uval32 Hash(uval32 key) { return key; }
};


class KFSDHash;
typedef KFSDHash** KFSDHashRef;

class KFSDHash : public BaseObj {
    typedef MasterDHashTable<HashData,HashData,AllocGlobal,
	AllocGlobal> HashTable;
    HashTable hashTable;
public:
    typedef enum {FOUND, NOT_FOUND} AllocateStatus;

    DEFINE_GLOBAL_NEW(KFSDHash);
    ~KFSDHash() {
	hashTable.cleanup();
    }
    static SysStatus Create(KFSDHashRef &ref);

    virtual SysStatus findOrAddAndLock(uval32 k,  HashData **d,
				       AllocateStatus *st);
    virtual SysStatus findAndLock(uval32 k, HashData **d);
    virtual SysStatus removeData(uval key);
    virtual SysStatus print() {
	hashTable.print();
	return 0;
    }
};

#endif // #ifndef __KFS_DHASH_H_
