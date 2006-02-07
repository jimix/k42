#ifndef __GEN_STATE_H_
#define __GEN_STATE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: GenState.H,v 1.10 2005/07/15 17:14:24 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines container for passing io state with gen count.
 * **************************************************************************/
#include <sys/SysStatus.H>

class GenState {
public:
    union {
	struct {
	    uval32 stamp;
	    uval16 stamp2;
	    uval16 state;
	};
	uval64 fullVal;
    };

    GenState() {
	fullVal = 0;
    }

    void operator=(const GenState& src) {
	fullVal = src.fullVal;
    }

    void operator=(const volatile GenState& src) {
	fullVal = src.fullVal;
    }

    void operator=(const GenState& src) volatile {
	fullVal = src.fullVal;
    }

    void operator=(const volatile GenState& src) volatile {
	fullVal = src.fullVal;
    }

    void makeNewer(const volatile GenState &x) {
	stamp = x.stamp + 1; // may wrap
    }

    uval isNewer(GenState x) volatile {
	/*
	 * Return true iff x is strictly newer than *this, relying on
	 * 32-bit unsigned arithmetic and on the assumption that x and *this
	 * are logically no more than half the counter range apart.
	 */
	return (((stamp - x.stamp) - 1) < 0x80000000);
    }

    void setInFuture(uval s) volatile {
	GenState next;
	GenState old;
	do {
	    old.fullVal = fullVal;
	    next.state = s;
	    /*
	     * Set the generation half way to the horizon:  it will
	     * appear newer than the next billion or so updates.  And if
	     * any of the last billion are still in transit, they will
	     * still be rejected as old.
	     */
	    next.stamp = old.stamp + 0x40000000; // may wrap
	} while (!CompareAndStore64(&fullVal, old.fullVal, next.fullVal));
    }

    uval setIfNewer(GenState x) volatile {
	GenState old;
	do {
	    old.fullVal = fullVal;
	    if (!x.isNewer(old)) {
		return 0;
	    }
	} while (!CompareAndStore64(&fullVal, old.fullVal, x.fullVal));
	return 1;
    }
};

#endif /* #ifndef __GEN_STATE_H_ */
