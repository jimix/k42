#ifndef __TRACE_RESERVE_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __TRACE_RESERVE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceReserve.H,v 1.3 2004/04/12 13:09:17 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines routine to reserve space for an event in
 *                     the trace array.
 * **************************************************************************/

static inline uval
traceTimestamp()
{
#if TRACE_LIB_K42
    /* FIXME -- X86-64 */
    return 0;
#else
    /* FIXME -- X86-64 */
    return uval(Scheduler::SysTimeNow());
#endif
}

#define USE_GENERIC_TRACE_RESERVE
