#ifndef __TRACE_BASE_H_
#define __TRACE_BASE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceBase.H,v 1.12 2004/04/12 13:09:17 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions for generalization of different major IDs
 * **************************************************************************/

/* tracing decarations, methods, etc that are needed only by the k42 base
   tracing and not by any tools that will be built using the trace include
   files
 */

#include <scheduler/Scheduler.H>
#include "trace/trace.H"
#include "trace/traceIncs.H"
#include "trace/traceUnified.H"
#include <sync/atomic.h>

// initialization routines
extern void TraceStartHeartBeat(VPNum vp);
extern void TraceStart(VPNum vp);
extern void TraceReset(VPNum vp);

#endif /* #ifndef __TRACE_BASE_H_ */
