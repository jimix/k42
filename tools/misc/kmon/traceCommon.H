#ifndef __TRACE_COMMON_H_
#define __TRACE_COMMON_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceCommon.H,v 1.5 2005/01/15 02:15:21 bob Exp $
 *****************************************************************************/


typedef int (*traceEvtFn_t)(const traceFileHeaderV3 *headerInfo,
			    const uval64 *event, uval len, bool byteSwap,
			    bool first, bool last, bool verbose);

int processTraceFile(const char *filename, traceEvtFn_t func, bool *byteSwapRef,
		     bool verbose = true);
int processTraceBuffers(traceEvtFn_t func, bool verbose = true);

extern int overrideGarbled, overrideCount, overrideWords;

#endif /* !defined(__TRACE_COMMON_H_) */
