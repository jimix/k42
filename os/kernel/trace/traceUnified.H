#ifndef __TRACE_UNIFIED_H_
#define __TRACE_UNIFIED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceUnified.H,v 1.7 2004/04/12 13:09:18 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions for generalization of different major IDs
 * **************************************************************************/

// we use this as a generalization of all different majorIDs of tracing
// so that we can loop over this array rather than having to write
// out the statements in many different places
// CAUTION this should be in the same order as above enum for majorID
typedef struct {
    char *name, *upName;
    uval majorID, enumMax, mask;
    TraceEventParse *traceEventParse;
} TraceUnified;

// declared in traceUtils.C
extern TraceUnified traceUnified[];

#endif /* #ifndef __TRACE_UNIFIED_H_ */
