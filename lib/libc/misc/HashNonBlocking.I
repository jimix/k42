/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HashNonBlocking.I,v 1.11 2003/03/21 21:12:45 marc Exp $
 *****************************************************************************/

template <class ALLOC, class Element, uval DefaultSetSize>
HashNonBlockingBase<ALLOC, Element, DefaultSetSize>::HashNonBlockingBase()
{
    uval index;
    hashMask = (sizeof(initialTable)/sizeof(initialTable[0]))-1;
    hashTable = initialTable;
    numNodes = 0;
    lock.init();
#ifdef ALLOW_NO_NONBLOCKING
    useLocking = 0;
#endif
    useLock = 0;
    extendRequest = 0;
    for(index=0; index<=hashMask; index++)
	hashTable[index].chainHead = 0;
    free = marked = removed = markedArray = removedArray = 0;
    marcPrintKludge = 0;
    DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(marker);
}

/*
 * return 1 if first node exists, 0 if no nodes
 */
template <class ALLOC, class Element, uval DefaultSetSize>
SysStatusUval
HashNonBlockingBase<ALLOC, Element, DefaultSetSize>::getFirst(Element*& node)
{
    HashLock hashLock(useLock);
    extendHashIfReady(hashLock);
    Element *next = 0;
    uval index;

    /*
     * find first (locked) node
     */
    for(index = 0; index <= hashMask; index++) {
	next = hashTable[index].chainHead;
	if(next && next->isLocked()) {
	    node = next;
	    return 1;
	}
	next = next->nextHash();
    }
    return 0;
}

/*
 * return 1 if node after node for key is found,
 * 0 if node for key is last,
 * -1 if key is not found
 * presumes things aren't changing
 */
template <class ALLOC, class Element, uval DefaultSetSize>
SysStatusUval
HashNonBlockingBase<ALLOC, Element, DefaultSetSize>::getNext(uval key, Element*& node)
{
    HashLock hashLock(useLock);
    extendHashIfReady(hashLock);
    Element *next = 0;
    uval index;
    index = hashMask&Element::Hash(key);
    next = hashTable[index].chainHead;
    // find node we provided on previous call
    while(next) {
	if(next->getKey() == key && next->isLocked()) {
	    break;
	}
	next = next->nextHash();
    }

    if(!next) return -1;		// previous key not found

    next = next->nextHash();
    
    while(1) {
	if(next && next->isLocked()) {
	    node = next;
	    return 1;
	}
	if(next) {
	    next = next->nextHash();
	} else {
	    do {
		index += 1;
		if(index > hashMask) return 0; // no more
		next = hashTable[index].chainHead;
	    } while(0==next);
	}
    }
}

/*
 * extend is the expensive operation in the non blocking hash
 * because we need to run blocking (useLock set) while the token
 * circulates.  It is possible that at the instant that the token
 * has circulated, the number of nodes is not large enough to require
 * an extend.  But we extend anyhow, to avoid an oscillation which
 * would keep the hash in blocking mode but never extend!
 * Thats the purpose of extendRequest.
 */

template <class ALLOC, class Element, uval DefaultSetSize>
void
HashNonBlockingBase<ALLOC, Element, DefaultSetSize>::locked_extendHash()
{
    /* we only need the lock to synchronize the initial setting
     * of uselock to non-zero with someone else trying to
     * check the marker before it is set.
     */
    _ASSERT_HELD(lock);
	
    if(!useLock) {
	useLock = &lock;
	extendRequest = numNodes;
	DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(lockMarker);	
#if 0
	err_printf(
	    "pid 0x%lx table 0x%lx lock wait cur 0x%lx request 0x%lx\n",
	    DREFGOBJ(TheProcessRef)->getPID(), uval(this),
	    hashMask+1, extendRequest);
#endif
	return;
    }

    /*
     * If we get here, someone else has set useLock and the lockMarker
     * and we will check to see if its save to expand.
     * Once the lockMarker is not active, there are no threads running
     * without the lock.  Since we have the lock, there are not
     * threads running hash code at all, and we can expand.
     */
	
    COSMgr::MarkerState stat;
    DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(lockMarker, stat);
    if (stat == COSMgr::ACTIVE) {
	return;
    }

#if 0
    err_printf(
	"pid 0x%lx table 0x%lx lock got cur 0x%lx request 0x%lx\n",
	DREFGOBJ(TheProcessRef)->getPID(), uval(this),
	hashMask+1, extendRequest);
#endif

    //err_printf("extend hash %lx %lx\n", hashMask+1, numNodes);
    HashNode *oldTable;
    Element *e, *nexte;
    uval oldMask = hashMask;
    uval oldNumNodes = numNodes;
    uval i, index;
    oldTable = hashTable;
    i = 2*(hashMask+1);
    // if we allocate a table, start at 128 entries
    if(i<128) i=128;
    // but make it at least twice the number in use
    while(2*extendRequest >= i) i = 2*i;
    hashMask = i-1;
    hashTable = (HashNode*) ALLOC::alloc(sizeof(HashNode) * i);
    if(!hashTable) {
	hashTable = oldTable;
	hashMask = oldMask;
	printListSizes();
	passertMsg(0, "out of memory in HashNonBlocking\n");
	return;
    }
    for(index=0; index <= hashMask; index++)
	hashTable[index].chainHead = 0;
    numNodes = 0;
	
    for(index=0; index <= oldMask; index++) {
	nexte = oldTable[index].chainHead;
	while((e = nexte)) {
	    nexte = e->nextHash();
	    locked_enqueue(e);
	}
    }
	
    passertMsg(numNodes == oldNumNodes,
	       "lost a page in extendHash was %ld is %ld\n",
	       oldNumNodes, numNodes);
	
    if(oldTable != initialTable) {
	ALLOC::free(oldTable, sizeof(HashNode) * (oldMask+1));
    }
	
#ifdef ALLOW_NO_NONBLOCKING
    /*
     * allow new threads to start running lock free again
     * By setting the useLocking non zero, you can test
     * performance with locking
     */
    if(!useLocking) {
	useLock = 0;
    }
#else
    useLock = 0;
#endif
    extendRequest = 0;
    return;
}

template <class ALLOC, class Element, uval DefaultSetSize>
SysStatus
HashNonBlockingBase<ALLOC, Element, DefaultSetSize>::alloc_node_slow(
    Element*& desc, HashLock& hashLock)
{
    Element* node;
    COSMgr::MarkerState stat;
    /*
     * precheck for any chance that we can use some previously
     * freed stuff.
     */
    if(marked == 0 && removed == 0 &&
       markedArray == 0 && removedArray == 0) {
	/* just malloc one */
	desc = (Element *)ALLOC::alloc(sizeof(Element));
	return 0;
    }

    DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(marker, stat);
    if (stat == COSMgr::ACTIVE) {
	/* just malloc one */
	desc = (Element *)ALLOC::alloc(sizeof(Element));
	return 0;
    }

    /* really lock, will be freed automagically
     * this prevents more than one attempt to manipulate the
     * free lists at the same time
     */
    hashLock.acquire(&lock);
    // try again, someone else may have provided free entries
    do {
	node = free;
	if(!node) break;
    } while(!CompareAndStoreSynced((uval*)(&free),
				   uval(node), uval(node->nextFree())));
    if(node) {
	desc = node;
	return 0;
    }
	
    /* verify that token has circulated now that we have lock*/
    DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(marker, stat);
    if (stat != COSMgr::ACTIVE) {
	if(marked) {
	    /*
	     * we only touch marked, and only add to free,
	     * under the lock.  So once free is zero while
	     * holding the lock it will stay zero.
	     */
	    free = marked;
	    marked = 0;
	}

	/* move removed nodes to marked state.  Since other
	 * threads may still be adding to removed, this is done
	 * using CAS.
	 */
	do {
	    marked = removed;
	} while(!CompareAndStoreSynced((uval*)&removed, uval(marked), 0));
	    
	/* token has circulated so also fix up the Array stuff.
	 * (We need to figure out how to package this seperately)
	 */
	    
	while(markedArray) {
	    node = markedArray;
	    markedArray = node->nextFree();
	    freePinnedGlobalPadded(node, sizeof(*node)*node->getKey());
	}

	do {
	    markedArray = removedArray;
	} while(!CompareAndStoreSynced(
	    (uval*)&removedArray, uval(markedArray), 0));

	/* record marker for next cycle
	 */
	DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(marker);	
    }
	
    //now try yet again to allocate a node
    do {
	node = free;
	if(!node) break;
    } while(!CompareAndStoreSynced((uval*)(&free),
				   uval(node), uval(node->nextFree())));
    if(!node) {
	/* we don't look at marked again, even though it is
	 * implausibly possible that it is non null and the token
	 * has circulated.
	 */
	node = (Element *)ALLOC::alloc(sizeof(Element));
    }		

    desc = node;
    return 0;
}

template <class ALLOC, class Element, uval DefaultSetSize>
void
HashNonBlockingBase<ALLOC, Element, DefaultSetSize>::locked_dequeue_slow(
    Element *node, uval index, HashLock& hashLock)
{
    /*
     * N.B. you can't do a general unsychronized chain remove
     * using ReadCopyUpdate.  Use the lock.  A cleanup thread is
     * another possibility, but since we do the common case above,
     * this is probably good enough.  Be careful.  The lock only
     * gurantees synchonization with other threads that do an
     * explicit acquire.  The normal non-blocking pathes continue
     * to run asynchonously.
     */
    hashLock.acquire(&lock);
    Element *prev, *next;
again:
    next = hashTable[index].chainHead;
    prev = 0;
    /* note that we must find the node to get prev.
     * we must do this now after the lock is held, since without
     * the lock another remove can change the prev->next chain.
     * Since only the thread that set use count zero tries to
     * remove it, node cannot have disappeared.
     */
    while(next != node) {
	prev = next;
	next = next->nextHash();
	tassertMsg(next, "Someone removed a node improperly\n");
    }
    if(prev) {
	// don't need atomic ops, only code under the lock messes
	// with next pointers in the chain
	prev->setNextHash(node->nextHash());
    } else {
	/* CAS fails if someone beats us by pushing a new node.
	 * Adds don't use the lock, so this can happen at any
	 * time.
	 */
	if(!CompareAndStoreSynced((uval *)(&hashTable[index].chainHead),
				  uval(node), uval(node->nextHash()))) {
	    goto again;
	}
    }
}
/* use these to scan entire hash table, assumes entire table remains
 * assume nothing changes for the duration
 * returns 0 if nothing to remove
 * start with restart=0
 */
template <class ALLOC, class Element, uval DefaultSetSize>
uval
HashNonBlockingBase<ALLOC, Element, DefaultSetSize>::removeNext(Element*& node, uval& restart)
{
    HashLock hashLock(useLock);
    extendHashIfReady(hashLock);
    Element *next = 0;
    sval wasLocked;

    /*
     * find first (locked) node
     */
    for(;restart <= hashMask; restart++) {
	next = hashTable[restart].chainHead;
	while(next) {
	    wasLocked = next->unLock();
	    // returns + if still locked, 0 if became unlocked
	    // - if already unlocked
	    if(wasLocked == 0) goto found;
	    if(wasLocked > 0) {
		passertMsg(0, "removeNext %lx count %ld\n",
			   next->getKey(), wasLocked);
	    }
	    next = next->nextHash();
	}
    }

    if(!next) return 0;
	
found:
    node = next;
    //err_printf("remove %lx \n", key);
    locked_dequeue(next, restart, hashLock);
    locked_free_node(next);
    return 1;
}

template <class ALLOC, class Element, uval DefaultSetSize>
void
HashNonBlockingBase<ALLOC, Element, DefaultSetSize>::destroy()
{
    // arrays are a kludge for Memory.H in linux support
    // which we haven't yet figured out how to lift out of the
    // base.
    passertMsg(markedArray == 0 && removedArray == 0,
	    "no support for destroy if Array's are in use\n");
    if(hashTable != initialTable) {
	ALLOC::free(hashTable, sizeof(HashNode) * (hashMask+1));
    }
    Element *list, *next;
    // free all three lists
    while((list=free?free:marked?marked:removed)) {
	do {
	    next = list->nextFree();
	    ALLOC::free(list, sizeof(Element));
	    list = next;
	} while(list);
    }
}
    

/*
 * returns 0 if already there, 1 if added
 * if alread there, value found is returned
 */
template<class ALLOC,uval keyshift, uval DefaultSetSize>
uval
HashSNBBase<ALLOC, keyshift, DefaultSetSize>::add(uval key, uval& datum)
{
    HashSNBNode *node;

    hashTable.alloc_node(node);

    node->setKey(key);
    node->setDatum(datum);
    node->setLock();
    if(hashTable.find_or_add_node(node)) return 1;
    datum = node->getDatum();
    return 0;
}

/*
 * returns 0 if already there, 1 if added
 * if alread there, datum is updated
 */
template<class ALLOC,uval keyshift, uval DefaultSetSize>
uval
HashSNBBase<ALLOC, keyshift, DefaultSetSize>::addOrUpdate(
    uval key, uval& datum)
{
    HashSNBNode *node;

    hashTable.alloc_node(node);

    node->setKey(key);
    node->setDatum(datum);
    node->setLock();
    if(hashTable.find_or_add_node(node)) return 1;
    uval newdatum = datum;
    datum = node->getDatum();
    node->setDatum(newdatum);
    return 0;
}


// returns 0 if not found, 1 if found
template<class ALLOC,uval keyshift, uval DefaultSetSize>
uval
HashSNBBase<ALLOC, keyshift, DefaultSetSize>::find(uval key, uval& datum)
{
    HashSNBNode *node;
    SysStatus rc;
    rc = hashTable.find_node(key, node);
    if(_SUCCESS(rc)) {
	datum = node->getDatum();
	return 1;
    } else {
	return 0;
    }
}

// returns 0 if not found, 1 if found (and returns old value)
template<class ALLOC,uval keyshift, uval DefaultSetSize>
uval
HashSNBBase<ALLOC, keyshift, DefaultSetSize>::update(uval key, uval& datum)
{
    HashSNBNode *node;
    SysStatus rc;
    rc = hashTable.find_node(key, node);
    if(_SUCCESS(rc)) {
	uval newdatum = datum;
	datum = node->getDatum();
	node->setDatum(newdatum);
	return 1;
    } else {
	return 0;
    }
}

// return 0 if not found, 1 if found
template<class ALLOC,uval keyshift, uval DefaultSetSize>
uval
HashSNBBase<ALLOC, keyshift, DefaultSetSize>::remove(
    uval key, uval& datum)
{
    SysStatus rc;
    rc = hashTable.remove_node(key);
    return _SUCCESS(rc)?1:0;
}

// use these to scan entire hash table, assumes entire table remains
// locked for the duration
// returns 0 if nothing to remove
// start with restart=0
template<class ALLOC,uval keyshift, uval DefaultSetSize>
uval
HashSNBBase<ALLOC, keyshift, DefaultSetSize>::removeNext(
    uval& key, uval& datum, uval& restart)
{
    HashSNBNode* node;
    if(0 == hashTable.removeNext(node, restart)) return 0;
    // even though node is removed, readcopyupdate allows us to read
    // the values!
    key = node->getKey();
    datum = node->getDatum();
    return 1;
}

// use these to scan entire hash table, assumes entire table remains
// unchanged for the duration
// returns 0 if nothing found
template<class ALLOC,uval keyshift, uval DefaultSetSize>
uval
HashSNBBase<ALLOC, keyshift, DefaultSetSize>::getFirst(
    uval& key, uval& datum)
{
    HashSNBNode* node;
    if(0 == hashTable.getFirst(node)) return 0;
    key = node->getKey();
    datum = node->getDatum();
    return 1;
}

// look for "next" entry after key - will cause a loop if key is not unique
// returns 0 if nothing follows restart node
// return 1 if node following restart was found
// return error if restart node was not found - something changed
// out from under the program walking the table
template<class ALLOC,uval keyshift, uval DefaultSetSize>
uval
HashSNBBase<ALLOC, keyshift, DefaultSetSize>::getNext(
    uval& key, uval& datum)
{
    HashSNBNode *node;
    SysStatus rc;
    rc = hashTable.getNext(key, node);
    if(rc == 0) return 0;
    if(_FAILURE(rc)) return rc;
    key = node->getKey();
    datum = node->getDatum();
    return 1;
}
