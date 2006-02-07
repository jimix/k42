/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SSACSimplePartitionedArray.C,v 1.13 2005/03/02 20:21:54 jappavoo Exp $
 *****************************************************************************/

#include <scheduler/Scheduler.H>
#include <sys/BaseProcess.H>
#include <misc/SSACSimplePartitionedArray.H>

//FIXME: A kludge for the moment does not handle changes in vp count.
#define NUMPROC DREFGOBJ(TheProcessRef)->vpCount()
#define MYVP    Scheduler::GetVP()

#define MYCO(field) ((myco)->field)
#define MYCOGLOBAL(field) (MYCO(root())->field)

template<class CACHEENTRY, class CACHEID, class MYCO> void
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>::HashQueues
::init(const uval numentries)
{
    tassert(!entries,
	    err_printf("\nSSACSimplePartitionedArray::HashQueues:initq:entries!"
		       "=0\n"));
#define _PROFILING 0
#if _PROFILING
    static SysTime getMax = 0;
    SysTime getStart = Scheduler::SysTimeNow();
#endif

    entries=new CACHEENTRY[numentries];
#if _PROFILING
    SysTime getTotal = Scheduler::SysTimeNow() - getStart;
    if (getTotal > getMax) {
	getMax = getTotal;
	err_printf("\n\n------------> NEW get MAX: %lld <-----------n", getMax);
    }
#endif
}

template<class CACHEENTRY, class CACHEID, class MYCO>
CACHEENTRY *
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>::HashQueues
::search(const CACHEID &id,const uval numentries)
{
    // This code assumes that a search target id will never
    // match entries with invalid ids hence no extra check is necessary.
    // All entries are assumed to have there id initially be set to invalid.
    uval i=0;
    while (i<numentries) {
	if (entries[i].isIdMatch(id)) return &(entries[i]);
	i++;
    }
    return (CACHEENTRY *)0;
}

template<class CACHEENTRY, class CACHEID, class MYCO>
CACHEENTRY *
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>::HashQueues
::lruentry(const uval numentries)
{
    // find least recently used
    // Note this could have been combined with first search but then
    // would have add expense to a hit.  this search should benifit
    // from a warmer cache.
     uval i=0;
     CACHEENTRY *ep=0;

    while (i<numentries) {
	if (! entries[i].isBusy()) {
	    if (!ep) {
		ep=&(entries[i]);
		if (!ep->isIdValid()) {
		    break;
		}
	    } else {
		if (!entries[i].isIdValid()) {
		    ep=&(entries[i]);
		    break;
		}
		if ( entries[i].lastUsed() < ep->lastUsed() ) {
		    ep=&(entries[i]);
		}
	    }
	}
	i++;
    }
    return ep;
}

template<class CACHEENTRY, class CACHEID, class MYCO> void
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>::HashQueues
::rollover()
{
    tassert(0,
	    err_printf("\nSSACSimplePartitionedArray::HashQueues:rollover() called\n"));
}

template<class CACHEENTRY, class CACHEID, class MYCO>
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>::HashQueues::HashQueues()
{
    count=0;
    entries=0;
}

template<class CACHEENTRY, class CACHEID, class MYCO>
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>::HashQueues::~HashQueues()
{
    if (entries) {
	delete[] entries;
    }
}

// returns the log 2 of val if it is and exact power of 2 otherwise returns 0
uval
ExactPowerOf2(uval val)
{
    const uval mask = 1;
    uval count = 0;

    if (val == 0 || val == 1) return 0;

    // find first bit set
    while (!((val >> count) & mask)) count++;

    // if all remaining bits are zero then val is an exact power of 2
    if ((val >> (count + 1)) == 0) return count;

    // else return 0
    return 0;
}

template<class CACHEENTRY, class CACHEID, class MYCO>
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>
::SSACSimplePartitionedArray(const uval idxPartSz, const uval nHashQsPerRep,
			     const uval assoc)
{
    //err_printf("SSACSPA:ctor: idxPartSz = %ld\n", idxPartSz);
    log2IndexPartitionSize=ExactPowerOf2(idxPartSz);
    tassert(log2IndexPartitionSize,
	    err_printf("Only supports partitionsizes which are exact powers"
		       " of 2\n"));
    associativity   =assoc;
    numHashQsPerRep =nHashQsPerRep;
    numProcs        =NUMPROC;

    hashqs = new HashQueues[numHashQsPerRep];

#if _DO_ERR_PRINTF
    err_printf("SSACSimplePartitionedArray() numHashqsPerRep=%ld"
	       " associativity=%ld numproc=%ld\n",
	       numHashQsPerRep, associativity, numProcs);
#endif
}


template<class CACHEENTRY, class CACHEID, class MYCO> void
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>
::partition(const uval index, uval &vp, uval &offset)
{
    vp     = (index >> log2IndexPartitionSize) % numProcs;
    offset = index % numHashQsPerRep;
}

template<class CACHEENTRY, class CACHEID, class MYCO> SysStatus
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>
::insertPageDesc(MYCO *myco, const CACHEID &id, CACHEENTRY* &ce)
{
    SysStatus rtn=-1;

    struct HashQueues *hashq;
    // the assignment of ce to ep is here just to avoid compliation warning

    // This class assumes that invalid id's are not real ids and as such never
    // expects to see a request for them.  As such it can use the invalid value
    // to mark entries as free and still do searches against them.
    tassert(id.isValid(),
	    err_printf("\nSSACSimplePartitionedArray::get() target is invalid\n"
		));

    uval vp=0;
    uval index=0;

    partition(id.index(), vp, index);

    if (vp!=MYVP) {
	// findRepOn now requires that the rep list is locked
	//err_printf("SSAC::insertPD(): going remote\n");
	MYCOGLOBAL(lockReps());
	CObjRep *rep=MYCOGLOBAL(locked_findRepOn(vp));
	MYCOGLOBAL(unlockReps());

	if (!rep) {
	    err_printf("SSAC::insertPD(): createRepOn(vp=%ld)\n", uval(vp));
	    MYCO(createRepOn(vp));
	    // findRepOn now requires that the rep list is locked
	    MYCOGLOBAL(lockReps());
	    rep=MYCOGLOBAL(locked_findRepOn(vp));
	    MYCOGLOBAL(unlockReps());

	    tassert(rep, err_printf("failure to find a rep for vp=%ld",vp));
	    err_printf("SSAC::insertPD(): createRepOn() done\n");
	}
	hashq = &(((MYCO *)rep)->_SSACSimplePartitionedArray()->hashqs[index]);
    } else {
	hashq = &(hashqs[index]);
    }


    if (!hashq->entries) {
	hashq->lock.acquire();
	if (vp!=MYVP) {
	    remoteHashqInit(vp,hashq);		//ensure memory is allocated
			                        //locally (due to use of local
			                        // strict
	} else {
	    hashq->init(associativity);
	}
	hashq->lock.release();
    }

    hashq->lock.acquire();

    tassert(!hashq->search(id,associativity), err_printf("Already in cache?"));

    // create CE
    ce=hashq->lruentry(associativity);

    if (ce) {
	if (ce->isDirty()) {
	    //clean the entry
	    MYCO(saveCacheEntry(ce));
	    ce->clearBusy();
	}
	//err_printf(">");
	// Right now I return the CE back to the inserter, who is responsible
	// for filling out the rest of the info. FIXME: revise
	ce->setId(id);
    } else {
	// no free elements take the simple way out for the moment
	// just pass back an error code
	rtn=-1;
	hashq->lock.release();
	return rtn;
    }
    hashq->count++;
    if (!hashq->count) {
	hashq->rollover();
    }
    ce->setLastUsed(hashq->count);
#if 0
    if (type == GETLOCKED) {
	ce->setBusy();
    }
#endif
    hashq->lock.release();
    rtn=1;
    return rtn;
}

template<class CACHEENTRY, class CACHEID, class MYCO> SysStatus
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>::get(
    MYCO *myco, const CACHEID &id, CACHEENTRY* &ce, const Gettype &type)

{
    SysStatus rtn=-1;

    struct HashQueues *hashq;

    // This class assumes that invalid id's are not real ids and as such never
    // expects to see a request for them.  As such it can use the invalid value
    // to mark entries as free and still do searches against them.
    tassert(id.isValid(),
	    err_printf("\nSSACSimplePartitionedArray::get() target is invalid\n"
		));

    uval vp=0;
    uval index=0;

    partition(id.index(), vp, index);
#if 0
    err_printf("index=%ld => l2ips=%ld, vp=%ld(%ld), offset=%ld\n",
	       id.index(), log2IndexPartitionSize, vp, (id.index() >>
		   log2IndexPartitionSize), index);
#endif

    if (vp!=MYVP) {
	// findRepOn now requires that the rep list is locked
	//err_printf("SSAC::get(): going remote\n");
	MYCOGLOBAL(lockReps());
	CObjRep *rep=MYCOGLOBAL(locked_findRepOn(vp));
	MYCOGLOBAL(unlockReps());

	if (!rep) {
	    MYCO(createRepOn(vp));
	    // findRepOn now requires that the rep list is locked
	    MYCOGLOBAL(lockReps());
	    rep=MYCOGLOBAL(locked_findRepOn(vp));
	    MYCOGLOBAL(unlockReps());

	    tassert(rep, err_printf("failure to find a rep for vp=%ld",vp));
	}
	hashq = &(((MYCO *)rep)->_SSACSimplePartitionedArray()->hashqs[index]);
    } else {
	hashq = &(hashqs[index]);
    }

    if (!hashq->entries) {
	hashq->lock.acquire();
	if (vp!=MYVP) {
	    remoteHashqInit(vp,hashq);		//ensure memory is allocated
			                        //locally (due to use of local
			                        // strict
	} else {
	    hashq->init(associativity);
	}
	hashq->lock.release();
    }


 again:
    hashq->lock.acquire();

    ce=hashq->search(id,associativity);

    if (ce) {
	// hit
	if ( ce->isBusy() ) {
	    //err_printf("Busy????\n\n\n");
	    hashq->lock.release();
	    ce->sleep();
	    goto again;
	}
	hashq->count++;
	if (!hashq->count) {
	    hashq->rollover();
	}
	ce->setLastUsed(hashq->count);
	if (type == this->GETLOCKED) {
	    ce->setBusy();
	}
	hashq->lock.release();
	rtn=1;
	//err_printf("H");
	return rtn;
    } else {
	// miss
	ce=hashq->lruentry(associativity);

	if (ce) {
	    if (ce->isDirty()) {
		//clean the entry
		MYCO(saveCacheEntry(ce));
		ce->clearBusy();
	    }
	    ce->setId(id);
	    // FIXME: DANGER: Unique interface dependence on containing object
	    //        And could cause a dead lock as this done with
	    //        lock held.  Must FIX!!!
	    MYCO(loadCacheEntry(ce));
	} else {
	    // no free elements take the simple way out for the moment
	    // just pass back an error code
	    rtn=-1;
	    hashq->lock.release();
	    return rtn;
	}
	hashq->count++;
	if (!hashq->count) {
	    hashq->rollover();
	}
	ce->setLastUsed(hashq->count);
	if (type == SSAC<CACHEENTRY,CACHEID,MYCO>::GETLOCKED) {
	    ce->setBusy();
	}
	hashq->lock.release();
	rtn=1;
	return rtn;
    }
}

template<class CACHEENTRY, class CACHEID, class MYCO> SysStatus
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>
::putbackAndUnlock(MYCO *myco, CACHEENTRY* ce, const Putflag &flag)
{

    struct HashQueues *hashq=0;
    uval vp=0;
    uval index;

    if (!ce) return -1;

    tassert(ce->isBusy(), err_printf("Trying to unlock a an entry which is"
				     " not locked\n"));

    partition(ce->idIndex(),vp,index);

    if (vp!=MYVP) {
	// findRepOn now requires that the rep list is locked
	MYCOGLOBAL(lockReps());
	CObjRep *rep=MYCOGLOBAL(locked_findRepOn(vp));
	MYCOGLOBAL(unlockReps());
	tassert(rep, err_printf("\n **** ERROR: putback:  could not findRepOn"
				" %ld\n",vp));
	hashq = &(((MYCO *)rep)->_SSACSimplePartitionedArray()->hashqs[index]);
    } else {
	hashq = &(hashqs[index]);
    }

    tassert((hashq->entries),
	    err_printf("\n\nSSACSimplePartitionedArray::putback: "
		       "bad queue entry:\n");
	    ce->print());

    hashq->lock.acquire();

    // FIXME:  If we are going to make the sanity checking complete we should put
    //         in a test here to ensure that ce passed in is a pointer to one of
    //         the cache entries on the que identified

    // Do real work of updating the cache entry and unlocking it.
    if (flag == SSAC<CACHEENTRY,CACHEID,MYCO>::KEEP) {
    	ce->setLastUsed(hashq->count);
    } else {
	ce->setLastUsed(0);
    }
    ce->clearBusy();
    hashq->lock.release();
    // Wake up anyone waiting on the entry.
    ce->wakeup();
    return 1;
}

#if 0
template<class CACHEENTRY, class CACHEID, class MYCO> SysStatus
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>::flush(MYCO *myco)
{
     struct HashQueues *hashq;
     CACHEENTRY *ce;
     uval i;

    for (i=0; i<numHashQsPerRep; i++) {
	hashq = &(hashqs[i]);
	if (!hashq->entries) continue;
    again:
	hashq->lock.acquire();
	for (  uval j=0; j<associativity; j++ ) {
	    ce=&(hashq->entries[j]);
	    if ( !ce->isBusy() && ce->isIdValid() &&  ce->isDirty() ) {
		ce->clearBusy();
		hashq->lock.release();
		// FIXME: unique interface dependence on containing object
		MYCO(saveCacheEntry(ce));
		hashq->lock.acquire();
		ce->clearDirty();
		ce->clearBusy();
		hashq->lock.release();
		ce->wakeup();
		goto again;
	    }
	}
	hashq->lock.release();
    }
    return 1;
}
#endif

template<class CACHEENTRY, class CACHEID, class MYCO>
SSACSimplePartitionedArray<CACHEENTRY,CACHEID,MYCO>
::~SSACSimplePartitionedArray()
{
    tassert(hashqs, err_printf("Trying to delete hashqs but it is null\n"));
    delete[] hashqs;
}
