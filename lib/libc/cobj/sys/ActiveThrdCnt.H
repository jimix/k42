#ifndef __ACTIVE_THRD_CNT_H_
#define __ACTIVE_THRD_CNT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ActiveThrdCnt.H,v 1.7 2004/08/12 13:04:33 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Machinery for keeping counts of active threads in
 * different generations.
 * **************************************************************************/

struct ActiveThrdCnt;

extern ActiveThrdCnt activeThrdCntLocal;
extern "C" void ActiveThrdCnt_NotifyAdvance(sval64 *cntP);

struct ActiveThrdCnt {
    static uval const LOG_NUM_GENERATIONS = 1;
    static uval const NUM_GENERATIONS = (1 << LOG_NUM_GENERATIONS);

    static uval const INDEX_BITS = 8;	// must be >= LOG_NUM_GENERATIONS
    static uval const COUNT_BITS = (64 - INDEX_BITS);

    uval64 genIndexAndActivationCnt;
    sval64 activeCnt[NUM_GENERATIONS];

    /*
     * Locking and synchronization:  We assume that all calls to a given object
     * of this class are made on the same processor, so sync operations are
     * never needed.  External synchronization is required to ensure that
     * advance() is called serially with respect to itself.  Activate() and
     * deactivate() may be called at any time, including during advance().
     */

    sval64 *activate() {
	uval64 indexAndCnt;
	uval index;

	/*
	 * Atomically pick up the current generation index and increment the
	 * current generation activation count.  We increment by 2 because the
	 * low-order bit is used to register an interest in the count going
	 * to zero.
	 */
	indexAndCnt = FetchAndAdd64Volatile(&genIndexAndActivationCnt, 2);
	index = indexAndCnt >> COUNT_BITS;

	/*
	 * Return the address of the current generation's active count, which
	 * deactivate() should decrement.
	 */
	return &activeCnt[index];
    }

    static sval64 *Activate() {
	return activeThrdCntLocal.activate();
    }

    static void Deactivate(sval64 *cntP) {
	/*
	 * Atomically decrement the active thread count location returned
	 * from activate().  We decrement by 2 because the low-order bit is
	 * used to register an interest in the count going to zero.
	 */
	sval64 oldCnt;
	oldCnt = FetchAndAddSigned64Volatile(cntP, sval64(-2));
	if (oldCnt == 3) {
	    /*
	     * The count is now 1, which means that there are no active threads
	     * and that someone has registered an interest in the count going
	     * to zero.
	     */
	    ActiveThrdCnt_NotifyAdvance(cntP);
	}
    }

    uval generation(sval64 *cntP) {
	/*
	 * Convert an active count pointer back to an index, for diagnostic
	 * purposes.
	 */
	return cntP - activeCnt;
    }

    uval advance();
    uval anyActive();
    void init();

    static uval Generation(sval64 *cntP) {
	return activeThrdCntLocal.generation(cntP);
    }

    static uval Advance() {
	return activeThrdCntLocal.advance();
    }

    static uval AnyActive() {
	return activeThrdCntLocal.anyActive();
    }

    static void ClassInit(VPNum /*vp*/) {
	activeThrdCntLocal.init();
    }
};

#endif /* #ifndef __ACTIVE_THRD_CNT_H_ */
