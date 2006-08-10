#ifndef __PAGE_DESC_DATA_H_
#define __PAGE_DESC_DATA_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageDescData.H,v 1.9 2003/08/22 12:33:30 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines basic functionality common to many page
 * descriptors.  This is used within FCMs to try to maintain some common
 * code inheritence.  It should not be leaked outside of FCMs, since we
 * do not want to constrain the implementation of FCMs to a particular
 * page descriptor, at least not from an interface perspective.
 * **************************************************************************/
#include "PageDesc.H"
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>
#include <misc/DHashTable.H>

class MasterPageDescData;

struct PageDescData : PageDesc {
    virtual void destroy() { delete this; }
};

class LocalPageDescData {
protected:
    PageDescData pd;
    uval empty;
    BLock lck;
public:

    struct EmptyArg {
	uval doNotify;
	PageFaultNotification *skipFn;
	SysStatus rc;
    };
    DEFINE_NOOP_NEW(LocalPageDescData);
    LocalPageDescData()           {
	                            pd.init(0,0,0);
                                    pd.doingIO = PageDesc::SET;
                                    clearEmpty();
                                    lck.init();
                                                           }
    // For the moment we are hardcoding len to be page size;
    inline void init()            { pd.len = PAGE_SIZE;    }
    static inline uval Hash(uval key)    { return key >> LOG_PAGE_SIZE; }
    inline uval isEmpty()         { return empty;          }
    inline void clearEmpty()      { empty = 0;             }
    inline uval getKey()          { return pd.fileOffset;  }
    inline void setKey(uval key)  { pd.fileOffset = key;   }
    inline void lock()            { lck.acquire();         }
    inline void unlock()          { lck.release();         }
    inline uval isLocked()        { return lck.isLocked(); }
    inline uval getPAddr()        { return pd.paddr;       }
    inline uval getLen()          { return pd.len;         }
    inline void setPAddr(uval p)  { pd.paddr = p;          }
    inline uval getFileOffset()   { return pd.fileOffset;  }
    inline void setEstablished()  { pd.established = PageDesc::SET; }
    inline uval getPinCount()     { return pd.pinCount; }
    inline void setPinCount(uval cnt)
	                          { pd.pinCount = cnt;  }
    inline PageFaultNotification *getFN()
				  { return pd.fn;          }
    inline uval isDoingIO()       { return pd.doingIO;     }
    inline uval isFreeAfterIO()   { return pd.freeAfterIO; }
    inline void setFreeAfterIO()  { pd.freeAfterIO = PageDesc::SET; }
    inline uval isForkIO()        { return pd.forkIO;         }
    inline uval isDirty()         { return pd.dirty;       }
    inline uval isCacheSynced()   { return pd.cacheSynced; }
    inline uval isFree()          { return pd.free;        }
    inline uval isMapped()        { return pd.mapped;      }
    inline uval getMapped()       { return pd.mapped;      } 
    inline void setMapped()       { pd.mapped = PageDesc::SET; }
    inline void clearMapped()     { pd.mapped = PageDesc::CLEAR; }
    inline void setDoingIO()      { pd.doingIO = PageDesc::SET; }
    inline void clearDoingIO()    { pd.doingIO = PageDesc::CLEAR; }
    inline void setCacheSynced()  { pd.cacheSynced = PageDesc::SET; }
    inline void setDirty()        { pd.dirty = PageDesc::SET; }
    inline void setFN(PageFaultNotification *fn)
				  { pd.fn = fn;            }
    inline void setPP(VPNum pp)   { pd.ppset |= (1 << pp); }
    inline uval getPPSet()        { return pd.ppset;       }
    inline void clearPPSet()      { pd.ppset = 0;          }
    inline void clearFree()       { pd.free = PageDesc::CLEAR; }
    virtual void destroy()        { delete this; }
    void print() { err_printf("vaddr=%lx paddr=%lx emptyFlg=%lx lockstate=%ld",
			      pd.fileOffset, pd.paddr, empty, isLocked()); }

    void notify(SysStatus rc, PageFaultNotification *skipFn);
    void setEmpty(DHashTableBase::OpArg ptrEmptyArg);
    DHashTableBase::OpStatus doSetPAddr(DHashTableBase::OpArg addr);
    struct IOCompleteArg {
	uval setDirty;
	PageFaultNotification *skipFn;
	SysStatus rc;
    };

#if 0
    struct PutArg {
	uval clearDirty;
	PageFaultNotification *skipFn;
	SysStatus rc;
    };
#endif /* #if 0 */

    DHashTableBase::OpStatus doIOComplete(
	DHashTableBase::OpArg ioCompleteArgPtr);
#if 0
    DHashTableBase::OpStatus doPut(DHashTableBase::OpArg putArgPtr);
#endif /* #if 0 */
    DHashTableBase::OpStatus doSetCacheSync(DHashTableBase::OpArg dummy);
    DHashTableBase::OpStatus doSetDoingIO(DHashTableBase::OpArg dummy);
    DHashTableBase::OpStatus doUnmap(DHashTableBase::OpArg ppsetPtr);
    DHashTableBase::OpStatus doSetDirty(DHashTableBase::OpArg dummy);
    DHashTableBase::OpStatus doSetFreeAfterIO(DHashTableBase::OpArg dummy);
    DHashTableBase::OpStatus doSetPAddrAndIOComplete(DHashTableBase::OpArg
                                                     addr);
    friend class MasterPageDescData;

    virtual ~LocalPageDescData() { }


};

class MasterPageDescData : public LocalPageDescData {
protected:
    VPSet replicaVPs;
public:
    inline void replicateToLocal(VPNum vp, LocalPageDescData *local) {
	tassert(lck.isLocked(), err_printf("master is not locked\n"));
	tassert(local->lck.isLocked(), err_printf("local is not locked\n"));
	local->empty=empty;
	local->pd = pd;
	local->pd.fn = NULL;
	// leave locks alone
	replicaVPs.addVP(vp);
    }
    VPNum firstReplica()        { return replicaVPs.firstVP();  }
    VPNum nextReplica(VPNum vp) { return replicaVPs.nextVP(vp); }
    MasterPageDescData() { }

    DHashTableBase::OpStatus doCacheSync(DHashTableBase::OpArg dummy);
    DHashTableBase::OpStatus doDirty(DHashTableBase::OpArg dummy);

    void print() { LocalPageDescData::print(); }
    void copyFrom(PageDescData *pdesc) { pd = *pdesc; }
};

#endif /* #ifndef __PAGE_DESC_DATA_H_ */
