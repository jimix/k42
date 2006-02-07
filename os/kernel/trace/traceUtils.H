#ifndef __TRACE_UTILS_H_
#define __TRACE_UTILS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceUtils.H,v 1.37 2004/10/06 14:45:24 azimi Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: general functions useful for tracing both in K42 and
 *                     in tools that will try to understand the trace output
 * **************************************************************************/

extern uval64 traceMajorIDPrintMask;
extern sval traceMinorIDPrint;
extern uval64 lastTime;
extern sval64 wrapAdjust;

// typedef for a pointer to a printf-style function
typedef int (*printfn_t)(const char *, ...);

#ifdef KERNEL
// seedy cast, but safe since we don't care about the return value
# define DEFAULT_PRINTFN ((printfn_t)err_printf)
#else
# define DEFAULT_PRINTFN printf
#endif

void setLastTimeAndWrapAdjust(uval64 last, sval64 wrap);
void tracePrintEventGeneric(const uval64 *event, uval64 ticksPerSecond,
    	    	    	    uval printDesc=1, uval byteSwap=0,
			    printfn_t printFn=DEFAULT_PRINTFN);
void tracePrintEventTest(const uval64 *event, uval64 ticksPerSecond,
    	    	    	 uval printDesc=1, uval byteSwap=0,
			 printfn_t printFn=DEFAULT_PRINTFN);
void tracePrintEventException(const uval64 *event, uval64 ticksPerSecond,
    	    	    	      uval printDesc=1, uval byteSwap=0,
			      printfn_t printFn=DEFAULT_PRINTFN);
void tracePrintEventHWPerfMon(const uval64 *event, uval64 ticksPerSecond,
    	    	    	      uval printDesc=1, uval byteSwap=0,
			      printfn_t printFn=DEFAULT_PRINTFN);
void tracePrintEventDefault(const uval64 *event, uval64 ticksPerSecond,
    	    	    	    uval printDesc=1, uval byteSwap=0,
			    printfn_t printFn=DEFAULT_PRINTFN);
void tracePrintEvent(const uval64 *event, uval64 ticksPerSecond,
    	    	     uval printDesc=1, uval byteSwap=0,
		     printfn_t printFn=DEFAULT_PRINTFN);

#define NEG_ONE_64 0xffffffffffffffffULL

#ifdef KERNEL
# define TRACE_SWAP16(x) (x)
# define TRACE_SWAP32(x) (x)
# define TRACE_SWAP64(x) (x)
#else
# include <sys/bswap.h>
# define TRACE_SWAP16(x) ( byteSwap ? bswap_16(x) : (x) )
# define TRACE_SWAP32(x) ( byteSwap ? bswap_32(x) : (x) )
# define TRACE_SWAP64(x) ( byteSwap ? bswap_64(x) : (x) )
#endif

// use the below if you know your trace event is packed only with 64 bit args
#define traceGetArg1Std(event) (TRACE_SWAP64(event[1]))
#define traceGetArg2Std(event) (TRACE_SWAP64(event[2]))
#define traceGetArg3Std(event) (TRACE_SWAP64(event[3]))
#define traceGetArg4Std(event) (TRACE_SWAP64(event[4]))
#define traceGetArg5Std(event) (TRACE_SWAP64(event[5]))
#define traceGetArg6Std(event) (TRACE_SWAP64(event[6]))

#endif /* #ifndef __TRACE_UTILS_H_ */
