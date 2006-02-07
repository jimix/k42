#ifndef __FCMCACHE_H_
#define __FCMCACHE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMCache.H,v 1.7 2001/10/05 21:49:49 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/
#include "mem/PageDesc.H"

class FCMCacheId : public CacheObjectId {
    // Assumes PAGE_SIZE offsets
    uval offset;
    static const uval INVALIDOFFSET = ~((uval)0);
public:
    // FIXME:  Assumes that the offset has an invalid value which is
    //         never be used externally. To fit assumptions in SSAC code
    FCMCacheId() {offset = INVALIDOFFSET;}
    FCMCacheId(const uval os) {offset=os;}
    FCMCacheId &operator=(const FCMCacheId& src)
	{offset=src.offset; return(*this);}
    FCMCacheId &operator=(const uval src)
	{ offset=src; return *this; }
    uval operator==(const FCMCacheId& other) const
	{return(offset==other.offset);}
    uval operator!=(const FCMCacheId& other) const
	{return(offset!=other.offset);}
    operator uval() const {return offset;}
    inline uval index() const {	return(offset >> LOG_PAGE_SIZE); }
    inline void invalidate()                 {offset = INVALIDOFFSET;}
    inline uval isValid() const              {return(offset != INVALIDOFFSET);}
    inline uval isMatch(const FCMCacheId& other) const
	{return(offset == other.offset);}
};


template<class CACHEID>
class FCMCacheEntry : public CacheEntry<CACHEID> {
    CACHEID offset;   // FIXME: Not using offset in PageDesc. Please remove
                     //        From PageDesc.H
    struct myPageDesc : PageDesc {
	DEFINE_NOOP_NEW(myPageDesc);
	virtual void destroy() {delete this;}
    }  pd;
    uval lastused;
    enum {BACKOFF=10000};
public:
    inline uval isBusy() const            {return(pd.doingIO==PageDesc::SET);}
    inline void setBusy()                 {pd.doingIO=PageDesc::SET;}
    inline void clearBusy()               {pd.doingIO=PageDesc::CLEAR;}
    inline uval isDirty() const           {return(pd.dirty==PageDesc::SET);}
    inline void setDirty()                {pd.dirty=PageDesc::SET;}
    inline void clearDirty()              {pd.dirty=PageDesc::CLEAR;}
    inline uval lastUsed() const          {return(lastused);}
    inline void setLastUsed(uval count)   {lastused=count;}
    inline void setId(const CACHEID &id)  {offset=id;}
    inline uval isIdValid() const     	  {return(offset.isValid()); }
    inline uval isIdMatch(const CACHEID &id) const {return offset.isMatch(id);}
    inline uval idIndex() const     {return offset.index();}
    inline void setPD(const PageDesc &locPD) { (PageDesc &)pd = locPD; }

    // FIXME: Just somemore kludges for the moment
    inline void sleep()	 { uval i=0; while(i<BACKOFF) i++; }
    inline void wakeup() { return; }
    inline void save()	 {tassert(0,err_printf("not implemented yet\n"));}
    inline void load()   {tassert(0,err_printf("not implemented yet\n"));}
    inline void print()	 {tassert(0,err_printf("not implemented yet\n"));}

    inline uval paddr() const            {return pd.paddr;}
    inline void setPaddr(const uval pa)  {pd.paddr=pa;}
    FCMCacheEntry() {pd.init(0,0,0);}
    DEFINE_LOCALSTRICT_NEW(FCMCacheEntry);
};
#endif /* #ifndef __FCMCACHE_H_ */
