#ifndef __SSAC_H_
#define __SSAC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SSAC.H,v 1.7 2002/07/11 17:13:02 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Sofware Set Associative Cache
 * **************************************************************************/
// Avoiding virtual classes to avoid overhead of virtual function calls.
// Using dumy base classes and templates.

class CacheObjectId {
public:
    // CacheObjectId requires an invalid offset that can be tested
    // for
    inline uval index() const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
    inline void invalidate() const
	{ tassert(0,err_printf("must be overidden\n"));}
    inline uval isValid() const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
    inline uval isMatch(const CacheObjectId& other) const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
};

// Here to provided base type for Cache Entries
template<class CACHEID>
class  CacheEntry {
public:
    inline uval isBusy() const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
    inline void setBusy()
	{ tassert(0,err_printf("must be overidden\n"));}
    inline void clearBusy()
	{ tassert(0,err_printf("must be overidden\n")); }
    inline uval isDirty() const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
    inline void setDirty()
	{ tassert(0,err_printf("must be overidden\n")); }
    inline void clearDirty()
	{ tassert(0,err_printf("must be overidden\n")); }
    inline uval lastUsed() const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
    inline void setLastUsed(uval count)
	{ tassert(0,err_printf("must be overidden\n"));}
    inline void setId(const CACHEID &id)
	{ tassert(0,err_printf("must be overidden\n"));}
    inline uval isIdValid() const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
    inline uval isIdMatch(const CACHEID &id) const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
    inline uval idIndex() const
	{ tassert(0,err_printf("must be overidden\n")); return 0;}
    inline void sleep()
	{ tassert(0,err_printf("must be overidden\n"));}
    inline void wakeup()
	{ tassert(0,err_printf("must be overidden\n"));}
    inline void save()
	{ tassert(0,err_printf("must be overidden\n"));}
    inline void load()
	{ tassert(0,err_printf("must be overidden\n"));}
    inline void print()
	{ tassert(0,err_printf("must be overidden\n")); }
};



// Base Class for SSAC classes
template<class CACHEENTRY, class CACHEID, class MYCO> class SSAC {
public:
    enum Putflag { KEEP, DISCARD };
    enum Gettype { GETLOCKED, GETUNLOCKED };

    SysStatus get(MYCO *myco, const CACHEID &id, CACHEENTRY* &ce,
		  const Gettype &type=GETUNLOCKED)

	{ tassert(0,err_printf("must be overidden\n")); return 0;}

    SysStatus getAndLock(MYCO *myco, const CACHEID &id, CACHEENTRY* &ce)
	{ tassert(0,err_printf("must be overidden\n")); return 0;}

    SysStatus putbackAndUnlock(MYCO *myco, CACHEENTRY* ce,
			       const Putflag &flag)
	{ tassert(0,err_printf("must be overidden\n")); return 0;}

    SysStatus flush(MYCO *myco)
	{ tassert(0,err_printf("must be overidden\n")); return 0;}

};

#endif /* #ifndef __SSAC_H_ */
