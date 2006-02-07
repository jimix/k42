#ifndef __HASH_SIMPLE_H_
#define __HASH_SIMPLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HashSimple.H,v 1.1 2003/10/30 03:42:47 dilma Exp $
 *****************************************************************************/
/* These classes implement a first cut at general purpose hash
 * functions to replace lists.  Like the lists defined in this
 * directory, these functions come in normal and locked versions.
 * They deal with the association of a key with a datum.  The key and
 * the datum may be any type which can be cast to/from a uval without
 * * loss of information.  In this incarnation, the hash function is a
 * simple right shift, whose size is specified as a template
 * parameter.  This should be improved to pass a hash function.  But
 * it must be done in a way that allows the hash function to inline.
 * The technique used in the C++ libraries is to replace the keyshift
 * template constant with a type which implements the hash function.
 * N.B. these functions allocate storage.  The destroy method must be
 * called to avoid memory leaks.
 * init() can be called after destroy if necessary.
 */

template<class ALLOC,uval keyshift>
class HashSimpleBase {
    // structure used for tracking swap page information; on freelist or hash
    struct HashSimpleNode {
	HashSimpleNode *next;		// next in chain (freelist or hash)
	uval           key;		// offset for this page
	uval           datum;           // value associated with key

	HashSimpleNode(uval k, uval d):key(k), datum(d) { /* empty body */ }

	DEFINE_ALLOC_NEW(HashSimpleNode);
    };


    // separate class for node to allow per-node locking in the future
    struct HashNode {
	HashSimpleNode *chainHead;
    };

    HashNode *hashTable;
    enum {InitialHashTableSize = 4};
    HashNode initialTable[InitialHashTableSize];
    uval   hashMask;			// size of table less 1
    uval   numNodes;

    /* no easy way to override the hash function
     *  when someone needs to, look at how g++ does this for
     *  one approach - namely having the hash function in a class
     *  that is a template paremter
     *  The problem is that hash is NOT a virtual function - don't
     *  want to pay the method dispatch cost
     */
    uval hash(uval key) {
	return (key >> keyshift) & hashMask;
    }

    void enqueue(HashSimpleNode * node)
    {
	uval index;

	index = hash(node->key);
	node->next = hashTable[index].chainHead;
	hashTable[index].chainHead = node;
	numNodes++;
    }

public:
    HashSimpleBase();

#ifndef NDEBUG
    ~HashSimpleBase() {
	tassertMsg(hashTable == 0,
		   "deleting hash table without calling destroy()\n");
    }
#endif

    void destroy();

    void extendHash();

    void add(uval key, uval datum);

    // returns 0 if not found, 1 if found
    uval find(uval key, uval& datum);

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

    // use these to implement periodic scans of the table which can deal
    // semantically with changes in the table. See comments in HashSimple.C
    /************************************************************
     * FIXME: Wrote these but they have not really been tested! *
     ************************************************************/
    // Get next with Fast Forward
    uval getNextWithFF(uval& key, uval& datum);
    // Get next with full (full=1) or partial (full=0) Rewind
    uval getNextWithRW(uval& key, uval& datum, uval full);

};

template<class ALLOC,uval keyshift>
class HashSimpleLockedBase : public HashSimpleBase<ALLOC, keyshift> {
protected:
    BLock lock;

    HashSimpleLockedBase() {lock.init();};

    void destroy();

    void extendHash();

    void add(uval key, uval datum);

    // returns 0 if not found, 1 if found
    uval find(uval key, uval& datum);

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

    // use these to implement periodic scans of the table which can deal
    // semantically with changes in the table. See comments in HashSimple.C
    /************************************************************
     * FIXME: Wrote these but they have not really been tested! *
     ************************************************************/
    // Get next with Fast Forward
    uval getNextWithFF(uval& key, uval& datum);
    // Get next with full (full=1) or partial (full=0) Rewind
    uval getNextWithRW(uval& key, uval& datum, uval full);

    void acquireLock()		{ lock.acquire(); }

    void releaseLock()		{ lock.release(); }
};

template<class K,class T,class ALLOC,uval keyshift>
class HashSimple : public HashSimpleBase<ALLOC, keyshift> {
public:
    DEFINE_ALLOC_NEW(HashSimple);

    HashSimple() { /* empty body */ }

    inline void destroy() {
	HashSimpleBase<ALLOC,keyshift>::destroy();
    }

    inline void extendHash() {
	HashSimpleBase<ALLOC,keyshift>::extendHash();
    }

    inline void add(K key, T datum) {
	HashSimpleBase<ALLOC,keyshift>::add((uval)key,(uval)datum);
    }

    inline uval find(K key, T  &datum) {
	return HashSimpleBase<ALLOC,keyshift>::find((uval)key, (uval &)datum);
    }

    inline uval remove(K key, T &datum) {
	return HashSimpleBase<ALLOC,keyshift>::remove((uval)key,(uval&)datum);
    }

    inline uval removeNext(K &key, T &datum, uval& restart) {
        uval retvalue;
	retvalue = HashSimpleBase<ALLOC,keyshift>::
	    removeNext((uval&)key,(uval&)datum,restart);
	return (retvalue);
    }

    inline uval getFirst(K &key, T &datum) {
        uval retvalue;
	retvalue = HashSimpleBase<ALLOC,keyshift>::
	    getFirst((uval&)key,(uval&)datum);
	return (retvalue);
    }

    inline uval getNext(K &key, T &datum) {
        uval retvalue;
	retvalue = HashSimpleBase<ALLOC,keyshift>::
	    getNext((uval&)key,(uval&)datum);
	return (retvalue);
    }

    inline uval getNextWithFF(K &key, T &datum) {
        uval retvalue;
	retvalue = HashSimpleBase<ALLOC,keyshift>::
	    getNextWithFF((uval&)key,(uval&)datum);
	return (retvalue);
    }

    inline uval getNextWithRW(K &key, T &datum, uval full) {
        uval retvalue;
	retvalue = HashSimpleBase<ALLOC,keyshift>::
	    getNextWithRW((uval&)key,(uval&)datum, full);
	return (retvalue);
    }

};

template<class K,class T,class ALLOC,uval keyshift>
class HashSimpleLocked : public HashSimpleLockedBase<ALLOC, keyshift> {
public:
    DEFINE_ALLOC_NEW(HashSimpleLocked);

    HashSimpleLocked() { /* empty body */ }

    inline void destroy() {
	HashSimpleLockedBase<ALLOC,keyshift>::destroy();
    }

    inline void extendHash() {
	HashSimpleLockedBase<ALLOC,keyshift>::extendHash();
    }

    inline void locked_add(K key, T datum) {
	HashSimpleBase<ALLOC,keyshift>::add((uval)key,(uval)datum);
    }

    inline void add(K key, T datum) {
	HashSimpleLockedBase<ALLOC,keyshift>::add((uval)key,(uval)datum);
    }

    inline uval locked_find(K key, T  &datum) {
        uval retvalue;
	retvalue = HashSimpleBase<ALLOC,keyshift>::
	    find((uval)key, (uval &)datum);
	return (retvalue);
    }

    inline uval find(K key, T  &datum) {
        uval retvalue;
	retvalue = HashSimpleLockedBase<ALLOC,keyshift>::
	    find((uval)key, (uval &)datum);
	return (retvalue);
    }

    inline uval locked_remove(K key, T &datum) {
        uval retvalue;
	retvalue = HashSimpleBase<ALLOC,keyshift>::
	    remove((uval)key,(uval&)datum);
	return (retvalue);
    }

    inline uval remove(K key, T &datum) {
        uval retvalue;
	retvalue = HashSimpleLockedBase<ALLOC,keyshift>::
	    remove((uval)key,(uval&)datum);
	return (retvalue);
    }

    inline uval removeNext(K &key, T &datum, uval& restart) {
        uval retvalue;
	retvalue = HashSimpleLockedBase<ALLOC,keyshift>::
	    removeNext((uval&)key,(uval&)datum,restart);
	return (retvalue);
    }

    inline uval getFirst(K &key, T &datum) {
        uval retvalue;
	retvalue = HashSimpleLockedBase<ALLOC,keyshift>::
	    getFirst((uval&)key,(uval&)datum);
	return (retvalue);
    }

    inline uval getNext(K &key, T &datum) {
        uval retvalue;
	retvalue = HashSimpleLockedBase<ALLOC,keyshift>::
	    getNext((uval&)key,(uval&)datum);
	return (retvalue);
    }

    inline uval getNextWithFF(K &key, T& datum) {
        uval retvalue;
	retvalue = HashSimpleLockedBase<ALLOC,keyshift>::
	    getNextWithFF((uval&)key,(uval&)datum);
	return (retvalue);
    }

    inline uval getNextWithRW(K &key, T &datum, uval full) {
        uval retvalue;
	retvalue = HashSimpleLockedBase<ALLOC,keyshift>::
	    getNextWithRW((uval&)key,(uval&)datum, full);
	return (retvalue);
    }

    inline void acquireLock() {
	HashSimpleLockedBase<ALLOC,keyshift>::acquireLock();
    }

    inline void releaseLock() {
	HashSimpleLockedBase<ALLOC,keyshift>::releaseLock();
    }
};


#endif /* #ifndef __HASH_SIMPLE_H_ */
