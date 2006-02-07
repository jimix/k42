#ifndef __KFSHASH_H_
#define __KFSHASH_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br).
 *
 * $Id: KFSHash.H,v 1.17 2005/02/23 16:21:32 mostrows Exp $
 *****************************************************************************/

#include <sync/BLock.H>
#include <misc/HashSimple.H>

template<class NODE>
class KFSHashEntry {
    BLock lockv;
    NODE node;
    uval invalid;
public:
    DEFINE_GLOBAL_NEW(KFSHashEntry);
    KFSHashEntry() : invalid(0) { lockv.init(); }
    void setData(NODE n) {
	tassertMsg(!invalid, "?");
	node = n;
    }
    NODE getData() {
	tassertMsg(!invalid, "?");
	return node;
    }
    void unlock() {
	// this entry can be invalid here; removeUnlock() is an example
	_ASSERT_HELD(lockv);
	lockv.release();
    }
    void lock() { lockv.acquire(); }
    uval getInvalid() { return invalid; }
    void setInvalid(uval v) { invalid = v; }
};

template<class KEY, class NODE>
class KFSHash :
    public HashSimpleLocked<KEY,KFSHashEntry<NODE>*,AllocGlobal,0> {
public:
    DEFINE_GLOBAL_NEW(KFSHash);

    KFSHash() {}

    ~KFSHash() {}

    // returns 1 if element was already there; 0 otherwise
    uval findAddLock(KEY key, KFSHashEntry<NODE> **entry) {
	AutoLock<BLock> al(&this->lock);	// locks now, unlocks on return
	KFSHashEntry<NODE> *tmp;
	if (locked_find(key, tmp)) {
	    uval ret = 1;
	    tmp->lock();
	    if (tmp->getInvalid()) {
		tmp->setInvalid(0);
		ret = 0;
	    }
	    *entry = tmp;
	    return ret;
	} else { // no there
	    tmp = new KFSHashEntry<NODE>;
	    tassertMsg(tmp != NULL, "?");
	    tmp->lock();
	    locked_add(key, tmp);
	    *entry = tmp;
	    return 0;
	}
    }

    // returns 1 if element was already there; 0 otherwise
    uval findLock(KEY key, KFSHashEntry<NODE> **entry) {
	AutoLock<BLock> al(&this->lock);	// locks now, unlocks on return
	KFSHashEntry<NODE> *tmp;
	if (locked_find(key, tmp)) {
	    if (!tmp->getInvalid()) {
		tmp->lock();
		*entry = tmp;
		return 1;
	    }
	}
	return 0;
    }

    // it should be locked_find, but the parent has a locked_find and it's
    // not virtual
    uval lockedFind(KEY key, KFSHashEntry<NODE>** entry) {
	_ASSERT_HELD((HashSimpleLocked<KEY,KFSHashEntry<NODE>*,AllocGlobal,0>::lock));
	passertMsg(0, "NIY");
	return 0;
    }

    void removeUnlock(KFSHashEntry<NODE> *entry) {
	tassertMsg(!entry->getInvalid(), "?");
	entry->setInvalid(1);
	entry->unlock();
    }

    uval locked_removeNext(KEY& key, KFSHashEntry<NODE>*& datum,
			   uval &restart) {
	return HashSimpleBase<AllocGlobal, 0>
	    ::removeNext((uval&)key, (uval&)datum, restart);
    }

    void removeAll() {
	AutoLock<BLock> al(&this->lock);	// locks now, unlocks on return
	KFSHashEntry<NODE> *entry;
	KEY key;
	uval restart = 0;
	while (locked_removeNext(key, entry, restart)) {
	    delete entry;
	}
    }

    // returns 1 if there is an element in the hash, 0 otherwise
    // If an element is returned, it is returned locked
    uval getFirstLock(KEY &key, KFSHashEntry<NODE> **n) {
	uval ret;
	do {
	    ret = getFirst(key, *n);
	    if (ret && !(*n)->getInvalid()) {
		(*n)->lock();
		return ret;
	    }
	} while (ret);
	return ret;
    }

    uval locked_getFirstLock(KEY &key, KFSHashEntry<NODE> **n) {
	_ASSERT_HELD((HashSimpleLocked<KEY,KFSHashEntry<NODE>*,AllocGlobal,0>::lock));
	uval elem;
	uval ret;
	do {
	    ret = HashSimpleBase<AllocGlobal, 0>::getFirst((uval)key, elem);
	    *n = (KFSHashEntry<NODE>*)elem;
	    if (ret && !(*n)->getInvalid()) {
		(*n)->lock();
		return ret;
	    }
	} while (ret);
	return ret;
    }

    uval isEmpty() {
	KEY k;
	KFSHashEntry<NODE> *n;
	// do not lock
	uval ret;
	do {
	    ret = getFirst(k, n);
	    if (ret && !n->getInvalid()) {
		return 0;
	    }
	} while (ret);
	return 1;
    }


    uval locked_isEmpty() {
	_ASSERT_HELD((HashSimpleLocked<KEY,KFSHashEntry<NODE>*,AllocGlobal,0>::lock));
	KEY k;
	KFSHashEntry<NODE> *n;
	// do not lock
	uval ret;
	do {
	    ret = HashSimpleBase<AllocGlobal, 0>::getFirst(k, n);
	    if (ret && !n->getInvalid()) {
		return 0;
	    }
	} while (ret);
	return 1;
    }

    // FIXME: get rid of this!!! Only used by a hack for Linux, and only
    // on the private part of tree
    uval isEmptyDontLock() {
	KEY k;
	KFSHashEntry<NODE> *n;
	// do not lock
	uval ret;
	do {
	    ret = HashSimpleBase<AllocGlobal, 0>::getFirst(k, n);
	    if (ret && !n->getInvalid()) {
		return 0;
	    }
	} while (ret);
	return 1;

    }

    // returns 0 if nothing, non-zero if something (skip invalid)
    uval locked_getFirst(KEY& key, KFSHashEntry<NODE>*& datum) {
	_ASSERT_HELD((HashSimpleLocked<KEY,KFSHashEntry<NODE>*,AllocGlobal,0>::lock));
	uval ret;
	do {
	    ret = HashSimpleBase<AllocGlobal, 0>::getFirst((uval&)key,
							   (uval&)datum);
	    if (ret && !datum->getInvalid()) {
		return ret;
	    }
	} while (ret);
	return ret;
    }
    // returns 0 if nothing, non-zero if something (skip invalid)
    uval locked_getNext(KEY& key, KFSHashEntry<NODE>*& datum) {
	_ASSERT_HELD((HashSimpleLocked<KEY,KFSHashEntry<NODE>*,AllocGlobal,0>::lock));
	uval ret;
	do {
	    ret = HashSimpleBase<AllocGlobal, 0>::getNext((uval&)key,
							  (uval&)datum);
	    if (ret && !datum->getInvalid()) {
		return ret;
	    }
	} while (ret);
	return ret;
    }
};

#endif /* !__KFSHASH_H_ */
