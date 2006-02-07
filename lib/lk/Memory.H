#ifndef __MEMORY_H_
#define __MEMORY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Memory.H,v 1.10 2005/05/24 20:44:48 rosnbrg Exp $
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

void * kmalloc(size_t size, unsigned int flags);
void kfree(const void *addr);

//unsigned long __get_free_pages (int gfp_mask, unsigned long order);

#ifdef __cplusplus
} // extern "C"
#endif /* #ifdef __cplusplus */

#include <misc/HashNonBlocking.H>

/*
 * we use fields in page for hash.
 * virtual is the key - it gets munged to __C_virtual__
 * next_hash is the bucket chain
 */
class MemoryHashNode: public page {
public:
    MemoryHashNode* nextFree() {
	return (MemoryHashNode*)lru.next;
    }
    void setNextFree(MemoryHashNode* node) {
	lru.next = (struct list_head*)node;
    }
    MemoryHashNode* nextHash() {
	return (MemoryHashNode*)__C__private;
    }
    void setNextHash(MemoryHashNode* node) {
	__C__private = (unsigned long)node;
    }
    uval getKey() {
	return uval(__C__virtual);
    }
    void setKey(uval key) {
	__C__virtual = (void*)key;
    }
    static uval Hash(uval key) {
	return key>>LOG_PAGE_SIZE;
    }
    /*
     * increase use count if alread in use
     * return true if in use
     */
    sval upLock() {
	sval32 c;
	do {
	    c = _count.counter;
	    if (c == 0) break;
	} while (!CompareAndStore32Synced((uval32*)(&_count.counter),
					 c, c+1));
	return c;
    }
    // returns + if still locked, 0 if became unlocked
    // - if already unlocked
    sval unLock() {
	sval32 c;
	do {
	    c = _count.counter;
	    if (c == 0) break;
	} while (!CompareAndStore32Synced((uval32*)(&_count.counter),
					 c, c-1));
	return c-1;
    }
    uval isLocked() {
	return uval(_count.counter);
    }
};

class MemoryHash:
    public HashNonBlockingBase<AllocPinnedGlobalPadded, MemoryHashNode, 2> {
public:
    /*
     * array of descriptor logic - special to linux Memory.C appliation
     */
    SysStatus alloc_array_node(uval count, MemoryHashNode*& desc) {
	HashLock hashLock(useLock);
	/* for now we don't even try to reuse already used ones */
	desc = (MemoryHashNode*)
	    allocPinnedGlobalPadded(sizeof(MemoryHashNode) * count);
	return 0;
    }


    SysStatus add_array_node(MemoryHashNode* nodearg, uval count) {
	HashLock hashLock(useLock);
	/* assume arrays are never on the chain */
	uval i;
	MemoryHashNode *head, *node;
	uval index;
	uval key;
	node = (MemoryHashNode*)nodearg;
	for (i=0; i< count; i++) {
	    key = node[i].getKey();
	    index = hashMask&MemoryHashNode::Hash(key);
	    do {
		head = hashTable[index].chainHead;
		node[i].setNextHash(head);
	    } while (!CompareAndStoreSynced(
		(uval *)(&hashTable[index].chainHead),
		uval(head), uval(&(node[i]))));
	}
	FetchAndAdd(&numNodes, count);
	return 0;
    }

    SysStatus remove_array_node(uval nodekey, uval count) {
	HashLock hashLock(useLock);
	uval index, key;
	MemoryHashNode *node;
	index = hashMask&MemoryHashNode::Hash(nodekey);
	node = hashTable[index].chainHead;
	while (node) {
	    if (node->getKey() == nodekey) {
		break;
	    }
	    node = node->nextHash();
	}
	tassertMsg(node, "0x%lx not found\n", nodekey);
	uval i, unLockRC;
	for (i=0;i<count;i++) {
	    key = node[i].getKey();
	    tassertMsg(key == (nodekey+PAGE_SIZE*i), "not an array by key\n");
	    index = hashMask&MemoryHashNode::Hash(key);
	    unLockRC = node[i].unLock();
	    tassertMsg(unLockRC == 0, "someone locked an array descriptor\n");
	    tassertMsg(uval(node[i].index) == uval(node), "not an array\n");
	    locked_dequeue(&node[i], index, hashLock);
	}
	node->setKey(count);		// remember count in free node
	MemoryHashNode *oldRemovedArray;
	do {
	    oldRemovedArray = removedArray;
	    node->setNextFree(oldRemovedArray);
	} while (!CompareAndStoreSynced((uval*)(&removedArray),
				       uval(oldRemovedArray), uval(node)));
	return 0;
    }
};

// This mess allows us to dynamically allocate a HashTable into a
// static piece of memory.  This allows the compiler to optimize out
// the pointer de-references on the pageHash pointer.
typedef MemoryHash __PageHash;

class PageHash: public __PageHash {
public:
    PageHash() {};
    inline void * operator new(size_t size, void* ptr) {
	return ptr;
    }
};

#endif /* #ifndef __MEMORY_H_ */

