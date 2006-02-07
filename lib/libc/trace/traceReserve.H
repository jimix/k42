#ifndef __TRACE_RESERVE_H_
#define __TRACE_RESERVE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceReserve.H,v 1.8 2004/04/12 13:09:17 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines routine to reserve space for an event in
 *                     the trace array.
 * **************************************************************************/

extern "C" void traceReserveSlow(uval length, uval layerID,
				 uval &index, uval &timestamp);

#include __MINC(traceReserve.H)

/*
 * Generic implementation of traceReserve() for use on architectures that
 * don't need a machine-dependent version.
 */
#ifdef USE_GENERIC_TRACE_RESERVE
static inline void
traceReserve(uval length, uval layerID, uval &index, uval &timestamp)
{
    uval oldIndex, newIndex;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    TraceControl *const trcCtrl = trcInfo->traceControl;

    do {
	oldIndex = trcCtrl->index;
	newIndex = oldIndex + length;
	if (TRACE_BUFFER_OFFSET_GET(newIndex) < length) {
	    traceReserveSlow(length, layerID, index, timestamp);
	    return;
	}
	timestamp = traceTimestamp();
    } while (!CompareAndStore64Volatile(&(trcCtrl->index), oldIndex, newIndex));
    index = oldIndex & trcInfo->indexMask;
}
#endif /* #ifdef USE_GENERIC_TRACE_RESERVE */

#endif /* #ifndef __TRACE_RESERVE_H_ */
