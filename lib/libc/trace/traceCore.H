#ifndef __TRACE_CORE_H_
#define __TRACE_CORE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceCore.H,v 1.85 2005/05/28 19:42:41 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: definitions for tracing facility
 * **************************************************************************/

/*
 * a quick "readme" for tracing
 *
 * all variables, constant, macros, and functions start with trace
 *
 * to add one new event in a pre-existing class see traceReadme
 *
 * to add an entire new class of events see traceReadme
 *

 below is the definition of the parse language

 each entry contains three fields: the minorID and two strings
 the first string is the packing format of the event - see immediately below
 the second string is a modified printf string: the standard % arguments
   are packaged into brackets and follow a % number string indicating the
   order to print the packed arguments in. e.g.,
   "32 str"
   "This is a str %1[%s] and int %0[%ld]"
   means print the string following str and the integer following int

 the language of the packing format string is:

 token* left-paren token* right-paren

 where
   token : 8 | 16 | 32 | 64 | str-ident
   str-ident : str number
   number : positive integer indicating the length the string including \0
   left-paren : (
   right-paren : )
   tokens are separated by spaces

   example

    { __TR(TRACE_TEST_STR_DATA), "64 str str",
      "here is data %0[%llx] str1 %1[%s] str2 %2[%s]"},

   also note that strings must be NULL terminated
 *
 * All tracing information is stored into the circular traceArray.
 *  traceControl->index holds the next location to be used.  The array is
 *  partitioned (logically) into buffers.  Events are not allowed to cross
 *  buffer boundaries.  Filler events are added if necessary.
 *

 A TraceInfo structure is embedded in kernelInfoLocal, which is a
 one-per-physical-processor read-only data structure that is shared by all
 VPs running on that processor.  The trace mask and tracing parameters are
 located in this structure.  The TraceInfo structure contains pointers to
 a read-write TraceControl structure and to the actual trace array (which is
 also read-write, obviously).  The main value stored in the TraceControl
 structure is the current index into the trace array.  The trace array is one
 page larger than necessary.  The trace daemon uses the extra page when
 writing a trace file header.

 An array of counters, an alternative to full-fledged tracing, is packaged
 with the trace mechanism for convenience.

  */


//#define TRACE_COMPILE_OUT

// macro that is used turn constant in the traceXxxEventParse array
// to be used both as the constant and as a string
#define __TR(C) C,#C

// A trace index is composed of two parts, a buffer number and a buffer offset.
// The actual number of buffers allocated is a run-time decision, although it
// must be a power of two for efficient computation.  We define a maximum
// number of bits for the buffer number, because the bufferCount array in
// traceControl must have a fixed size.  OFFSET_BITS must be at least as
// large as TRACE_LENGTH_BITS (since a buffer must be able to hold an event
// of maximum size).  Making OFFSET_BITS larger reduces fragmentation.  Making
// it smaller increases trace responsiveness.

#define TRACE_BUFFER_NUMBER_BITS 8
#define TRACE_BUFFER_OFFSET_BITS 16

#define TRACE_MAX_BUFFER_NUMBER (1ULL << TRACE_BUFFER_NUMBER_BITS)
#define TRACE_BUFFER_SIZE (1ULL << TRACE_BUFFER_OFFSET_BITS)
#define TRACE_BUFFER_OFFSET_MASK (TRACE_BUFFER_SIZE - 1)

#define TRACE_BUFFER_NUMBER_GET(index) ((index) >> TRACE_BUFFER_OFFSET_BITS)
#define TRACE_BUFFER_OFFSET_GET(index) ((index) & TRACE_BUFFER_OFFSET_MASK)
#define TRACE_BUFFER_OFFSET_CLEAR(index) ((index) & ~TRACE_BUFFER_OFFSET_MASK)


// defines used to create and manage first word in event
#define TRACE_TIMESTAMP_BITS 32
#define TRACE_TIMESTAMP_SHIFT 32
#define TRACE_TIMESTAMP_MASK ((1ULL << TRACE_TIMESTAMP_BITS) - 1)
#define TRACE_TIMESTAMP_GET(x) \
	    (((x) >> TRACE_TIMESTAMP_SHIFT) & TRACE_TIMESTAMP_MASK)

#define TRACE_LENGTH_BITS 8
#define TRACE_LENGTH_SHIFT 24
#define TRACE_LENGTH_MASK ((1ULL << TRACE_LENGTH_BITS) - 1)
#define TRACE_LENGTH_GET(x) \
	    (((x) >> TRACE_LENGTH_SHIFT) & TRACE_LENGTH_MASK)

#define TRACE_LAYER_ID_BITS 4
#define TRACE_LAYER_ID_SHIFT 20
#define TRACE_LAYER_ID_MASK ((1ULL << TRACE_LAYER_ID_BITS) - 1)
#define TRACE_LAYER_ID_GET(x) \
	    (((x) >> TRACE_LAYER_ID_SHIFT) & TRACE_LAYER_ID_MASK)

#define TRACE_MAJOR_ID_BITS 6
#define TRACE_MAJOR_ID_SHIFT 14
#define TRACE_MAJOR_ID_MASK ((1ULL << TRACE_MAJOR_ID_BITS) - 1)
#define TRACE_MAJOR_ID_GET(x) \
	    (((x) >> TRACE_MAJOR_ID_SHIFT) & TRACE_MAJOR_ID_MASK)

#define TRACE_DATA_BITS 14
#define TRACE_DATA_SHIFT 0
#define TRACE_DATA_MASK ((1ULL << TRACE_DATA_BITS) - 1)
#define TRACE_DATA_GET(x) \
	    (((x) >> TRACE_DATA_SHIFT) & TRACE_DATA_MASK)

#define TRACE_OVERFLOW_WRAP 0
#define TRACE_OVERFLOW_STOP_MASK_ON 1
#define TRACE_OVERFLOW_STOP_MASK_OFF 2
#define TRACE_OVERFLOW_STOPPED_MASK_ON 3
#define TRACE_OVERFLOW_STOPPED_MASK_OFF 4

struct TraceTimerEvent;

typedef struct {
    // index is a 64 bit qunatity the upper 32 bits are the consumer index
    //   the lower 32 bits are the producer index
    volatile uval64 index;
    volatile uval buffersProduced;
    volatile uval buffersConsumed;

    // a flag that tells whether we are in a critical place and not to pause
    //   to write any trace events out
    volatile uval writeOkay;

    volatile uval bufferCount[TRACE_MAX_BUFFER_NUMBER];
} TraceControl;

typedef struct {
    // mask MUST be first variable in structure - we will set the pointer
    //      to this structure to 64 bits of 0's so that all the inline
    //      checks of mask will not work until we initialize everything
    // the Mask which we check to see if a particular class of tracing
    // is on followed by the mask values
    uval64 mask;

    // indexMask is initialized based on the number of buffers that are
    // actually allocated.  This needs to be 64 bits to guarantee it's
    // consistent with index and that we mask of all the upper bits in index
    uval64 indexMask;

    // overflowBehavior describes what to do when buffer is full
    // choices our to stop and throw away new events or to wrap and
    // throw away old events
    uval64 overflowBehavior;

    // pointers to writable structures
    TraceControl *traceControl;
    uval64 *traceCounters;
    uval64 *traceArray;

    uval64 *traceArrayPhys; // physical version traceArray

    // number of buffers must be a power of 2 to allow quick safety checking
    //   since traceControl->index is user writable
    uval64 numberOfBuffers; // number of trace buffer
    uval tracedRunning; // boolean indicating whether there's a traced running
} TraceInfo;

typedef struct {
    uval eventMinorID;
    char *eventString, *eventParse, *eventMainPrint;
} TraceEventParse;

typedef struct {
    uval64 version;
    uval64 alignmentSize;
    uval64 ticksPerSecond;
} traceFileHeaderV1;

typedef struct {
    uval64 version;
    uval64 alignmentSize;
    uval64 ticksPerSecond;
    uval64 physProc;
    uval64 initTimestamp;
} traceFileHeaderV2;

#define TRACE_BIG_ENDIAN 0
#define TRACE_LITTLE_ENDIAN 1

typedef struct {
    char endianEncoding;   // avoid endian issues use only a byte
    char extra[3];         // align to 32 bits
    uval32 version;        // which header version we are using
    uval32 headerLength;   // length of whole header
    uval32 flags;          // reserved
    uval64 alignmentSize;  // size of alignment boundaries
    uval64 ticksPerSecond;
    uval64 physProc;
    uval64 initTimestamp;
} traceFileHeaderV3;

enum {TRACE_HWP_LAYER_ID = 0, TRACE_K42_LAYER_ID = 2};

// traceStep2
// traceStep3
#include <trace/traceOSClasses.h>
#include <trace/traceHWClasses.h>
#include <trace/traceAPPClasses.h>

#define TRACE_ALL_MASK 0xffffffffffffffffULL


#ifdef TRACE_COMPILE_OUT

#define traceDefault0(majorID, minorID, TRACE_K42_LAYER_ID)
#define traceDefault1(majorID, minorID, TRACE_K42_LAYER_ID, d1)
#define traceDefault2(majorID, minorID, TRACE_K42_LAYER_ID, d1, d2)
#define traceDefault3(majorID, minorID, TRACE_K42_LAYER_ID, d1, d2, d3)
#define traceDefault3_1(majorID, minorID, TRACE_K42_LAYER_ID, d1, d2, d3, str1)
#define traceDefault4(majorID, minorID, TRACE_K42_LAYER_ID, d1, d2, d3, d4)
#define traceDefault5(majorID, minorID, TRACE_K42_LAYER_ID, d1, d2, d3, d4, d5)
#define traceDefault6(majorID, minorID, TRACE_K42_LAYER_ID,
		      d1, d2,d3, d4, d5, d6)
#define traceDefault7(majorID, minorID, TRACE_K42_LAYER_ID,
		      d1, d2, d3, d4, d5, d6, d7)
#define traceDefault8(majorID, minorID, TRACE_K42_LAYER_ID,
		      d1, d2, d3, d4, d5, d6, d7, d8)
#define traceDefault9(majorID, minorID, TRACE_K42_LAYER_ID,
		      d1, d2, d3, d4, d5, d6, d7, d8, d9)
#define traceDefaultGeneric(majorID, minorID, TRACE_K42_LAYER_ID,
			    numbWords, numbStrs, ...)
#define traceDefaultBytes(majorID, minorID, TRACE_K42_LAYER_ID, len, d)
#define traceDefaultPre2Bytes(majorID, minorID, TRACE_K42_LAYER_ID, 
			      d1, d2, len, d)
#define traceDefaultStr(majorID, minorID, TRACE_K42_LAYER_ID, str)

#else

#ifdef __cplusplus
#define TRACE_DECL extern "C"
#else
#define TRACE_DECL extern
#endif

TRACE_DECL void
traceDefault0(uval8 majorID, uval16 minorID, uval layerID);

TRACE_DECL void
traceDefault1(uval8 majorID, uval16 minorID, uval layerID, uval64 data1);

TRACE_DECL void
traceDefault2(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2);

TRACE_DECL void
traceDefault3(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3);

TRACE_DECL void
traceDefault3_1(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
		uval64 data2, uval64 data3, const char* str1);		

TRACE_DECL void
traceDefault4(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4);

TRACE_DECL void
traceDefault5(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5);	      

TRACE_DECL void
traceDefault6(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5,
	      uval64 data6);

TRACE_DECL void
traceDefault7(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5,
	      uval64 data6, uval64 data7);

TRACE_DECL void
traceDefault8(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5,
	      uval64 data6, uval64 data7, uval64 data8);	      

TRACE_DECL void
traceDefault9(uval8 majorID, uval16 minorID, uval layerID, uval64 data1,
	      uval64 data2, uval64 data3, uval64 data4, uval64 data5,
	      uval64 data6, uval64 data7, uval64 data8, uval64 data9);	      

TRACE_DECL void
traceDefaultGeneric(uval majorID, uval minorID, uval layerID, uval numbWords,
		    uval numbStrs, ...);

TRACE_DECL void
traceDefaultBytes(uval8 majorID, uval16 minorID, uval layerID, uval len, 
		  void *data);

TRACE_DECL void
traceDefaultPre2Bytes(uval8 majorID, uval16 minorID, uval layerID, 
		      uval64 data1, uval64 data2, uval len, void *data);

TRACE_DECL void
traceDefaultStr(uval8 majorID, uval16 minorID, uval layerID, const char *str);

#undef TRACE_DECL


#endif /* COMPILE_OUT */

#endif /* #ifndef __TRACE_CORE_H_ */
