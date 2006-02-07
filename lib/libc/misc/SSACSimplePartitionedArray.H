#ifndef __SSACSIMPLE_PARTITIONED_ARRAY_H_
#define __SSACSIMPLE_PARTITIONED_ARRAY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SSACSimplePartitionedArray.H,v 1.13 2004/10/08 21:40:07 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Sofware Set Associative Cache
 *    These classes have been designed as first cut approach to embedding a
 *    'generic' distributed data structure in a containing clustered object.
 *    There are two classes that make this data structure up.
 *      SSACSimplePartitionedArrayRoot : an instance of which should be embedded
 *                                       in the root class of the containing
 *                                       clustered object.
 *      SSACSimplePartitionedArray : an instance of which should be embedded
 *                                   in the representative class of the
 *                                   clustered object.
 *
 *    Requirements :
 *        Specification of  CacheObjectId and CacheEntry entry classes.
 *        The implementations should inherit the respective classes in SSAC.H
 *
 *        The containing root class must have:
 *         1) A method _SSACSimplePartitionedArrayRoot() which returns a pointer
 *            to the embedded instance of the SSACSimplePartitionedArrayRoot
 *            class
 *
 *        The containing rep class must have:
 *         1) A method _SSACSimplePartitonedArray() which returns a pointer
 *            to the embedded instance of the SSACSimpleParitionedArray class
 *
 *         2) A method loadCacheEntry(CACHEENTRY *ce)
 *
 *         3) A method saveCacheEntry(CACHEENTRY *ce)
 *
 *         4) A method createRepOn(vp)
 * **************************************************************************/
#include <misc/SSAC.H>

class SSACSimplePartitionedArrayRoot {
private:
    SSACSimplePartitionedArrayRoot() { /* empty body */ }
public:
    uval  indexPartitionSize;
    uval  numHashQsPerRep;
    uval  associativity;

    SSACSimplePartitionedArrayRoot(const uval idxPartSz,
	                           const uval nHashQsPerRep,
			           const uval assoc ){
	indexPartitionSize = idxPartSz;
	numHashQsPerRep    = nHashQsPerRep;
	associativity      = assoc;
#if 0
	err_printf("SSACSimplePartitionedArrayRoot indexPartitionSize=%ld"
		   " numHashQsPerRep=%ld associativity=%ld\n",
		   indexPartitionSize, numHashQsPerRep, associativity);
#endif /* #if 0 */
    }

};

template<class CACHEENTRY, class CACHEID, class MYCO>
class SSACSimplePartitionedArray : public SSAC<CACHEENTRY,CACHEID,MYCO> {
    typedef typename SSAC<CACHEENTRY,CACHEID,MYCO>::Putflag Putflag;
    typedef typename SSAC<CACHEENTRY,CACHEID,MYCO>::Gettype Gettype;
    
    struct HashQueues {
	BLock lock;
	uval count;
	CACHEENTRY *entries;

	void init(const uval numentries);
	inline CACHEENTRY *search(const CACHEID &id, const uval numentries);
	inline CACHEENTRY *lruentry(const uval numentries);
	void rollover();
	HashQueues();
	~HashQueues();
	DEFINE_LOCALSTRICT_NEW(typename SSACSimplePartitionedArray::HashQueues);
    } *hashqs;

    struct hashqInitMsg { struct HashQueues *hq; uval nentries; };

    static SysStatus hashqInitMsgHandler(uval msgUval) {
	struct hashqInitMsg *msg=(struct hashqInitMsg *)msgUval;
	(msg->hq)->init(msg->nentries);
	return 1;
    }

    SysStatus remoteHashqInit(uval vp, struct HashQueues *hq) {
	SysStatus rc;
	struct hashqInitMsg msg = { hq, associativity };
	rc=MPMsgMgr::SendSyncUval(Scheduler::GetEnabledMsgMgr(),
				  SysTypes::DSPID(0, vp),
				  hashqInitMsgHandler, uval(&msg), rc);
	tassert(_SUCCESS(rc), err_printf("failure in remote call to "
					 "remoteHashqMsgHandler\n"));
	return rc;
    }


    uval      log2IndexPartitionSize;
    uval      numHashQsPerRep;
    uval      associativity;
    uval      numProcs;

    void partition(const uval index, uval &vp, uval &offset);
public:
    DEFINE_LOCALSTRICT_NEW(SSACSimplePartitionedArray);
    SSACSimplePartitionedArray(const uval idxPartSz,
	                       const uval nHashQsPerRep,
			       const uval assoc);

    SysStatus get(MYCO *myco, const CACHEID &id, CACHEENTRY* &ce,
		  const Gettype &type
		      =(SSAC<CACHEENTRY,CACHEID,MYCO>::GETUNLOCKED));

    SysStatus insertPageDesc(MYCO *myco, const CACHEID &id,
			     CACHEENTRY* &ce);

    SysStatus getAndLock(MYCO *myco, const CACHEID &id, CACHEENTRY* &ce) {
	return get(myco, id,ce,SSAC<CACHEENTRY,CACHEID,MYCO>::GETLOCKED);
    }

    SysStatus putbackAndUnlock(MYCO *myco, CACHEENTRY* ce,
			       const Putflag &flag
			           =(SSAC<CACHEENTRY,CACHEID,MYCO>::KEEP));

#if 0
    SysStatus flush(MYCO *myco);
#endif /* #if 0 */

    ~SSACSimplePartitionedArray();
};
#endif /* #ifndef ... */
