/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceLib.H,v 1.6 2004/09/29 21:51:22 cascaval Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of tracing facility
 * **************************************************************************/

#include <sync/atomic.h>
#include "trace/traceCore.H"
#include "trace/traceUnified.H"
#include <trace/traceReserve.H>
#include "trace/traceDefault.h"

static inline uval64
traceFormFirstWord(uval timestamp, uval length, uval layerID,
		   uval majorID, uval data)
{
    uval64 retvalue;

#if (TRACE_TIMESTAMP_BITS + TRACE_TIMESTAMP_SHIFT) != 64
    // timestamp may have high order bits set.  If they won't be eliminated
    // by the shift below, eliminate them here.
    timestamp &= TRACE_TIMESTAMP_MASK;
#endif /* #if (TRACE_TIMESTAMP_BITS + ... */

    tassert((length<=TRACE_LENGTH_MASK),err_printf("length too large\n"));
    tassert((majorID<=TRACE_MAJOR_ID_MASK),err_printf("major id too large\n"));
    tassert((data<=TRACE_DATA_MASK),err_printf("data too large\n"));

    retvalue = ((((uval64) timestamp) << TRACE_TIMESTAMP_SHIFT) |
	    (((uval64) length) << TRACE_LENGTH_SHIFT) |
	    (((uval64) layerID) << TRACE_LAYER_ID_SHIFT) |
	    (((uval64) majorID) << TRACE_MAJOR_ID_SHIFT) |
	    (((uval64) data) << TRACE_DATA_SHIFT));
    return (retvalue);
}

static inline void
traceCommit(uval index, uval length)
{
    volatile TraceInfo *const trcInfo = TRACE_INFO;
    TraceControl *const trcCtrl = trcInfo->traceControl;

    AtomicAddVolatile(&(trcCtrl->bufferCount[TRACE_BUFFER_NUMBER_GET(index)]),
		      length);
}

extern "C" void
traceReserveSlow(uval length, uval layerID, uval &index, uval &timestamp)
{
    uval oldIndex, newIndex, offset;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    TraceControl *const trcCtrl = trcInfo->traceControl;
    uval64 *const trcArray = trcInfo->traceArray;

    passert(length <= TRACE_BUFFER_SIZE, err_printf("Trace event too big\n"));

    do {
	oldIndex = trcCtrl->index;
	newIndex = oldIndex + length;
	offset = TRACE_BUFFER_OFFSET_GET(newIndex);
	if ((offset < length) && (offset > 0)) {
	    index = TRACE_BUFFER_OFFSET_CLEAR(newIndex);
	    newIndex = index + length;
	} else {
	    index = oldIndex;

	}
	timestamp = traceTimestamp();
    } while (!CompareAndStore64Volatile(&(trcCtrl->index), oldIndex, newIndex));

    index &= trcInfo->indexMask;

    if (offset < length) {
	newIndex &= trcInfo->indexMask;
	AtomicAddVolatile(&(trcCtrl->
			    bufferCount[TRACE_BUFFER_NUMBER_GET(newIndex)]),
			  -TRACE_BUFFER_SIZE);
			    // Can't simply set to 0 because other producers
			    // may have already started writing.
	if (offset > 0) {
	    oldIndex &= trcInfo->indexMask;
	    trcArray[oldIndex]=traceFormFirstWord(timestamp, length - offset,
						  layerID, 
						  TRACE_DEFAULT_MAJOR_ID,
						  TRACE_DEFAULT_FILLER);
	    traceCommit(oldIndex, length - offset);
	}
	AtomicAddVolatile(&(trcCtrl->buffersProduced), 1);
    }
}

#ifndef TRACE_COMPILE_OUT

extern "C" void
traceDefault0(uval8 majorID, uval16 minorID, uval layerID)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(1, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 1, layerID, 
					 majorID, minorID);

    traceCommit(index, 1);
}

extern "C" void
traceDefault1(uval8 majorID, uval16 minorID, uval layerID, uval64 data1)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(2, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 2, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;

    traceCommit(index, 2);
}

extern "C" void
traceDefault2(uval8 majorID, uval16 minorID, uval layerID,
	      uval64 data1, uval64 data2)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(3, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 3, layerID,
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;

    traceCommit(index, 3);
}

extern "C" void
traceDefault3(uval8 majorID, uval16 minorID, uval layerID, 
	      uval64 data1, uval64 data2, uval64 data3)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2, data3);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(4, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 4, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;
    trcArray[index+3] = data3;

    traceCommit(index, 4);
}

extern "C" void
traceDefault3_1(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
		uval64 data2, uval64 data3, const char* str)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2, data3);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval len=1;
    // we don't need to verify len because traceReserve will check
    if (str) {
	len = strlen(str)+1;
    }

    uval resLen = 4 + ((len-1)/8)+1;

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(resLen, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, resLen, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;
    trcArray[index+3] = data3;
    if (str) {
	memcpy(&trcArray[index+4], str, len-1);
    }
    ((char*)&trcArray[index+4])[len-1]=0;
    traceCommit(index, resLen);
}

extern "C" void
traceDefault4(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	       uval64 data2, uval64 data3, uval64 data4)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2, data3, data4);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(5, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 5, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;
    trcArray[index+3] = data3;
    trcArray[index+4] = data4;

    traceCommit(index, 5);
}

extern "C" void
traceDefault5(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	       uval64 data2, uval64 data3, uval64 data4, uval64 data5)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2, data3, data4, data5);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(6, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 6, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;
    trcArray[index+3] = data3;
    trcArray[index+4] = data4;
    trcArray[index+5] = data5;

    traceCommit(index, 6);
}

extern "C" void
traceDefault6(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5,
	      uval64 data6)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2, data3, data4, data5, data6);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(7, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 7, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;
    trcArray[index+3] = data3;
    trcArray[index+4] = data4;
    trcArray[index+5] = data5;
    trcArray[index+6] = data6;

    traceCommit(index, 7);
}

extern "C" void
traceDefault7(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5,
	      uval64 data6, uval64 data7)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2, data3, data4, data5, data6, data7);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(8, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 8, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;
    trcArray[index+3] = data3;
    trcArray[index+4] = data4;
    trcArray[index+5] = data5;
    trcArray[index+6] = data6;
    trcArray[index+7] = data7;

    traceCommit(index, 8);
}

extern "C" void
traceDefault8(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5,
	      uval64 data6, uval64 data7, uval64 data8)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2, data3, data4, data5, data6, data7, data8);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(9, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 9, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;
    trcArray[index+3] = data3;
    trcArray[index+4] = data4;
    trcArray[index+5] = data5;
    trcArray[index+6] = data6;
    trcArray[index+7] = data7;
    trcArray[index+8] = data8;

    traceCommit(index, 9);
}

extern "C" void
traceDefault9(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5,
	      uval64 data6, uval64 data7, uval64 data8, uval64 data9)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     data1, data2, data3, data4, data5, data6, data7, data8, data9);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    traceReserve(10, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, 10, layerID, 
					 majorID, minorID);

    trcArray[index+1] = data1;
    trcArray[index+2] = data2;
    trcArray[index+3] = data3;
    trcArray[index+4] = data4;
    trcArray[index+5] = data5;
    trcArray[index+6] = data6;
    trcArray[index+7] = data7;
    trcArray[index+8] = data8;
    trcArray[index+9] = data9;

    traceCommit(index, 10);
}

#define TRACE_MAX_STRINGS 10
#define TRACE_MAX_WORDS 10

extern "C" void
traceDefaultGeneric(uval majorID, uval minorID, uval layerID, uval numbWords,
		    uval numbStrs, ...)
{
    uval index, timestamp;
    char *traceStrInd;
    char *storeStrs[TRACE_MAX_STRINGS];
    uval dataWords[TRACE_MAX_WORDS];
    uval lTraceIndex;
    uval i;
    uval len=0;
    va_list ap;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    passert(numbStrs <= TRACE_MAX_STRINGS,
	    err_printf("max number of trace strings per event exceeded\n"));
    passert(numbWords <= TRACE_MAX_WORDS,
	    err_printf("max number of trace dta words per event exceeded\n"));

    va_start(ap, numbStrs);
    for (i=0; i<numbWords; i++) {
	dataWords[i] = va_arg(ap, uval64);
    }

    len = numbWords;
    for (i=0; i<numbStrs; i++) {
	storeStrs[i] = va_arg(ap, char *);
	// pack strings to align on 64 bits and make sure space for \0
	// yes this expression is correct we want to round up accounting for \0
	// we don't need to verify len because traceReserve will check
	if (!(storeStrs[i])) {
	    storeStrs[i] = "";
	}
	len = len + ((strlen(storeStrs[i]))/8) + 1;

    }
    va_end(ap);

    // +1 for header
    traceReserve(len+1, layerID, index, timestamp);
    trcArray[index] = traceFormFirstWord(timestamp, len+1, layerID,
					 majorID, minorID);

    for (i=0; i<numbWords; i++) {
	trcArray[(index+1)+i] = dataWords[i];
    }

    lTraceIndex = (index+1)+numbWords;
    for (i=0; i<numbStrs; i++) {
	traceStrInd = (char *) (&(trcArray[lTraceIndex]));
	memcpy(&trcArray[lTraceIndex], storeStrs[i], strlen(storeStrs[i])+1);
	// pack strings to align 64 bits
	// yes this expression is correct we want to round up accounting for \0
	lTraceIndex += ((strlen(storeStrs[i]))/8) + 1;
    }
    traceCommit(index, len+1);
}

extern "C" void
traceDefaultBytes(uval8 majorID, uval16 minorID, uval layerID,
		  uval len, void *data)
{
    uval resLen, index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    // we don't need to verify len because traceReserve will check
    resLen = ((len-1)/8)+1;

    // +1 for header
    traceReserve(resLen+1, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, resLen+1, layerID,
					 majorID, minorID);

    memcpy(&trcArray[index+1], data, len);

    traceCommit(index, resLen+1);
}

/**
 * Almost the same as traceDefaultBytes, but provides two extra uval64 values
 * before the buffer.
 */  
extern "C" void
traceDefaultPre2Bytes(uval8 majorID, uval16 minorID, uval layerID,
		      uval64 val1, uval64 val2, uval len, void *data)
{
    uval resLen, index, timestamp;

    volatile TraceInfo *const trcInfo = TRACE_INFO;
    uval64 *const trcArray = trcInfo->traceArray;

    // we don't need to verify len because traceReserve will check
    resLen = ((len-1)/8)+1+2;

    // +1 for header
    traceReserve(resLen+1, layerID, index, timestamp);

    trcArray[index] = traceFormFirstWord(timestamp, resLen+1, layerID,
					 majorID, minorID);
    trcArray[index+1] = val1;
    trcArray[index+2] = val2;
    memcpy(&trcArray[index+3], data, len);

    traceCommit(index, resLen+1);
}

extern "C" void
traceDefaultStr(uval8 majorID, uval16 minorID, uval layerID, const char *str)
{
#ifdef PORTING_K42
  err_printf("^%s %s: ",
	     traceUnified[majorID].name,
	     traceUnified[majorID].traceEventParse[minorID].eventString);
  err_printf(traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	     str);
  err_printf("\n");
#endif /* #ifdef PORTING_K42 */

    uval len=0;

    // we don't need to verify len because traceReserve will check
    len = strlen(str);

    // len+1 too account '\0'
    traceDefaultBytes(majorID, minorID, layerID, len+1, (void *)str);
}

#endif /* TRACE_COMPILE_OUT */
