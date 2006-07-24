#ifndef __DHASH_TABLE_H_
#define __DHASH_TABLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DHashTable.H,v 1.20 2005/02/23 16:20:37 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Component class which provide a Clustered Object
 *                     with a Distributed Hash Table.  Place a LocalDHashTable
 *                     in the Rep class and a MasterDHashTable in the root
 *                     class.
 * **************************************************************************/

// FIXME: this example is not up to date. For now, look at
// lib/libc/cobj/missHandlingTest.C (classes TestHashData and TestHashCO).
/*********** EXAMPLE Local and Master Data classes ************************
 *   Note you must provide a classes which implement all methods in the
 *   examples below (you do not have to use two classes nor do the lock
 *   methods really have to do anything if you don't want data locking,
 *   I think).
// Avoiding virtual classes to avoid overhead of virtual function calls.
// Using dumy base classes and templates.

class masterTestData;

class localTestData {
protected:
    uval vaddr;
    uval paddr;
    uval emptyFlg;
    BLock lck;
public:
    localTestData()               { vaddr = 0; paddr = 0; emptyFlg = 0;
                                    lck.init();            }
    inline uval isEmpty()         { return emptyFlg;       }
    inline void setEmpty()        { emptyFlg = 1;          }
    inline void clearEmpty()      { emptyFlg =0;           }
    inline uval getKey()          { return vaddr;          }
    inline void setKey(uval addr) { vaddr = addr;          }
    inline void lock()            { lck.acquire();         }
    inline void unlock()          { lck.release();         }
    inline uval isLocked()        { return lck.isLocked(); }
    void print() { err_printf("vaddr=%lx paddr=%lx emptyFlg=%lx lockstate=%ld",
			      vaddr, paddr, emptyFlg, isLocked()); }
    friend class masterTestData;
};

class masterTestData : public localTestData {
protected:
    uval ppset;
public:
    inline void replicateToLocal(localTestData *local) {
	tassert(lck.isLocked(), err_printf("master is not locked\n"));
	tassert(local->lck.isLocked(), err_printf("local is not locked\n"));
	local->vaddr=vaddr; local->paddr=paddr; local->emptyFlg=emptyFlg;
	// leave locks alone
    }
    masterTestData() { ppset=0; }
    void print() { localTestData::print(); err_printf(" ppset=%ld",ppset); }
};
******************************************************************************/

#include <defines/experimental.H> /* to get  DHASH_RCU_REQUESTCOUNT, which
				   * is NOT on by default. Do not try to turn
				   * it on without knowing what you're doing
				   */

#include <sync/RequestCount.H>

#define DHASH_NO_SHRINK
//#define DHASH_DO_DEBUG

#ifdef DHASH_RCU_REQUESTCOUNT
#define RC_ENTER this->requestCount.enter(&token)
#define RC_LEAVE this->requestCount.leave(token)
#else
#define RC_ENTER this->requestCount.enter()
#define RC_LEAVE this->requestCount.leave()
#endif

#ifndef DHASH_DO_DEBUG
#define DHASH_DBG(stmt)
#else
#define DHASH_DBG(stmt) stmt
#endif

class DHashTableBase {
protected:
    // This is an existence lock for all the memory associated with
    // the hash table.  grow's and shinks can only occur via a thread
    // which has successfully executed a stop on the requestCount.
#ifdef DHASH_RCU_REQUESTCOUNT
    RequestCountWithStopRCU requestCount;
#else
    RequestCountWithStop requestCount;
#endif

    uval log2numBuckets;
    uval minlog2numBuckets;
    class Count {
	uval skip;
	volatile uval num;
    public:
	inline void inc() {
	    if (skip) return;
	    else FetchAndAddSynced(&num,1);
	}
	inline void dec() {
	    if (skip) return;
	    else FetchAndAddSynced(&num,1);
	}
	inline uval value() { return num; }
	inline uval isSkip() { return skip; }
	inline void reset(uval skipCount) { skip=skipCount; num = 0; }
    } numValues;

    volatile uval reSizing;

    inline uval mask() { return (1 << log2numBuckets)  - 1; }

    uval needGrow(uval curSize, uval numVal) {return curSize < (2 * numVal);}
    uval needGrow() { return needGrow(1<<log2numBuckets, numValues.value()); }

    // FIXME:  This is a hack which ensures that
    //         the number of buckets does not grow beyond
    //         4 times the number of active values;
    uval needShrink(uval curSize, uval minSize,  uval numVal) {
	return curSize > minSize && curSize >= (4 * numVal);
    }
    uval needShrink() {
#ifndef DHASH_NO_SHRINK
	return needShrink(1<<log2numBuckets, 1<<minlog2numBuckets,
			  numValues.value());
#else
	return 0;
#endif
    }

    uval calcLog2Buckets(uval log2start, uval log2min, uval numValues) {
	uval i = log2start;
	const uval curNumBuckets = 1 << log2start;

	if (needGrow(curNumBuckets,numValues)) {
	    while (needGrow((1 << i),numValues)) i++;
	} else {
	    uval minNumBuckets = 1 << log2min;
	    if (needShrink(curNumBuckets, minNumBuckets, numValues)) {
		while (needShrink((1 << i), minNumBuckets, numValues)) i--;
	    }
	}
	return i;
    }

    uval newLog2Buckets(uval numValues) {
	uval num=calcLog2Buckets(log2numBuckets, minlog2numBuckets, numValues);
	return num;
    }

public:
    enum { INITNUMENTRIES = 1 };

    uval getNumNonEmpty() { return numValues.value(); }
    typedef enum { FOUND, ALLOCATED } AllocateStatus;
    typedef enum { FAILURE, SUCCESS, LOCALFAILURE } OpStatus;
    typedef enum { LOCKALL, LOCKONEATATIME, LOCKNONE } OpDirective;
    
    typedef uval OpArg;
    DHashTableBase(uval numEntries,uval minEntries, uval skipCount) :
	requestCount() {
      log2numBuckets = calcLog2Buckets(0,0,minEntries);
	minlog2numBuckets = calcLog2Buckets(0,0,numEntries);
      numValues.reset(skipCount); reSizing = 0;
	DHASH_DBG(
	    err_printf("DHash: %p: log2numBuckets=%ld minlog2numBuckets=%ld"
		       " numValues=%ld skipCount=%ld\n", this, log2numBuckets,
		       minlog2numBuckets,
		       numValues.value(), numValues.isSkip())
	    );
    }
};


template <class DATA,class ALLOC> class DHashTable : public DHashTableBase {
public:
    class DHashEntry {
    protected:
	DATA data;
	DHashEntry *nxt;
    public:
	inline uval isEmpty()                 { return data.isEmpty();       }
	inline void clearEmpty()              { return data.clearEmpty();    }
        inline void setEmpty(DHashTableBase::OpArg emptyArg)
	                                     { return data.setEmpty(emptyArg);}
        inline uval match(uval key)           { uval retvalue;
	                                        retvalue =!data.isEmpty() &&
						key == data.getKey();
	                                        return (retvalue);            }
	inline void init()                    { data.init();                 }
	inline void setKey(uval key)          { data.setKey(key);            }
	inline uval getKey()                  { return data.getKey();        }
	inline void lock()                    { data.lock();                 }
	inline void unlock()                  { data.unlock();               }
	inline uval isLocked()                { return data.isLocked();      }
	inline void setData(DATA *d)          { data = *d;                   }
	inline DATA * getData()               { return &data;                }
	inline void setNext(DHashEntry *n)    { nxt=n;                       }
        inline DHashEntry *getNext()          { return nxt;                  }

	void print() { data.print(); err_printf(" nxt=%p\n",nxt); }

	DHashEntry()                            { nxt=0; }
    };

    class AllocHashEntry : public DHashEntry {
    public:
	DEFINE_ALLOC_NEW(AllocHashEntry);
    };

protected:
    class HashBucket {
	DHashEntry *entries;
    public:
	DHashEntry *getFirst() { return entries; }
	DHashEntry *find(uval key) {
	    for (DHashEntry *e=entries; e!=0; e=e->getNext()) {
		if (e->match(key)) return  e;
	    }
	    return 0;
	}

	DHashTableBase::AllocateStatus findOrAllocateAndLock(uval key,
					     DHashEntry **rtnPtr) {
	    DHashEntry *e, *curEntries;
	    DHashTableBase::AllocateStatus rtnVal;
	retry:
	    curEntries = entries;
	    for (e=curEntries; e!=0; e=e->getNext()) {
		if (e->match(key)) break;
	    }
	    if (e) {
		e->lock();
		if (!(e->match(key))) {
		    e->unlock();
		    goto retry;
		}
		rtnVal = DHashTableBase::FOUND;
	    } else {
		AllocHashEntry *ne = new AllocHashEntry;
		ne->init();
		ne->setKey(key);
		ne->setNext(curEntries);
		ne->lock();
		if (!CompareAndStoreVolatile((volatile uval *)&entries,
					     (uval)curEntries,(uval)ne)) {
		    delete ne;
		    goto retry;
		}
		e=ne;
		rtnVal = ALLOCATED;
	    }
	    *rtnPtr = e;
	    return rtnVal;
	}


	void insert(DHashEntry *e) {
	    // THIS is not locked or ATOMIC can only be used when
	    // RequestCount is stopped
	    e->setNext(entries);
	    entries=e;
	}

	DHashEntry *remove() {
	    DHashEntry *rtn=entries;
	    if (rtn) entries=(DHashEntry *)(rtn->getNext());
	    return rtn;
	}
	HashBucket() { entries=0; }
#if 0
        ~HashBucket() {
            err_printf("b:");
            while (entries) {
                err_printf("e");
                delete (AllocHashEntry *)remove();
            }
        }
#endif
	void print() {
	    uval i=0;
	    for (DHashEntry *e=entries; e!=0; e=e->getNext()) {
		err_printf("    entry[%ld]: ", i);
		e->print();
		i++;
	    }
	}
	inline uval isEmpty() { return entries==0; }
	DEFINE_ALLOC_NEW(HashBucket);
    };

    HashBucket *buckets;
#if 0
    ~DHashTable() {
        if (buckets)  {
            err_printf("t:");
            delete[] buckets;
        }
    }
#endif

    inline uval hash(uval key) {return (mask() & DATA::Hash(key));}

    inline HashBucket *findBucket(uval key) {return &(buckets[hash(key)]); }

    DHashEntry * countedLockedFind(uval key) {
	HashBucket *b;
	DHashEntry *e;

	b=findBucket(key);
	e=b->find(key);
	if (e == 0)  return 0;

        tassert(e->isLocked(),
                err_printf("entry for key=%ld not locked\n", key));
	tassert(e->match(key),
                err_printf("what the entry does no match\n"));
	return e;
    }

    DHashEntry * countedFindAndLock(uval key) {
	HashBucket *b;
	DHashEntry *e;

    retry:
	b=findBucket(key);
	e=b->find(key);
	if (e == 0)  return 0;

	e->lock();
	if (!e->match(key)) {
	    e->unlock();
	    goto retry;
	}
	return e;
    }
#if 0
    DHashEntry * findAndLock(uval key) {
	DHashEntry *e;
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif
        RC_ENTER;
	e=countedFindAndLock(key);
        RC_LEAVE;
	return e;
    }
#endif /* #if 0 */

    sval resize() {
        if (!CompareAndStoreSynced(&reSizing,0,1)) return -1;

#ifdef DHASH_RCU_REQUESTCOUNT
        DHASH_DBG(
            err_printf("DHash: calling qdo %p : %ld %ld\n", this,
                       (uval)(1 << log2numBuckets),(uval)numValues.value())
            );
        return requestCount.qdo(resizeEntry, (uval)this);
#else
        DHASH_DBG(
            err_printf("DHash: launching resize %p : %ld %ld\n", this,
                       (uval)(1 << log2numBuckets),(uval)numValues.value())
            );
        Scheduler::ScheduleFunction(resizeEntry,(uval)this);
        return 1;
#endif
    }

    static void resizeEntry(uval objPtr) {
        (void)((DHashTable<DATA,ALLOC> *)objPtr)->doResize();
    }

    sval doResize() {
	// FIXME:  This code removes empty entries during resize
	//         Not sure this is really the right thing to do
	//         when increasing the table. (rather recycle empty entries)
	HashBucket *oldBuckets, *curBuc, *newBuc;
	DHashEntry  *curEnt;
	uval newLog2numBuckets, oldNumBuckets;

        tassert(reSizing==1,
                err_printf("attempting resize with reSizing==0"));

#ifndef DHASH_RCU_REQUESTCOUNT
	if (requestCount.stop() < 0) return -1;
#endif
        newLog2numBuckets = newLog2Buckets(numValues.value());
        // if we find out that we no longer really need a resize we do
        // one anyway to try and avoid oscillating ... seems like a good idea
        // but maybe not :-)
        DHASH_DBG(
            err_printf("DHash: R %p: %ld -> %ld : ", this, log2numBuckets,
                       newLog2numBuckets)
            );
        if (newLog2numBuckets == log2numBuckets) {
            DHASH_DBG(err_printf("DHash: equal resize\n"));
#if 0
            newLog2numBuckets++;
#else
            FetchAndClearSynced(&reSizing);
#ifndef DHASH_RCU_REQUESTCOUNT
            requestCount.restart();
#endif
            return 0;
#endif
        }
        DHASH_DBG(
            err_printf("DHash: %ld -> %ld", log2numBuckets, newLog2numBuckets);
            if (newLog2numBuckets > log2numBuckets) err_printf("+\n");
            else err_printf("-\n")
            );

	oldNumBuckets = 1 << log2numBuckets;
	oldBuckets    = buckets;
	log2numBuckets = newLog2numBuckets;
	buckets=new HashBucket[1 << log2numBuckets];

	for (uval i=0; i<oldNumBuckets; i++) {
	    curBuc = &(oldBuckets[i]);
	    for (curEnt = curBuc->remove(); curEnt != 0;
		 curEnt = curBuc->remove()) {
		if (!curEnt->isEmpty()) {
		    newBuc = &(buckets[hash(curEnt->getKey())]);
		    newBuc->insert(curEnt);
		} else {
		    tassert(!curEnt->isLocked(), err_printf("WHAT!!!\n"));
		    //FIXME:  We don't want to delete Locally Allocated
		    //        memory on remote processors.
                    //        BIG KLUDGE!!!!! resize is only called after
		    //        allocation which is only called on a local
		    //        processor.
		    delete (AllocHashEntry *)curEnt;
		}
	    }
	}
	delete[] oldBuckets;
        FetchAndClearSynced(&reSizing);
#ifndef DHASH_RCU_REQUESTCOUNT
	this->requestCount.restart();
#endif
	return 0;
    }

    //FIXME:  We don't want to delete Locally Allocated
    //        memory on remote processors.
    //        BIG KLUDGE!!!!! checkResize is only called after
    //        allocation which is only called on a local
    //        processor.
    sval checkResize() {
        if (needGrow() || needShrink()) return resize();
	return 0;
    }

    DHashEntry  * getFirst() {
	for (uval i=0; i<((uval)(1 << log2numBuckets)); i++) {
	    if (!(buckets[i].isEmpty())) {
		return buckets[i].getFirst();
	    }
	}
	return 0;
    }

    DHashEntry  * getNext(DHashEntry *p) {
	if (p == 0) return 0;
	if (p->getNext() != 0) return p->getNext();
	for (uval i = hash(p->getKey()) + 1;
	     i<((uval)(1 << log2numBuckets)); i++) {
	    if (!(buckets[i].isEmpty())) {
		return buckets[i].getFirst();
	    }
	}
	return 0;
    }

public:
    // FIXME: very fragile code.  Blindly give destroy everything.
    void cleanup() {
	HashBucket *curBuc;
	DHashEntry  *curEnt;
        uval numBuckets;

        //FIXME: not sure this is right but to be on the safe side
//        err_printf("T:");

//      FIXME:
//      CLEANUP should only be called from gc triggered call hence hash should
//      all ready be quite.  shutdown should not really be required regardless
//      but since it is not yet implemented with the qdo request count we
//      avoid it regardless
#ifndef DHASH_RCU_REQUESTCOUNT
        this->requestCount.shutdown();
#endif
        if (buckets) {
            numBuckets = 1 << log2numBuckets;
            for (uval i=0; i<numBuckets; i++) {
//                err_printf("b:");
                curBuc = &(buckets[i]);
                for (curEnt = curBuc->remove(); curEnt != 0;
                     curEnt = curBuc->remove()) {
//                    err_printf("e");
                    tassert(!curEnt->isLocked(), err_printf("WHAT!!!\n"));
                    //FIXME:  This could cause problems with elements
                    //        are allocated localstrict and deallocated on
                    //        the wrong processor.
                    delete (AllocHashEntry *)curEnt;
                }
            }
        }
        delete[] buckets;
//        err_printf("\n");
    }

    DATA * lockedFind(uval key) {
	DHashEntry *e;
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif

        RC_ENTER;
        e=countedLockedFind(key);
        RC_LEAVE;

	return e->getData();
    }

    DATA * findAndLock(uval key) {
	DHashEntry *e;
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif

        RC_ENTER;
	e = countedFindAndLock(key);
        RC_LEAVE;

	if (e) {
	    return e->getData();
	} else {
	    return NULL;
	}
    }

    void print() {
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif
        RC_ENTER;
	err_printf("HashTable: log2numBuckets=%ld numValues=%ld:\n",
		   log2numBuckets, numValues.value());
	for (uval i=0; i<((uval)(1 << log2numBuckets)); i++) {
	    if (!(buckets[i].isEmpty())) {
		err_printf("  bucket[%ld]:\n",i);
		buckets[i].print();
	    }
	}
        RC_LEAVE;
    }
    DHashTable(uval numEntries,uval minEntries, uval skipCount) :
	DHashTableBase(numEntries,minEntries, skipCount) {
	buckets = new HashBucket[1 << log2numBuckets];
    }
};

template<class MDATA,class LDATA,class MALLOC,class LALLOC>
class MasterDHashTable;

//FIXME:  consider using fixed array of Entries
template<class LDATA,class MDATA,class LALLOC,class MALLOC>
class LocalDHashTable : public DHashTable<LDATA,LALLOC>  {
    friend class MasterDHashTable<MDATA,LDATA,MALLOC,LALLOC>;
    typedef typename DHashTable<LDATA,LALLOC>::DHashEntry DHashEntry;
    typedef typename DHashTable<LDATA,LALLOC>::HashBucket HashBucket;

protected:
    MasterDHashTable<MDATA,LDATA,MALLOC,LALLOC> *masterDHashTable;
//    typedef DHashTable<MDATA,MALLOC>::DHashEntry MasterDHashEntry;
public:
    typedef DHashTableBase::OpStatus (LDATA::* LOP)(DHashTableBase::OpArg);

    void setMasterDHashTable(MasterDHashTable<MDATA,LDATA,MALLOC,LALLOC> *mt) {
	tassert(masterDHashTable==0,;);
	masterDHashTable = mt;
    }

    DHashTableBase::AllocateStatus findOrAllocateAndLock(uval key, LDATA **ld,
					 uval noResize=0) {
	DHashEntry *e;
	HashBucket *b;
#ifdef DHASH_RCU_REQUESTCOUNT
            RequestCountWithStopRCU::RequestToken token;
#endif

        RC_ENTER;
    retry:
	b=this->findBucket(key);
	e=b->find(key);
	if (e) {
	    // Local Hit (Maybe)
	    e->lock();
	    if (e->match(key)) {
		// FIXME:  We can now probably dec ref count here as we
		//         are seperately allocating enteries
                RC_LEAVE;
		*ld = e->getData();
		return DHashTableBase::FOUND;    // confirm hit
	    } else {
		// OOPS something changed e is nolonger what we wanted unlock
		// and retry
		e->unlock();
		goto retry;
	    }
	} else {
	    // Local Miss
            RC_LEAVE;
	    // Must try Global table
	    // cast is necessary to account for possible difference in
	    // allocation type of master and local entry
	    MDATA *md;

	    DHashTableBase::AllocateStatus mStat=
		masterDHashTable->findOrAllocateAndLock(key,&md);

	    RC_ENTER;
	    b=this->findBucket(key);
	    DHashTableBase::AllocateStatus lStat=
		b->findOrAllocateAndLock(key,&e);
	    RC_LEAVE;

	    if (lStat == DHashTableBase::ALLOCATED) {
		this->numValues.inc();
		md->replicateToLocal(Scheduler::GetVP(), e->getData());
		if (!noResize) {
		    (void)this->checkResize();
		}
		tassert(e->match(key), ;);
	    }
	    md->unlock();
	    *ld = e->getData();
	    return mStat;
	}
    }

    void emptyData(LDATA *ld, DHashTableBase::OpArg emptyArg) {
	ld->setEmpty(emptyArg);
	this->numValues.dec();
//	err_printf("DHash: emptyData %p %ld\n", this, this->numValues);
    }

    DHashTableBase::OpStatus doOp(uval key, LOP lo, DHashTableBase::OpArg oa,
				  DHashTableBase::OpDirective od) {
#ifdef DHASH_RCU_REQUESTCOUNT
	RequestCountWithStopRCU::RequestToken token;
#endif

	RC_ENTER;
	HashBucket *b;
	DHashEntry *le;
        LDATA *ld;
	DHashTableBase::OpStatus stat;

	if (od == DHashTableBase::LOCKONEATATIME) {
	    le = this->countedFindAndLock(key);
	} else {
	    b = this->findBucket(key);
	    le = b->find(key);
	}
	if (le == 0) {
            RC_LEAVE;
	    tassert(0, ;);
	    return DHashTableBase::FAILURE;
	}
	ld=le->getData();
	if ((ld->*lo)(oa) == DHashTableBase::FAILURE) {
	    stat = DHashTableBase::FAILURE;
	} else {
	    stat = DHashTableBase::SUCCESS;
	}

	if (od == DHashTableBase::LOCKONEATATIME) le->unlock();

        RC_LEAVE;
	return stat;
    }

    LocalDHashTable(uval numEntries=DHashTableBase::INITNUMENTRIES,
                    uval minEntries=DHashTableBase::INITNUMENTRIES,
                    uval skipCount=0) :
	DHashTable<LDATA,LALLOC>(numEntries,minEntries,skipCount)
        { masterDHashTable=0; }
};


template <class MDATA,class LDATA,class MALLOC,class LALLOC>
class MasterDHashTable : public DHashTable<MDATA,MALLOC> {
protected:
    typedef typename DHashTable<MDATA,MALLOC>::DHashEntry DHashEntry;
    typedef typename DHashTable<MDATA,MALLOC>::HashBucket HashBucket;
    typedef typename DHashTable<LDATA,LALLOC>::DHashEntry LocalDHashEntry;
public:
    typedef DHashTableBase::OpStatus (MDATA::* MOP)(DHashTableBase::OpArg);
    typedef DHashTableBase::OpStatus (LDATA::* LOP)(DHashTableBase::OpArg);

    class EmptyContinuation {
	friend class MasterDHashTable;
	struct LocalInfo {
	    LocalDHashTable<LDATA,MDATA,LALLOC,MALLOC> *lt;
#ifdef DHASH_RCU_REQUESTCOUNT
            RequestCountWithStopRCU::RequestToken token;
#endif
	    LocalDHashEntry *le;
	    LocalInfo *next;
	    DEFINE_LOCALSTRICT_NEW(LocalInfo);  //hmm this probably needs to be
	    //fixed to allow for pinned memory
	};

#ifdef DHASH_RCU_REQUESTCOUNT
            RequestCountWithStopRCU::RequestToken token;
#endif
	MDATA *md;
	LocalInfo *lst;
    public:
	EmptyContinuation() : md(0), lst(0) { /* empty body */ }
	MDATA *getData() { return md; };
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken getRequestToken() { return token; }
#endif
    };

protected:
    //FIXME: Use atomic dynamic array here.
    LocalDHashTable<LDATA,MDATA,LALLOC,MALLOC>
      *localTables[Scheduler::VPLimit];

    DHashTableBase::OpStatus lockedDoLocalOPs(uval key, MDATA *md, LOP lo,
					      DHashTableBase::OpArg oa,
					      DHashTableBase::OpDirective od) {
	LocalDHashTable<LDATA,MDATA,LALLOC,MALLOC> *lt;
	DHashTableBase::OpStatus stat = DHashTableBase::SUCCESS;
	for (VPNum vp = md->firstReplica(); vp != Scheduler::VPLimit;
	     vp = md->nextReplica(vp)) {
	    lt = localTables[vp];
	    tassert(lt!=0, err_printf("huh no ltable on vp=%ld\n",vp));
	    if (lt->doOp(key,lo,oa,od) == DHashTableBase::FAILURE) {
		stat = DHashTableBase::FAILURE;
	    }
	}
	return stat;
    }
#ifdef DHASH_RCU_REQUESTCOUNT
    void buildEmptyContinuation(MDATA *md, EmptyContinuation *ec,
                                RequestCountWithStopRCU::RequestToken token) {
#else
    void buildEmptyContinuation(MDATA *md, EmptyContinuation *ec) {
#endif
	typename EmptyContinuation::LocalInfo *li, *lst=0;
	uval key = md->getKey();
    	for (VPNum vp = md->firstReplica(); vp != Scheduler::VPLimit;
	     vp = md->nextReplica(vp)) {
	    li=new typename EmptyContinuation::LocalInfo;
	    li->next=lst;
	    lst=li;
	    li->lt = localTables[vp];
#ifdef DHASH_RCU_REQUESTCOUNT
	    li->lt->requestCount.enter(&(li->token));
#else
	    li->lt->requestCount.enter();
#endif
	    li->le = li->lt->countedFindAndLock(key);
	    tassert(li->le!=0, ;);
	}
#ifdef DHASH_RCU_REQUESTCOUNT
        ec->token=token;
#endif
	ec->md=md;
	ec->lst=lst;
    }
public:
    void addLTable(VPNum vp, VPNum cs,
		   LocalDHashTable<LDATA,MDATA,LALLOC,MALLOC> *lt) {
	VPSet cluster;
	cluster.addAllInUvalMask(COSMgr::clusterSetMask(vp,cs));
	for (VPNum ltvp = cluster.firstVP(); ltvp != Scheduler::VPLimit;
	     ltvp = cluster.nextVP(ltvp)) {
	    tassert(localTables[ltvp]==0,
		    err_printf("oops ltvp=%ld already has an ltable\n",ltvp));
	    localTables[ltvp]=lt;
	}
    }

    DHashTableBase::AllocateStatus findOrAllocateAndLock(uval key,
					 MDATA **md, uval noResize=0) {
	DHashEntry *e;
	HashBucket *b;
#ifdef DHASH_RCU_REQUESTCOUNT
	RequestCountWithStopRCU::RequestToken token;
#endif

	RC_ENTER;
	b=this->findBucket(key);
	DHashTableBase::AllocateStatus aStat = b->findOrAllocateAndLock(key,
									&e);
	RC_LEAVE;

	if (aStat == DHashTableBase::ALLOCATED) {
	    this->numValues.inc();
	    if (!noResize) {
		(void)this->checkResize();
	    }
	    tassert(e->match(key), ;);
	}
	*md=e->getData();
	return aStat;
    }

#ifdef DHASH_RCU_REQUESTCOUNT
    void addReference(RequestCountWithStopRCU::RequestToken *token) {
        requestCount.enter(token);
    }

    void removeReference(RequestCountWithStopRCU::RequestToken token) {
        requestCount.leave(token);
        //FIXME:  ideally we should probably check for resize here
        //        but due to our desire to keep deallocations local
        //        we don't.  Better think about this in general.
    }
#else
    void addReference() {
        this->requestCount.enter();
    }

    void removeReference() {
        this->requestCount.leave();
        //FIXME:  ideally we should probably check for resize here
        //        but due to our desire to keep deallocations local
        //        we don't.  Better think about this in general.
    }
#endif

    // The loops across the master table using the *Next* methods
    // Must be guarded by addReference and removeReference calls
    // FIXME:  Extend Request Count so that this can be asserted
    void *getNextAndLockAll(void *curr, MDATA **md) {
	DHashEntry *me;

	if (curr == 0) {
	    me = this->getFirst();
	} else {
	    me = (DHashEntry *)curr;
	    me = this->getNext(me);
	}

	for ( ; me != 0; me = getNext(me)) {
	    me->lock();
	    if (me->isEmpty()) {
		me->unlock();
	    } else {
		break;
	    }
	}

	if (me != 0) {
	    *md = me->getData();
	    lockAllReplicas(*md);
	}
	return (void *)me;
    }

    // The loops across the master table using the *Next* methods
    // Must be guarded by addReference and removeReference calls
    // FIXME:  Extend Request Count so that this can be asserted
    void *getNextAndLock(void *curr, MDATA **md) {
	DHashEntry *me;

	if (curr == 0) {
	    me = this->getFirst();
	} else {
	    me = (DHashEntry *)curr;
	    me = this->getNext(me);
	}

	for ( ; me != 0; me = getNext(me)) {
	    me->lock();
	    if (me->isEmpty()) {
		me->unlock();
	    } else {
		break;
	    }
	}

	if (me != 0) {
	    *md = me->getData();
	}

	return (void *)me;
    }

    void lockAllReplicas(MDATA *md) {
	uval  key = md->getKey();
	LDATA *ld;
	LocalDHashTable<LDATA,MDATA,LALLOC,MALLOC> *ltable;

	for (VPNum vp = md->firstReplica(); vp != Scheduler::VPLimit;
	     vp = md->nextReplica(vp)) {
	    ltable=localTables[vp];
	    tassert(ltable!=0, err_printf("huh no ltable on vp=%ld\n",vp));
	    ld=ltable->findAndLock(key);
	    tassert(ld!=0,err_printf("huh no entry for key=%ld on vp=%ld\n",
				     key,vp));
	}
    }

    void unlockAllReplicas(MDATA *md, LDATA *keepThisLeLocked=0)  {
	uval  key = md->getKey();
        LDATA *ld;
	LocalDHashTable<LDATA,MDATA,LALLOC,MALLOC> *ltable;

	for (VPNum vp = md->firstReplica(); vp != Scheduler::VPLimit;
	     vp = md->nextReplica(vp)) {
	    ltable=localTables[vp];
	    tassert(ltable!=0, err_printf("huh no ltable on vp=%ld\n",vp));
            ld=ltable->lockedFind(key);
	    tassert(ld!=0,err_printf("huh no entry for key=%ld on vp=%ld\n",
				     key,vp));
	    tassert(ld->isLocked(),err_printf("entry for key=%ld on vp=%ld"
					      " not Locked\n", key,vp));
	    if (ld!=keepThisLeLocked) {
		ld->unlock();
	    }
	}
    }

    void emptyData(MDATA *md, DHashTableBase::OpArg emptyArg) {
        md->setEmpty(emptyArg);
	this->numValues.dec();
//	err_printf("DHash: emptyData %p %ld\n", this, this->numValues);

    }


#if 0 // these methods seem to be covered by doOp
    DHashEntry * findAndLockAll(uval key) {
	DHashEntry *me = findAndLock(key);

	if (me == 0) return 0;
	lockAllReplicas(me);
    }

    void unlockAll(DHashEntry *me) {
	tassert (me != 0, ;);
	unlockAllReplicas(me);
	me->unlock();
    }
#endif /* #if 0 // these methods seem to be ... */

    // Assumes the md is locked but replicas are not.
    SysStatus startEmpty(MDATA *md, EmptyContinuation *ec) {
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif
        RC_ENTER;
	tassert(md!=0, ;);
	tassert(md->isLocked(), ;);
	if (md->isEmpty()) {
	    tassert(0, err_printf("hmmm does this really make sense\n"));
            RC_LEAVE;
	    return -1;
	}
#ifdef DHASH_RCU_REQUESTCOUNT
	buildEmptyContinuation(md, ec,token);
#else
	buildEmptyContinuation(md, ec);
#endif
	return 0;
    }

    SysStatus startEmpty(uval key, EmptyContinuation *ec) {
	DHashEntry *me;
        MDATA *md;
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif

        RC_ENTER;
	me = this->countedFindAndLock(key);
	if (me == 0) {
            RC_LEAVE;
	    return -1;
	}
	md=me->getData();

	if (md->isEmpty()) {
	    md->unlock();
            RC_LEAVE;
	    return -1;
	}
#ifdef DHASH_RCU_REQUESTCOUNT
	buildEmptyContinuation(md, ec, token);
#else
	buildEmptyContinuation(md, ec);
#endif
	return 0;
    }

    SysStatus finishEmpty(EmptyContinuation *ec,
			  DHashTableBase::OpArg emptyArg) {
        MDATA *md;
	LDATA *ld;
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif
	typename EmptyContinuation::LocalInfo *li, *lst=0;

	if (ec == 0) return -1;

	if (ec->getData() == 0) return -1;

#ifdef DHASH_RCU_REQUESTCOUNT
        token = ec->getRequestToken();
#endif
	md=ec->getData();
	lst=ec->lst;


	//FIXME: Don't see any reason why this is two loops
	//       The order of operations once requestCounts and
	//       locks are held does not matter.

	for (li=lst; li!=0; li=li->next) {
	    ld=li->le->getData();
	    li->lt->emptyData(ld, emptyArg);
	}

	while (lst) {
	    li=lst;
	    li->le->unlock();
#ifdef DHASH_RCU_REQUESTCOUNT
	    li->lt->requestCount.leave(li->token);
#else
	    li->lt->requestCount.leave();
#endif
	    lst=lst->next;
	    delete li;
	}

	emptyData(md, emptyArg);

	md->unlock();
        RC_LEAVE;

	return 0;
    }

    SysStatus doEmpty(uval key, DHashTableBase::OpArg emptyArg) {
	EmptyContinuation ec;

	if (startEmpty(key, &ec) == 0) {
	    return finishEmpty(&ec, emptyArg);
	}
	return -1;
    }

    SysStatus doEmpty(MDATA *md, DHashTableBase::OpArg emptyArg) {
	EmptyContinuation ec;

	if (startEmpty(md, &ec) == 0) {
	    return finishEmpty(&ec, emptyArg);
	}
	return -1;
    }


    // FIXME: keepThisLeLocked is very very Bad.  It only works under
    //        very special conditions in which so other fact is servering
    //        as an existence lock to ensure its validity.  Remove this
    //        and rework the code that uses this feature (FCM code
    //        using doingIO to validate keepThisLeLocked).
    // keepThisLeLocked is only makes sence if you have ensured by other
    // means that it is valid
    DHashTableBase::OpStatus doOp(uval key, MOP mo, LOP lo,
				  DHashTableBase::OpArg oa,
				  LDATA *keepThisLeLocked=0,
				  DHashTableBase::OpDirective od
				      =DHashTableBase::LOCKALL) {
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif

        RC_ENTER;
	  DHashEntry *me;
          MDATA *md;
	  DHashTableBase::OpStatus stat = DHashTableBase::SUCCESS;

	  me = this->countedFindAndLock(key);
	  if (me == 0) {
              RC_LEAVE;
	      return DHashTableBase::FAILURE;
  	  }
	  md=me->getData();
	  if (od == DHashTableBase::LOCKALL) lockAllReplicas(md);
	    if (mo!=0 && (md->*mo)(oa) == DHashTableBase::FAILURE) {
	        stat = DHashTableBase::FAILURE;
	    } else {
	        if (lockedDoLocalOPs(key, md, lo, oa, od) ==
                    DHashTableBase::FAILURE) {
		        stat = DHashTableBase::LOCALFAILURE;
	        }
	    }
	  if (od == DHashTableBase::LOCKALL) {
	      unlockAllReplicas(md,keepThisLeLocked);
	  }
	  me->unlock();
        RC_LEAVE;
	return stat;
    }

    DHashTableBase::OpStatus doOp(MDATA *md, LOP lo, DHashTableBase::OpArg oa,
				  DHashTableBase::OpDirective od
				      =DHashTableBase::LOCKALL) {
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif

        RC_ENTER;
	DHashTableBase::OpStatus stat = DHashTableBase::SUCCESS;

	  tassert(md!=NULL, ;);

	  if (od == DHashTableBase::LOCKALL) lockAllReplicas(md);
             if (lockedDoLocalOPs(md->getKey(), md, lo, oa, od) ==
                     DHashTableBase::FAILURE) {
		    stat = DHashTableBase::LOCALFAILURE;
	        }
	  if (od == DHashTableBase::LOCKALL) unlockAllReplicas(md);
        RC_LEAVE;
	return stat;
    }

    void doOPAll(MOP mo, LOP lo, DHashTableBase::OpArg oa) {
	MDATA *md;
#ifdef DHASH_RCU_REQUESTCOUNT
        RequestCountWithStopRCU::RequestToken token;
#endif

        RC_ENTER;
	for (DHashEntry *me = this->getFirst();
	     me != 0;
	     me = getNext(me)) {
  	    me->lock();
	      if (me->isEmpty()) {
		  me->unlock();
		  continue;
	      }
	      md=me->getData();
	      lockAllReplicas(md);

	        if (mo!=0 && (md->*mo)(oa) == DHashTableBase::FAILURE) {
		    me->unlock();
		    continue;
	        }

	        (void)lockedDoLocalOPs(md->getKey(), md, lo, oa,
                                   DHashTableBase::LOCKNONE);
	      unlockAllReplicas(md,0);
	    me->unlock();
	}
        RC_LEAVE;
    }

    MasterDHashTable(uval numEntries=DHashTableBase::INITNUMENTRIES,
                     uval minEntries=DHashTableBase::INITNUMENTRIES,
                     uval skipCount=0) :
	    DHashTable<MDATA,MALLOC>(numEntries,minEntries,skipCount) {
	for (VPNum vp = 0; vp < Scheduler::VPLimit; vp++) {
	    localTables[vp] = 0;
	}
    }
};


#define TEMPLATEDHASHTABLE(MD,MA,LD,LA)                           \
template class DHashTable<MD,MA>;                                 \
template class DHashTable<LD,LA>;                                 \
template class MasterDHashTable<MD,LD,MA,LA>;                     \
template class LocalDHashTable<LD,MD,LA,MA>;

#endif /* #ifndef __DHASH_TABLE_H_ */
