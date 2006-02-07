#ifndef __VPSET_H_
#define __VPSET_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VPSet.H,v 1.15 2002/03/06 13:05:00 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: class for keeping track of a set of vp's
 *                     for now use a bit vector but ...
 * **************************************************************************/


class VPSet {
    uval vec;
public:
    /*
     * Number of VPs the implementation can manage.  Also, one more than
     * the largest valid VPNum, since we number from 0.
     */
    static const VPNum VPLimit = 32;

    VPSet() { init(); }
    void init() {vec = 0;}
    /* next vp in the set in some order, or VPLimit
     * FIXME: need version that can optimize for when only a few bits are set
     */
    VPNum nextVP(VPNum vp, VPNum limit = VPLimit) {
	if ((vec >> (vp+1))==0) return limit;
	for (vp = vp+1; (vp < limit) && !(vec&(uval(1)<<vp)); vp++);
	return vp;
    }
    VPNum firstVP(VPNum limit = VPLimit) {
	VPNum vp;
	if (vec==0) return limit;
	for (vp = 0; (vp < limit) && !(vec & (uval(1)<<vp)); vp++);
	return vp;
    }
    void addVP(VPNum vp) { vec |= (uval(1)<<vp);}
    void addAllInUvalMask(uval bitMask) { vec |= bitMask; }
    void removeVP(VPNum vp) { vec &= ~(uval(1)<<vp); }
    void unite(VPSet set) { vec |= set.vec; }
    void atomicAddVP(VPNum vp) {
	uval oldvec, newvec;
	do { oldvec = vec; newvec = oldvec | (uval(1)<<vp);
	} while (!CompareAndStoreVolatile(&vec, oldvec, newvec));
    }
    void atomicRemoveVP(VPNum vp) {
	uval oldvec, newvec;
	do { oldvec = vec; newvec = oldvec & ~(uval(1)<<vp);
	} while (!CompareAndStoreVolatile(&vec, oldvec, newvec));
    }

    uval atomicRemoveVPAndTestEmpty(VPNum vp) {
	uval oldvec, newvec;
	do { oldvec = vec; newvec = oldvec & ~(uval(1)<<vp);
	} while (!CompareAndStoreVolatile(&vec, oldvec, newvec));
	return newvec == 0;
    }

    void intersect(VPSet other, VPSet *rtn) { rtn->vec = vec & other.vec; }
    void intersect(VPSet other) { vec &= other.vec; }
    uval isSet(VPNum vp)     { return vec & uval(1)<<vp; }
    uval isOnlySet(VPNum vp) { return vec == uval(1)<<vp; }
    uval isEmpty()           { return vec == 0; }

    // for debugging support we allow cast to uval
    uval dbgToUval()         { return vec; }

};




#endif /* #ifndef __VPSET_H_ */
