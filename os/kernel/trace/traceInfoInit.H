#ifndef __TRACE_INFO_H_
#define __TRACE_INFO_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceInfoInit.H,v 1.1 2004/04/21 13:07:24 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions for tracing facility
 * **************************************************************************/

void
traceInfoInit(TraceInfo *traceInfo)
{
    traceInfo->mask = 0;
    traceInfo->indexMask = 0;
    traceInfo->traceControl = NULL;
    traceInfo->traceCounters = NULL;
    traceInfo->traceArray = NULL;
    traceInfo->traceArrayPhys = NULL;
    traceInfo->numberOfBuffers = 0;
    traceInfo->tracedRunning = 0;
}

#endif /* #ifndef __TRACE_INFO_H_ */
