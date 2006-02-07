/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HashSimple.C,v 1.2 2003/11/25 03:43:44 lbsoares Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "HashSimple.H"

template<class ALLOC,uval keyshift>
HashSimpleBase<ALLOC, keyshift>::HashSimpleBase()
{
    uval index;
    hashMask = InitialHashTableSize-1; hashTable = initialTable;
    numNodes = 0;
    for (index=0; index<=hashMask; index++)
	hashTable[index].chainHead = 0;
}

template<class ALLOC,uval keyshift>
void
HashSimpleLockedBase<ALLOC, keyshift>::destroy()
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    HashSimpleBase<ALLOC, keyshift>::destroy();
}

template<class ALLOC,uval keyshift>
void
HashSimpleBase<ALLOC, keyshift>::destroy()
{
    tassertMsg(numNodes == 0, "destroying a non empty hashtable\n");
    if (hashTable != initialTable) {
	ALLOC::free(hashTable, sizeof(HashNode) * (hashMask+1));
    }
#ifndef NDEBUG
    hashTable = 0;
#endif
}

template<class ALLOC,uval keyshift>
void
HashSimpleLockedBase<ALLOC, keyshift>::extendHash()
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    HashSimpleBase<ALLOC, keyshift>::extendHash();
}

template<class ALLOC,uval keyshift>
void
HashSimpleBase<ALLOC, keyshift>::extendHash()
{
    HashNode *oldTable;
    HashSimpleNode *e, *nexte;
    uval oldMask = hashMask;
    uval oldnumNodes = numNodes;
    uval i, index;
    oldTable = hashTable;
    i = 2*(hashMask+1);
    // if we allocate a table, start at 128 entries
    if (i<128) i=128;
    // but make it at least twice the number in use
    while (2*numNodes >= i) i = 2*i;

    // FIXME FIXME FIXME! Linux allocator does not allow contiguous
    // allocations with more than 131072 bytes!
    if (sizeof(HashNode) * i > 131072) {
	i = 131072 / sizeof(HashNode);
    }

    hashMask = i-1;
    hashTable = (HashNode*) ALLOC::alloc(sizeof(HashNode) * i);
    if (!hashTable) {
	hashTable = oldTable;
	hashMask = oldMask;
	return;
    }
    for (index=0; index <= hashMask; index++)
	hashTable[index].chainHead = 0;
    numNodes = 0;

    for (index=0; index <= oldMask; index++) {
	nexte = oldTable[index].chainHead;
	while ((e = nexte)) {
	    nexte = e->next;
	    enqueue(e);
	}
    }

    tassert(numNodes == oldnumNodes,
	    err_printf("lost a page in extendHash\n"));

    if (oldTable != initialTable) {
	ALLOC::free(oldTable, sizeof(HashNode) * (oldMask+1));
    }

    return;
}

template<class ALLOC,uval keyshift>
void
HashSimpleLockedBase<ALLOC, keyshift>::add(uval key, uval datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    HashSimpleBase<ALLOC, keyshift>::add(key, datum);
}

template<class ALLOC,uval keyshift>
void
HashSimpleBase<ALLOC, keyshift>::add(uval key, uval datum)
{
    HashSimpleNode* node;

    // FIXME FIXME FIXME! Linux allocator does not allow contiguous
    // allocations with more than 131072 bytes!
    if (2*numNodes > hashMask && (hashMask+1)*sizeof(HashNode) != 131072) {
	extendHash();
    }

    tassert({uval temp; find(key, temp) == 0;},
	    err_printf("re-adding not allowed\n"));

    node = new HashSimpleNode(key, datum);

    enqueue(node);
}


template<class ALLOC,uval keyshift>
uval
HashSimpleLockedBase<ALLOC, keyshift>::find(uval key, uval& datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return HashSimpleBase<ALLOC, keyshift>::find(key, datum);
}


// returns 0 if not found, 1 if found
template<class ALLOC,uval keyshift>
uval
HashSimpleBase<ALLOC, keyshift>::find(uval key, uval& datum)
{
    uval index;
    HashSimpleNode *node;

    index = hash(key);
    node = hashTable[index].chainHead;
    while (node != 0) {
	if (node->key == key) {
	    datum = node->datum;
	    return 1;
	}
	node = node->next;
    }
    return 0;
}

template<class ALLOC,uval keyshift>
uval
HashSimpleLockedBase<ALLOC, keyshift>::remove(uval key, uval& datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return HashSimpleBase<ALLOC, keyshift>::remove(key, datum);
}


// return 0 if not found, 1 if found
template<class ALLOC,uval keyshift>
uval
HashSimpleBase<ALLOC, keyshift>::remove(uval key, uval& datum)
{
    uval index;
    HashSimpleNode *node, *prev;

    index = hash(key);
    node = hashTable[index].chainHead;
    prev = 0;
    while (node != 0) {
	if (node->key == key) {
	    if (prev != 0) {
		prev->next = node->next;
	    } else {
		hashTable[index].chainHead = node->next;
	    }
	    datum = node->datum;
	    delete node;
	    numNodes--;
	    return 1;
	}
	prev = node;
	node = node->next;
    }
    return 0;
}

template<class ALLOC,uval keyshift>
uval
HashSimpleLockedBase<ALLOC, keyshift>::removeNext(
    uval& key, uval& datum, uval& restart)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return HashSimpleBase<ALLOC, keyshift>::removeNext(key, datum, restart);
}



// use these to scan entire hash table, assumes entire table remains
// locked for the duration
// returns 0 if nothing to remove
// start with restart=0
template<class ALLOC,uval keyshift>
uval
HashSimpleBase<ALLOC, keyshift>::removeNext(uval& key, uval& datum, uval& restart)
{
    HashSimpleNode* node;

    for (; restart <= hashMask; restart++) {
	if (hashTable[restart].chainHead != 0) {
	    node = hashTable[restart].chainHead;
	    key = node->key;
	    datum = node->datum;
	    hashTable[restart].chainHead = node->next;
	    delete node;
	    numNodes--;
	    return 1;
	}
    }

    return 0;
}

template<class ALLOC,uval keyshift>
uval
HashSimpleLockedBase<ALLOC, keyshift>::getFirst(uval& key, uval& datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return HashSimpleBase<ALLOC, keyshift>::getFirst(key, datum);
}

template<class ALLOC,uval keyshift>
uval
HashSimpleLockedBase<ALLOC, keyshift>::getNextWithFF(uval& key, uval& datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return HashSimpleBase<ALLOC, keyshift>::getNextWithFF(key, datum);
}

template<class ALLOC,uval keyshift>
uval
HashSimpleLockedBase<ALLOC, keyshift>::getNextWithRW(uval& key, 
                                                     uval& datum, uval full)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return HashSimpleBase<ALLOC, keyshift>::getNextWithRW(key, datum, full);
}

// use these to scan entire hash table, assumes entire table remains
// locked for the duration
// returns 0 if nothing found
template<class ALLOC,uval keyshift>
uval
HashSimpleBase<ALLOC, keyshift>::getFirst(uval& key, uval& datum)
{
    HashSimpleNode* node;
    uval restart;

    for (restart=0; restart <= hashMask; restart++) {
	if (hashTable[restart].chainHead != 0) {
	    node = hashTable[restart].chainHead;
	    key = node->key;
	    datum = node->datum;
	    return 1;
	}
    }

    return 0;
}

template<class ALLOC,uval keyshift>
uval
HashSimpleLockedBase<ALLOC, keyshift>::getNext(uval& key, uval& datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return HashSimpleBase<ALLOC, keyshift>::getNext(key, datum);
}



// look for "next" entry after key - will cause a loop if key is not unique
// returns 0 if nothing follows restart node
// return 1 if node following restart was found
// return error if restart node was not found - something changed
// out from under the program walking the table
template<class ALLOC,uval keyshift>
uval
HashSimpleBase<ALLOC, keyshift>::getNext(uval& key, uval& datum)
{
    uval index;
    HashSimpleNode *node;

    index = hash(key);
    node = hashTable[index].chainHead;
    // node is at beginning of hash chain containing restart value
    while (node != 0) {
	if (node->key == key) {
	    // found restart node - search from here
	    node = node->next;
	    if (!node) {
		// nothing more on restart's hash chain, look for
		// a following non-empty hash chain
		for (index++; index <= hashMask; index++) {
		    if (hashTable[index].chainHead != 0) {
			node = hashTable[index].chainHead;
			break;
		    }
		}
		// nothing follows restart node
		if (!node) return 0;
	    }
	    key = node->key;
	    datum = node->datum;
	    return 1;
	}
	node = node->next;
    }
    return _SERROR(1881, 0, ENOENT);
}

/************************************************************
 * FIXME: Wrote these but they have not really been tested! *
 ************************************************************/

//FF can't find it on the chain move forward in the table: might skip entries
//   but does not reprocess.  With FF missing entry are considered not to be a
//   big deal we would rather do an entire sweep  of the table (current hash 
//   space) ensuring that older nodes get visited. With the possibility of 
//   skipping nodes that have been since the search was started.  
//   Typically used when we are perodically sweeping the table in chunks 
//   (each sweep bounded by some number of nodes to process), releasing the
//   lock between periods.
//   As such we will catch the new nodes that we might miss on 
//   a latter sweep, if they are long lived enough, to survive period between 
//   sweep invocations.
// returns 1 if a node that would follow a node identifed by key is found
//           Note this is independent of whether a node identified by key
//           is in the table or not. If it is not we simply move forward in 
//           the table (skipping the chain key would have been on)
// returns 0 if there are no nodes after where a node identified by key would
//           be located.  Either a node with key existed but was the last node
//           in the hash space or it did not exist and there are no others in
//           the table.
template<class ALLOC,uval keyshift>
uval
HashSimpleBase<ALLOC, keyshift>::getNextWithFF(uval& key, uval& datum)
{
    uval index;
    HashSimpleNode *node;

    // locate the chain on which the restart node should be on.
    index = hash(key);
    node = hashTable[index].chainHead;

    // Search hash chain that the node should be on
    if (node!=0) {
        while (node!=0) {
            if (node->key==key) {
                // if we find the restart node then the next node 
                // is the one we want.  If it is null we are at then
                // end the chain so we will have to continue the search
                // but this is taken care of below as node will be set to null.
                node=node->next;
                break;
            }
            node=node->next;
        }
    }

    // If we could not find the restart node or there were no nodes after it
    // on the chain then we look for the first non empty chain following
    // the restart node's chain and identify the head as the next node.
    if (node==0) {
        for (index++; index <= hashMask; index++) {
            if (hashTable[index].chainHead != 0) {
                node = hashTable[index].chainHead;
                break;
            }
        }
    }

    if (node!=0) {
        key = node->key;
        datum = node->datum;
        return 1;
    }

    return 0;
}


//RW if we can not find the node which are supposed to restart the search at
//    by rewinding.  We use this when we are
//    periodically sweeping the table and expect it to be relatively stable
//    between periods and expect that a series of successive sweeps
//    will make there way through the entire table not missing a node.
//    However, if the restart node is missing we would rather rewind either to
//    the begining and not miss new nodes or at least rewind to the 
//    beginning of the hash chain it was supposed to be on or a
//    preceeding chain to restart the search..  This means that we might 
//    reprocess but won't miss items if we can't find our restart node. In the
//    case of a full rewind, a missing entry implies a change that prompts 
//    entire table repocessing.
//    Note this does not imply anything about other changes that might have
//    occured between sweeps.  If we want this we would have to implement 
//    something that notes any adds or deletes between periods and always
//    rewinds if it notices a change.  
//
// input flag: full = 1 indicates a full rewind if we cannot find restart
//             full = 0 indicates a partial rewind starting at the chain
//                      restart should have been ocontinuing backwards until
//                      we reach the beginning. if still cannot find a node
//                      we simply return the first node if the table is not
//                      empty
//
// returns 1 if restart node was found and we have a next or if we did not
//           find restart but we rewound and found any node (eg table not empty)
// returns 0 if restart was found but was the last node or if restart was not 
//           found and the table is empty
template<class ALLOC,uval keyshift>
uval
HashSimpleBase<ALLOC, keyshift>::getNextWithRW(uval& key, uval& datum, 
                                               uval full)
{
    uval index;
    uval found;
    HashSimpleNode *node;

    // locate the chain on which the restart node should be on.        
    index = hash(key);
    node = hashTable[index].chainHead;
    found = 0;
    // Search hash chain that the node should be on
    if (node!=0) {
        while (node!=0) {
            if (node->key==key) {
                // if we find the restart node then the next node 
                // is the one we want.  If it is null we are at the
                // end of the chain but this is taken care of below as node
                //  will be set to null but found = 1;
                found = 1;
                node=node->next;
                break;
            }
            node=node->next;
        }
    }
    if (found == 1) {
        // if we found the restart node but the node after it is empty we 
        // continue search for next
        if (node == 0) {
            for (index++; index <= hashMask; index++) {
                if (hashTable[index].chainHead != 0) {
                    node = hashTable[index].chainHead;
                    break;
                }
            }
            // reached the end of the search restart was the last node
            if (node == 0) return 0;
        }
    } else {
        // could not find the restart node 
        if (full!=1) {
            // if we are only doing a partial rewind then move backwards
            // by starting at the head of the chain that it restart node
            // should have been on 
            node=hashTable[index].chainHead;
            
            // and if thatchain is empty walk backwards in the table
            if (node==0) {
                for (index--; index >= 0; index--) {
                    if (hashTable[index].chainHead != 0) {
                        node = hashTable[index].chainHead;
                        break;
                    }
                }
            }
        }
    }
    // If node is not empty either we found the restart node and identified
    // a node after it or we did not find the restart node but found a
    // a 'preceeding' node in the case of a partial rewind
    if (node!=0) {
        key = node->key;
        datum = node->datum;
        return 1;
    }

    // Either full=1 and we did not find the restart node or full=0
    // and we could not find are predecessor we then try to find the first
    // node.
    return getFirst(key, datum);
}


template HashSimpleBase<AllocGlobal, 0>;
template HashSimpleLockedBase<AllocGlobal, 0>;
