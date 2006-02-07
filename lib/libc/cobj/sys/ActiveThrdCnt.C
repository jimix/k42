/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ActiveThrdCnt.C,v 1.2 2004/08/12 13:04:33 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Machinery for keeping counts of active threads in
 * different generations.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/sys/ActiveThrdCnt.H>

uval
ActiveThrdCnt::advance()
{
    uval oldIndex, newIndex;
    uval64 oldIndexAndCnt, newIndexAndCnt;
    sval64 oldCnt;

    /*
     * WARNING:  external synchronization must ensure that this routine
     *           is called serially with respect to itself.
     */

    /*
     * Pick up the current (soon to be old) generation index and determine
     * the new index.
     */
    oldIndex = genIndexAndActivationCnt >> COUNT_BITS;
    newIndex = (oldIndex + 1) & (NUM_GENERATIONS - 1);

    /*
     * We can't advance if the new generation still has active threads
     * from the last go-round.
     */
    if (activeCnt[newIndex] != 0) {
	/*
	 * Register our interest in this counter going to 0.
	 */
	oldCnt = FetchAndOr64Volatile((uval64 *)(&activeCnt[newIndex]),
				      uval64(1));
	if (oldCnt == 0) {
	    /*
	     * The last thread went away before our interest was recorded,
	     * so we have to do the NotifyAdvance().
	     */
	    ActiveThrdCnt_NotifyAdvance(&activeCnt[newIndex]);
	}
	return 0;
    }

    /*
     * Atomically switch to the new generation index, set the activation
     * count associated with the new generation to a large known value, and
     * retrieve the activation count for the old generation.  We set the
     * activation count for the new generation to a large value temporarily
     * so that anyActive() below will never see a too-low number of active
     * threads.  We subtract the large value once we've added the old count
     * to its generation's activeCnt.
     */
    const uval64 HUGE_COUNT = uval64(1) << (COUNT_BITS - 1);

    newIndexAndCnt = (uval64(newIndex) << COUNT_BITS) | HUGE_COUNT;
    oldIndexAndCnt = FetchAndStore64Volatile(&genIndexAndActivationCnt,
					     newIndexAndCnt);
    oldCnt = sval64((oldIndexAndCnt << INDEX_BITS) >> INDEX_BITS);

    /*
     * Atomically add the activation count for the old generation into
     * its active thread count.  Deactivations may have already occurred.
     */
    (void) FetchAndAddSigned64Volatile(&activeCnt[oldIndex], oldCnt);

    /*
     * Now we can subtract out the artificial activation count.  Activations
     * may have occurred, so the count may not be going to zero.
     */
    (void) FetchAndAdd64Volatile(&genIndexAndActivationCnt, -HUGE_COUNT);

    /*
     * We succeeded in advancing to a new generaion.
     */
    return 1;
}

uval
ActiveThrdCnt::anyActive()
{
    sval numActive;
    uval i;

    numActive = sval64((genIndexAndActivationCnt << INDEX_BITS) >> INDEX_BITS);

    for (i = 0; i < NUM_GENERATIONS; i++) {
	numActive += activeCnt[i];
    }

    /*
     * Compare against 2 because the low-order bit is used to register an
     * interest in the count becoming zero.
     */
    return (numActive >= 2);
}

void
ActiveThrdCnt::init()
{
    genIndexAndActivationCnt = 0;
    for (uval i = 0; i < NUM_GENERATIONS; i++) {
	activeCnt[i] = 0;
    }
}

extern "C" void
ActiveThrdCnt_NotifyAdvance(sval64 *cntP)
{
    /*
     * At this point the count should be exactly 1, meaning that someone
     * has registered an interest in the count going to zero, and that
     * there are no active threads.  No one else should be changing the
     * count at this point, so we don't need atomic operations.
     */
    tassertMsg((*cntP) == 1, "Unexpected thread count\n");
    (*cntP) = 0;
    /*
     * Here's where we should tell somebody that advance() will work now.
     */
}
