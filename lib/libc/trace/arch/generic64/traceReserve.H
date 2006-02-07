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
 * $Id: traceReserve.H,v 1.2 2001/10/05 21:48:47 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines routine to reserve space for an event in
 *                     the trace array.
 * **************************************************************************/

static inline uval
traceTimestamp()
{
  return 0;
}

#define USE_GENERIC_TRACE_RESERVE

