#ifndef __HASH_NONBLOCKING_H_
#define __HASH_NONBLOCKING_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HashNonBlocking.H,v 1.9 2003/03/21 21:12:45 marc Exp $
 *****************************************************************************/
/*
 * This class implements a non blocking hash lookup
 * using ReadCopyUpdate type logic.
 */

#include <sync/atomic.h>

/*
 * we use fields in element for hash chains.
 * user must provide an Element class with
 * the appropriate methods to manipulate
 * the fields.
 * The following sample documents the requirements
 * Two chains are required.  The hash chain must always
 * be available to the hash mechanism.
 * The free chain can be shared with any field which
 * will not be used once the element is free.
 */
#if 0    //!!!!!!!sample of Element to be provided by user
    class Element {
    public:
	Element* nextFree() {
	    return (Element*)hash_chain;
	}
	void setNextFree(Element* node) {
	    hash_chain = (hash_chain_type)node;
	}
	Element* nextHash() {
	    return (Element*)next_hash;
	}
	void setNextHash(Element* node) {
	    next_hash = (next_hash_type)node;
	}
	uval getKey() {
	    return key;
	}
	void setKey(uval key) {
	    key = (key_type)key;
	}
	static uval Hash(uval key) {
	    return hashfunc(key);
	}
	/*
	 * the hash supports use counts - that is elements
	 * which are in use by multiple users.
	 * increase use count if alread in use
	 * return true if in use
	 * at a minimum, the Element must provide a way of marking
	 * the Element as in use/no longer in use.
	 * upLock returns 0 if the element is no longer in use.
	 * such elements cannot return to the inuse state.
	 * upLock returns non-zero if the element is in use.
	 */
	sval upLock() {
	    sval32 c;
	    do {
		c = use_count;
		if(c == 0) break;
	    } while(!CompareAndStore32Synced((uval32*)(&use_count),
				       c, c+1));
	    return c;
	}
	/*
	 * returns + if still inuse, 0 if became unused
	 * - if already unused
	 */
	sval unLock() {
	    sval32 c;
	    do {
		c = use_count;
		if(c == 0) break;
	    } while(!CompareAndStore32Synced((uval32*)(&use_count),
				       c, c-1));
	    return c-1;
	}
	uval isLocked() {
	    return uval(use_count);
	}
    };
#endif

template <class ALLOC, class Element, uval DefaultSetSize>
class HashNonBlockingBase {
protected:
    
    /*
     * Normally, we do not lock.  If we detect that the hash table
     * needs to be enlarged, we set useLock and record the
     * GC generation.  All subsequent calls use locking.  When the
     * generation count has changed, we know that all threads using
     * this hash are using locking.  At that point, a thread
     * holding the lock enlarges the hash table and turns off
     * useLock, thus returning to nonBlocking operation.
     *
     * Because HashLock methods can't refer to HashNonBlockingBase members,
     * we need to pass the information to the constructor.  The
     * most efficient way is a slight kludge.  The useLock flag is
     * zero or a pointer to the lock.  So we only need to pass
     * one value to the constructor, and its code is minimal in the
     * case that useLock is 0.
     */
    class HashLock {
	BLock *lockp;
    public:
	HashLock(BLock *l) {
	    lockp = l;
	    if(lockp) {
		lockp->acquire();
	    }
	}
	DEFINE_NOOP_NEW(HashLock);
	~HashLock() {
	    if(lockp) {
		lockp->release();
	    }
	}
	void acquire(BLock *l) {
	    if(!lockp) {
		lockp = l;
		lockp->acquire();
	    }
	}
    };
    
    
    struct HashNode {
	Element *chainHead;
    };

    HashNode *hashTable;
    HashNode initialTable[DefaultSetSize*2];
    uval   hashMask;			// size of table less 1
    uval   numNodes;			// sval because 
    BLock  lock;
    COSMgr::ThreadMarker marker;	// oldEntries generation marker
    BLock* useLock;			// when true use locking
    uval   extendRequest;		// we need to enlarge the hash table
    COSMgr::ThreadMarker lockMarker;	// lock request generation marker
#ifdef ALLOW_NO_NONBLOCKING
    uval   useLocking;			// used to turn off nonblocking
					// for measurments
#endif

    /*
     * ReadCopyUpdate requires that we defer reuse of nodes.
     * When a node is freed, it is added to removed
     * When free list is empty, we see if the token marker has changed.
     * If so, the list of marked entries is available.  That list is moved
     * to free, the removed list is moved to marked, and a new marker is
     * recorded.
     * If we can't get a free entry, we allocate one.
     * We do normal alloc/free nonblocking, but do the token manipulations
     * holding the lock.
     */
    Element *free;		// reusable entries
    Element *marked;		// entries waiting for marker
    Element *removed;		// newly freed entries
    Element *markedArray;	// array entries waiting for marker
    Element *removedArray;	// newly freed array entries

    void locked_enqueue(Element * node) {
	_ASSERT_HELD(lock);
	uval index, key;
	Element * next;
	key = node->getKey();
	index = hashMask&Element::Hash(key);
	do {
	    next = hashTable[index].chainHead;
	    node->setNextHash(next);
	} while(!CompareAndStoreSynced((uval*)(&hashTable[index].chainHead),
				       uval(next), uval(node)));
	FetchAndAdd(&numNodes, 1);
    }

    void locked_extendHash();

    /*
     * organize code so relatively cheap check of marker is done
     * inline when already trying to extend.
     */
    void extendHashIfReady(HashLock& hashLock) {
	if(!extendRequest) return;
	// in extendRequest mode, lock is held
	_ASSERT_HELD(lock);
	if (extendRequest < numNodes) extendRequest = numNodes;
	COSMgr::MarkerState stat;
	DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(lockMarker, stat);
	if (stat != COSMgr::ACTIVE) {
	    locked_extendHash();
	}
    }
    
    void locked_free_node(Element* node) {
	Element *oldRemoved;
	do{
	    oldRemoved = removed;
	    node->setNextFree(oldRemoved);
	} while(!CompareAndStoreSynced((uval*)(&removed),
				       uval(oldRemoved), uval(node)));
    }

    void locked_dequeue(
	Element *node, uval index, HashLock& hashLock) {

	FetchAndAdd(&numNodes, uval(-1));

	/* we must free it - no one else will try to free this node
	 * take a crack at the common case that this is the first
	 * and only node.
	 * Because we only push nodes, if node->nextHash is zero, it can't
	 * become non-zero.
	 */
	while((hashTable[index].chainHead == node) && (0==node->nextHash())) {
	    if(CompareAndStoreSynced(
		(uval *)(&hashTable[index].chainHead), uval(node), 0)) {
		return;
	    }
	}

	locked_dequeue_slow(node, index, hashLock);
    }
    void locked_dequeue_slow(Element *node, uval index, HashLock& hashLock);

    SysStatus alloc_node_slow(Element*& desc, HashLock& hashLock);

    /*
     * following debugging call is NOT SAFE
     * since lists can change out from under it
     */
    uval listSize(Element* list) {
	uval i = 0;
	while(list) {
	    i++;
	    list = list->nextFree();
	}
	return i;
    }
    
    void printListSizes() {
	err_printf("free %ld ", listSize(free));
	err_printf("marked %ld ", listSize(marked));
	err_printf("removed %ld ", listSize(removed));
    }		   

    uval marcPrintKludge;
    
public:

    HashNonBlockingBase();

    /*
     * force locking for measurements
     */
    void setUseLocking() {
#ifdef ALLOW_NO_NONBLOCKING
	useLocking = 1;
	useLock = &lock;
#endif
    }
	
    /*
     * blows away all storage - so be sure no other threads are
     * still using anything
     */
    void destroy();
    
    SysStatus alloc_node(Element*& desc) {
	HashLock hashLock(useLock);
	Element* node;
	do {
	    node = free;
	    if(!node) break;
	} while(!CompareAndStoreSynced((uval*)(&free),
				       uval(node), uval(node->nextFree())));
	if(node) {
	    desc = node;
	    return 0;
	}
	return alloc_node_slow(desc, hashLock);
    }
	

    /*
     * node must point to an initialized, locked (use count non-zero)
     * node, which will be used or freed
     * returns 1 if added, 0 if found
     */
    uval find_or_add_node(Element*& desc) {
	HashLock hashLock(useLock);
	extendHashIfReady(hashLock);
	if(marcPrintKludge) {
	    printListSizes();
	    marcPrintKludge = 0;
	}
	Element* node = (Element*)desc;
	uval index;
	uval key;
	tassertMsg(node->isLocked(), "adding unlocked node\n");
	Element *next, *head;
	key = node->getKey();
	index = hashMask&Element::Hash(key);
	/* insert involves either locking an existing node or
	 * inserting the new one.  Because of readcopyupdate and the
	 * fact that we only push new nodes at the front, it is true
	 * that if a key is not found and the chainhead has not changed
	 * that key cannot be on the chain.
	 */
	do {
	    head = next = hashTable[index].chainHead;
	    uval c;
	    while(next) {
		if(next->getKey() == key && (c = next->upLock())) {
		    locked_free_node(node);
		    //err_printf("find %lx\n count %lx", key, c);
		    desc = next;
		    return 0;
		}
		next = next->nextHash();
	    }
	    // nothing on the chain matches, so try to push the new node
	    node->setNextHash(head);
	} while(!CompareAndStoreSynced((uval *)(&hashTable[index].chainHead),
				       uval(head), uval(node)));   
	//err_printf("add %lx\n", key);
	FetchAndAdd(&numNodes,1);	// N.B. only increment on new node
	// once we decide to extend, we always extend - see comments
	// at extend hash.
	if((2*numNodes > hashMask)) {
	    hashLock.acquire(&lock);
	    locked_extendHash();
	};
	return 1;
    }

    /*
     * search for a key.
     */
    SysStatus find_node(uval key, Element*& desc) {
	HashLock hashLock(useLock);
	uval index;
	Element * next;
	extendHashIfReady(hashLock);
	index = hashMask&Element::Hash(key);
	next = hashTable[index].chainHead;
	while(next) {
	    if(next->getKey() == key && next->isLocked()) {
		desc = next;
		return 0;
	    }
	    next = next->nextHash();
	}
	return _SERROR(2182, 0, ENOENT);
    }

    SysStatus remove_node(uval key, Element*& node) {
	HashLock hashLock(useLock);
	uval index;
	Element *next;
	extendHashIfReady(hashLock);
	sval wasLocked;
	index = hashMask&Element::Hash(key);

	/*
	 * first find a locked node that matches
	 */
	next = hashTable[index].chainHead;
	while(next) {
	    if(next->getKey() == key) {
		node = next;
		// returns + if still locked, 0 if became unlocked
		// - if already unlocked
		wasLocked = next->unLock();
		if(wasLocked == 0) break;
		if(wasLocked > 0) {
		    //err_printf("remove %lx count %lx\n", key, wasLocked);
		    return _SERROR(2183, 0, EBUSY);
		}
		// key matched, but node is inactive, so keep searching
	    }
	    next = next->nextHash();
	}

	if(!next) return _SERROR(2226, 0, ENOENT);
	
	//err_printf("remove %lx \n", key);
	locked_dequeue(next, index, hashLock);
	locked_free_node(next);
	return 0;
    }

    SysStatus remove_node(uval key) {
	Element *dummy;
	return remove_node(key, dummy);
    }
    
    uval removeNext(Element*& node, uval& restart);
    SysStatusUval getFirst(Element*& node);
    SysStatusUval getNext(uval key, Element*& node);
    // user hint about minimum table size, expressed
    // as the number of entries the hash should be able to support
    void numberOfEntries(uval numEntries) {
	lock.acquire();
	if(!useLock) {
	    useLock = &lock;
	    DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(lockMarker);	
	}
	/*
	 * enlarge table to support max of current request,
	 * current number of nodes, and the user hint
	 */
	if (numNodes > extendRequest) extendRequest = numNodes;
	if (numEntries > extendRequest) extendRequest = numEntries;
	lock.release();
    }

};


template<class ALLOC, uval keyshift, uval DefaultSetSize>
class HashSNBBase{
protected:
    class HashSNBNode {
	HashSNBNode* next;
	uval         key;
	uval         datum;
	uval         valid;
    public:
	// use key as free list, since datum and next may be read
	// from a freed node by another thread
	HashSNBNode* nextFree() {
	    return (HashSNBNode*)key;
	}
	void setNextFree(HashSNBNode* node) {
	    key = (uval)node;
	}
	HashSNBNode* nextHash() {
	    return next;
	}
	void setNextHash(HashSNBNode* node) {
	    next = node;
	}
	uval getKey() {
	    return key;
	}
	void setKey(uval keyval) {
	    key = keyval;
	}
	uval getDatum() {
	    return datum;
	}
	void setDatum(uval datumval) {
	    datum = datumval;
	}
	static uval Hash(uval key) {
	    return key>>keyshift;
	}

	void setLock() {
	    valid = 1;
	}
	
	/*
	 * we use a simple valid/non valid lock.
	 * this means that by the time the value is returned to
	 * the caller, it may be gone
	 */
	sval upLock() {
	    return valid;
	}

	/*
	 * 0 if became unused
	 * - if already unused
	 */
	sval unLock() {
	    do {
		if(valid == 0) return -1;;
	    } while(!CompareAndStoreSynced(&valid, 1, 0));
	    return 0;
	}
	uval isLocked() {
	    return valid;
	}
	DEFINE_ALLOC_NEW(HashSNBNode);
    };

    HashNonBlockingBase<ALLOC, HashSNBNode, DefaultSetSize> hashTable;

public:

    HashSNBBase() {};

    void destroy() {hashTable.destroy();};

    void setUseLocking() {hashTable.setUseLocking();};
    
    uval add(uval key, uval& datum);

    uval addOrUpdate(uval key, uval& datum);

    // returns 0 if not found, 1 if found
    uval find(uval key, uval& datum);

    // returns 0 if not found, 1 if found (and returns old datum)
    uval update(uval key, uval& datum);

    // return 0 if not found, 1 if found
    uval remove(uval key, uval& datum);

    // use these to scan entire hash table, assumes entire table remains
    // locked for the duration
    // returns 0 if nothing to remove
    // start with restart=0
    uval removeNext(uval& key, uval& datum, uval& restart);

    // use these to scan table without modifying it.  results will vary
    // if table changes out from under scan
    // return 0 if nothing, non-zero if something
    uval getFirst(uval& key, uval& datum);
    uval getNext(uval& key, uval& datum);
    void numberOfEntries(uval numEntries) {
	hashTable.numberOfEntries(numEntries);
    }
};

template<class K, class T, class ALLOC, uval keyshift,
    uval DefaultSetSize>
class HashSimpleNonBlocking :
	    public HashSNBBase<ALLOC, keyshift, DefaultSetSize> {
public:
    DEFINE_ALLOC_NEW(HashSimpleNonBlocking);

    HashSimpleNonBlocking() { /* empty body */ }

    inline void destroy() {
	HashSNBBase<ALLOC,keyshift, DefaultSetSize>::destroy();
    }

    /*
     * returns 1 if added, 0 if already in
     * if 0, return current value of datum
     */
    inline uval add(K key, T& datum) {
	return HashSNBBase<ALLOC,keyshift, DefaultSetSize>::add(
	    (uval)key,(uval&)datum);
    }

    /*
     * returns 1 if added, 0 if updated
     */
    inline uval addOrUpdate(K key, T& datum) {
	return HashSNBBase<ALLOC,keyshift, DefaultSetSize>::
	    addOrUpdate((uval)key,(uval&)datum);
    }

    inline uval find(K key, T  &datum) {
	return HashSNBBase<ALLOC,keyshift, DefaultSetSize>::find(
	    (uval)key, (uval&)datum);
    }

    inline uval update(K key, T  &datum) {
	return HashSNBBase<ALLOC,keyshift, DefaultSetSize>::update(
	    (uval)key, (uval&)datum);
    }

    inline uval remove(K key, T &datum) {
	return HashSNBBase<ALLOC,keyshift, DefaultSetSize>::remove(
	    (uval)key,(uval&)datum);
    }

    inline uval removeNext(K &key, T &datum, uval& restart) {
        uval retvalue;
	retvalue = HashSNBBase<ALLOC,keyshift, DefaultSetSize>::
	    removeNext((uval&)key,(uval&)datum,restart);
	return(retvalue);
    }

    inline uval getFirst(K &key, T &datum) {
        uval retvalue;
	retvalue = HashSNBBase<ALLOC,keyshift, DefaultSetSize>::
	    getFirst((uval&)key,(uval&)datum);
	return(retvalue);
    }

    inline uval getNext(K &key, T &datum) {
        uval retvalue;
	retvalue = HashSNBBase<ALLOC,keyshift, DefaultSetSize>::
	    getNext((uval&)key,(uval&)datum);
	return(retvalue);
    }
};
#endif /* #ifndef __HASH_NONBLOCKING_H_ */

