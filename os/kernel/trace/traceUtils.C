/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceUtils.C,v 1.19 2005/08/23 18:36:53 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: general functions useful for tracing both in K42 and
 *                     in tools that will try to understand the trace output
 * **************************************************************************/

#ifdef KERNEL
# include "kernIncs.H"
# include <misc/baseStdio.H>
# define atoi		baseAtoi
# define sprintf	baseSprintf
# define snprintf(STR, LEN, FMT, ARGS...)	sprintf(STR, FMT, ARGS);
#else
# include <stdlib.h>
# include <stdio.h>
# include <stdarg.h>
# include <string.h>
# include <errno.h>
# include "sys/hostSysTypes.H"
# include "sys/SysStatus.H"
#endif

// traceStep5a
#define TRACE_DEFAULT_PARSE_DEFINE_IN_C
#define TRACE_MEM_PARSE_DEFINE_IN_C
#define TRACE_TEST_PARSE_DEFINE_IN_C
#define TRACE_USER_PARSE_DEFINE_IN_C
#define TRACE_IO_PARSE_DEFINE_IN_C
#define TRACE_ALLOC_PARSE_DEFINE_IN_C
#define TRACE_PROC_PARSE_DEFINE_IN_C
#define TRACE_LOCK_PARSE_DEFINE_IN_C
#define TRACE_MISC_PARSE_DEFINE_IN_C
#define TRACE_MISCKERN_PARSE_DEFINE_IN_C
#define TRACE_CLUSTOBJ_PARSE_DEFINE_IN_C
#define TRACE_EXCEPTION_PARSE_DEFINE_IN_C
#define TRACE_SCHEDULER_PARSE_DEFINE_IN_C
#define TRACE_DBG_PARSE_DEFINE_IN_C
#define TRACE_FS_PARSE_DEFINE_IN_C
#define TRACE_LINUX_PARSE_DEFINE_IN_C
#define TRACE_CONTROL_PARSE_DEFINE_IN_C
#define TRACE_HWPERFMON_PARSE_DEFINE_IN_C
#define TRACE_SWAP_PARSE_DEFINE_IN_C
#define TRACE_RESMGR_PARSE_DEFINE_IN_C
#define TRACE_APP_PARSE_DEFINE_IN_C
#define TRACE_OMP_PARSE_DEFINE_IN_C
#define TRACE_INFO_PARSE_DEFINE_IN_C
#define TRACE_LIBC_PARSE_DEFINE_IN_C
#define TRACE_MPI_PARSE_DEFINE_IN_C
#define TRACE_DISK_PARSE_DEFINE_IN_C

#include <trace/tracePack.H>
#include "trace/traceUtils.H"
#include "trace/trace.H"
#include "trace/traceIncs.H"
#include "trace/traceUnified.H"

// traceStep5b
TraceUnified traceUnified[] = {
    {"Default", "DEFAULT", TRACE_DEFAULT_MAJOR_ID, TRACE_OS_DEFAULT_MAX,
     TRACE_DEFAULT_MASK, traceDefaultEventParse},
    {"Test", "TEST", TRACE_TEST_MAJOR_ID, TRACE_OS_TEST_MAX,
     TRACE_TEST_MASK, traceTestEventParse},
    {"Mem", "MEM", TRACE_MEM_MAJOR_ID, TRACE_OS_MEM_MAX,
     TRACE_MEM_MASK, traceMemEventParse},
    {"Lock", "LOCK", TRACE_LOCK_MAJOR_ID, TRACE_OS_LOCK_MAX,
     TRACE_LOCK_MASK, traceLockEventParse},
    {"User", "USER", TRACE_USER_MAJOR_ID, TRACE_OS_USER_MAX,
     TRACE_USER_MASK, traceUserEventParse},
    {"IO", "IO", TRACE_IO_MAJOR_ID, TRACE_OS_IO_MAX,
     TRACE_IO_MASK, traceIOEventParse},
    {"Alloc", "ALLOC", TRACE_ALLOC_MAJOR_ID, TRACE_OS_ALLOC_MAX,
     TRACE_ALLOC_MASK, traceAllocEventParse},
    {"Misc", "MISC", TRACE_MISC_MAJOR_ID, TRACE_OS_MISC_MAX,
     TRACE_MISC_MASK, traceMiscEventParse},
    {"Proc", "PROC", TRACE_PROC_MAJOR_ID, TRACE_OS_PROC_MAX,
     TRACE_PROC_MASK, traceProcEventParse},
    {"MiscKern", "MISCKERN", TRACE_MISCKERN_MAJOR_ID, TRACE_OS_MISCKERN_MAX,
     TRACE_MISCKERN_MASK, traceMiscKernEventParse},
    {"ClustObj", "CLUSTOBJ", TRACE_CLUSTOBJ_MAJOR_ID, TRACE_OS_CLUSTOBJ_MAX,
     TRACE_CLUSTOBJ_MASK, traceClustObjEventParse},
    {"Exception", "EXCEPTION", TRACE_EXCEPTION_MAJOR_ID, TRACE_OS_EXCEPTION_MAX,
     TRACE_EXCEPTION_MASK, traceExceptionEventParse},
    {"Scheduler", "SCHEDULER", TRACE_SCHEDULER_MAJOR_ID, TRACE_OS_SCHEDULER_MAX,
     TRACE_SCHEDULER_MASK, traceSchedulerEventParse},
    {"Dbg", "DBG", TRACE_DBG_MAJOR_ID, TRACE_OS_DBG_MAX,
     TRACE_DBG_MASK, traceDbgEventParse},
    {"FS", "FS", TRACE_FS_MAJOR_ID, TRACE_OS_FS_MAX,
     TRACE_FS_MASK, traceFSEventParse},
    {"Linux", "LINUX", TRACE_LINUX_MAJOR_ID, TRACE_OS_LINUX_MAX,
     TRACE_LINUX_MASK, traceLinuxEventParse},
    {"Control", "CONTROL", TRACE_CONTROL_MAJOR_ID, TRACE_OS_CONTROL_MAX,
     TRACE_CONTROL_MASK, traceControlEventParse},
    {"HWPerfMon", "HWPERF", TRACE_HWPERFMON_MAJOR_ID, TRACE_HW_HWPERFMON_MAX,
     TRACE_HWPERFMON_MASK, traceHWPerfMonEventParse},
    {"Swap", "SWAP", TRACE_SWAP_MAJOR_ID, TRACE_OS_SWAP_MAX,
     TRACE_SWAP_MASK, traceSwapEventParse},
    {"ResMgr", "RESMGR", TRACE_RESMGR_MAJOR_ID, TRACE_OS_RESMGR_MAX,
     TRACE_RESMGR_MASK, traceResMgrEventParse},
    {"App", "APP", TRACE_APP_MAJOR_ID, TRACE_APP_APP_MAX,
     TRACE_APP_MASK, traceAppEventParse},
    {"OMP", "OMP", TRACE_OMP_MAJOR_ID, TRACE_APP_OMP_MAX,
     TRACE_OMP_MASK, traceOMPEventParse},
    {"Info", "INFO", TRACE_INFO_MAJOR_ID, TRACE_OS_INFO_MAX,
     TRACE_INFO_MASK, traceInfoEventParse},
    {"LibC", "LIBC", TRACE_LIBC_MAJOR_ID, TRACE_LIB_LIBC_MAX,
     TRACE_LIBC_MASK, traceLibCEventParse},
    {"MPI", "MPI", TRACE_MPI_MAJOR_ID, TRACE_LIB_MPI_MAX,
     TRACE_MPI_MASK, traceMPIEventParse},
    {"Disk", "DISK", TRACE_DISK_MAJOR_ID, TRACE_OS_DISK_MAX,
     TRACE_DISK_MASK, traceDiskEventParse}
};

uval64 traceMajorIDPrintMask = TRACE_ALL_MASK;
sval traceMinorIDPrint = -1;
uval64 lastTime = NEG_ONE_64;
sval64 wrapAdjust=0;

#define PRECISION 1000000000ULL // nine places after the decimal

void
setLastTimeAndWrapAdjust(uval64 last, sval64 wrap)
{
    lastTime = last;
    wrapAdjust = wrap;
}

static void
printEventHeader(uval timestamp, uval lTicksPerSec, const char *str,
    	    	 printfn_t printFn)
{
    uval64 intPart, fractPart, adjTimestamp;

    if (timestamp < lastTime) {
	// a wrap occurred
	if (lastTime == NEG_ONE_64) {
	    // actually this was the first one
	    wrapAdjust  = (sval64)((-1)*(sval64)timestamp);
	} else {
	    wrapAdjust += ((uval64)1)<<(TRACE_TIMESTAMP_BITS);
	}
    }
    lastTime = timestamp;
    adjTimestamp = timestamp+wrapAdjust;

    intPart = adjTimestamp/lTicksPerSec;
    fractPart = (PRECISION*((uval64)adjTimestamp%(uval64)lTicksPerSec))/
	(uval64)lTicksPerSec;
    printFn("%4lld.%09lld %-35.35s", intPart, fractPart, str);
}

static sval
unpackTraceEventToString(const uval64 *event,
			 const char *eventParse, const char *eventMainPrint,
			 char *myString, size_t maxlen,
			 uval printDesc = 1, uval byteSwap = 0)
{
    enum {TRACE_TYPE_STR, TRACE_TYPE_64, TRACE_TYPE_32};
    uval locStrPtr=0;
    uval numbOfArgs=0;
    uval64 args[15];  // leave ourselves breathing room
    char format[20];
    uval type;
    uval len, i, bits;
    char con[4];
    uval bitNumb;
    uval lindex;
    uval myIndex, strIndex, argIndex;
    uval ordIndex, formIndex;
    uval ret;

    lindex = 1;  // skip first trace word it's not data
    len = strlen(eventParse);
    if (len == 0) {
	// there's no argument but there still could be a string
	strcpy(myString, eventMainPrint);
	return (0);
    }
    bitNumb = 0;
    while (1) {
	i=0;
	while ((eventParse[locStrPtr] != ' ') && (locStrPtr < len)) {
	    con[i] = eventParse[locStrPtr];
	    locStrPtr++;
	    i++;
	}
	locStrPtr++;
	con[i] = '\0';

	// now we have one of three options number, str, or (number
	if (con[0] == '(') {
	    strcpy(myString, "unpackAndPrintTraceEvent  Sorry NYI");
	    return _SERROR(1702, 0, EINTR);
	}
	else if (con[0] == 's') {
	    if (bitNumb != 0) {
		// we were in the middle of a word need to re-align
		// all strings start on 64 bit boundaries
		//FIXME why am I bumping numbOfArgs
		numbOfArgs++;
		lindex++;
		bitNumb = 0;
	    }
	    // FIXME - we used to & with TRACE_1H_32_MASK - WHY????
	    args[numbOfArgs] = (uval64)((uval) (&(event[lindex])));

	    // don't forget there's a \0 at end of string
	    lindex = lindex + (strlen((char *)(((uval)args[numbOfArgs])))/8)+1;
	    numbOfArgs++;
	}
	else {
	    SysStatusUval ssu = atoi(con);
	    if (_SUCCESS(ssu)) {
		bits = _SGETUVAL(ssu);
	    } else {
		bits = 0;
	    }

	    switch (bits) {
	    case 8:
		strcpy(myString, "unpackAndPrintTraceEvent  Sorry NYI");
		return _SERROR(1703, 0, EINTR);
		break;
	    case 16:
		if ((bitNumb & 0xf) || (bitNumb > 63)) {
		    // we were in the middle of a word need to re-align
		    numbOfArgs++;
		    lindex++;
		    bitNumb = 0;
		}
		if (byteSwap) {
		    args[numbOfArgs] = TRACE_SWAP16((event[lindex] >> bitNumb)
						    & TRACE_1Q_16_MASK);
		} else {
		    args[numbOfArgs] = (event[lindex] >> (64 - (bitNumb + 16)))
				       & TRACE_1Q_16_MASK;
		}
		numbOfArgs++;
		bitNumb += 16;
		if (bitNumb>63) {
		    bitNumb-=64;
		    ++lindex;
		}
		break;
	    case 32:
		if ((bitNumb != 0) && (bitNumb != 32)) {
		    // we were in the middle of a word need to re-align
		    numbOfArgs++;
		    lindex++;
		    bitNumb = 0;
		}
		if (bitNumb == 0) {
		    if (byteSwap) {
			args[numbOfArgs]
			    = TRACE_SWAP32(event[lindex] & TRACE_1H_32_MASK);
		    } else {
			args[numbOfArgs]
			    = (event[lindex] & TRACE_2H_32_MASK) >> 32;
		    }
		    numbOfArgs++;
		    bitNumb = 32;
		}
		else {
		    if (byteSwap) {
		    	args[numbOfArgs]
			    = TRACE_SWAP32(event[lindex]&TRACE_2H_32_MASK)>>32;
		    } else {
			args[numbOfArgs] = event[lindex] & TRACE_1H_32_MASK;
		    }
		    numbOfArgs++;
		    bitNumb = 0;
		    lindex++;
		}
		break;
	    case 64:
		if (bitNumb != 0) {
		    // we were in the middle of a word need to re-align
		    numbOfArgs++;
		    lindex++;
		    bitNumb = 0;
		}
		args[numbOfArgs] = TRACE_SWAP64(event[lindex]);
		numbOfArgs++;
		lindex++;
		break;
	    default:
		strcpy(myString, "Sorry unknown bit quantity");
		return _SERROR(1705, 0, EINTR);
		break;
	    }
	}
	if (numbOfArgs > 10) {
	    strcpy(myString, "unpackAndPrintTraceEvent: too many args NYI");
	    return _SERROR(1706, 0, EINTR);
	}
	if (locStrPtr >= len) break;
    }

    // okay now go through and parse main print string
    myIndex = argIndex = strIndex = 0;

    len = strlen(eventMainPrint);

    while (strIndex < strlen(eventMainPrint)) {
	if (eventMainPrint[strIndex] == '%') {
	    strIndex++;

	    //FIXME at some point we'll need to get more than 9 args
	    ordIndex = (eventMainPrint[strIndex] - '0');

	    // now get the format string
	    strIndex++;
	    if (eventMainPrint[strIndex] != '[') {
		strcpy(myString, "error: incorrect format");
		return _SERROR(2556, 0, EINTR);
	    }
	    formIndex = 0;
	    strIndex++;
	    while (eventMainPrint[strIndex] != ']') {
		format[formIndex] = eventMainPrint[strIndex];
		strIndex++;
		formIndex++;
	    }
	    format[formIndex] = ' ';
	    format[formIndex+1] = '\0';

	    if (format[formIndex-1] == 's') {
		type = TRACE_TYPE_STR;
	    }
	    else if ((format[formIndex-2] == 'l') &&
		     (format[formIndex-3] == 'l') &&
		     ((format[formIndex-1] == 'x') ||
		      (format[formIndex-1] == 'd'))) {
		type = TRACE_TYPE_64;
	    }
	    else {
		type = TRACE_TYPE_32;
	    }

	    // not output the string to myString with the correct format
	    switch (type) {
	    case TRACE_TYPE_32:
		ret = snprintf(myString+myIndex, maxlen-myIndex , format,
			       (uval32)args[ordIndex]);
		break;
	    case TRACE_TYPE_64:
		ret = snprintf(myString+myIndex, maxlen-myIndex , format,
			       args[ordIndex]);
		break;
	    case TRACE_TYPE_STR:
		ret = snprintf(myString+myIndex, maxlen-myIndex , format,
			       (char *)((uval)args[ordIndex]));
		break;
	    default:
		strcpy(myString, "internal error: unknown type - help me OB1");
		return _SERROR(2557, 0, EINTR);
		break;
	    }
	    if (ret<0) {
		sprintf(myString, "printf failure: %s\n", format);
		return _SERROR(2605, 0, EINTR);
	    }
	    myIndex+=ret;
	    argIndex++;
	}
	else {
	    if (printDesc) {
		myString[myIndex] = eventMainPrint[strIndex];
		myIndex++;
	    }
	    myString[myIndex] = '\0';
	}
	strIndex++;
    }

    myString[myIndex] = '\0';
    return (0);
}

static void
unpackAndPrintTraceEvent(const uval64 *event, const char *eventParse,
			 const char *eventMainPrint, uval printDesc,
			 uval byteSwap, printfn_t printFn)
{
    char myString[512];

    // Check for sanity in the tables
    // FIXME: need to find a better place for this, and also bomb out on error
    if (TRACE_LAST_MAJOR_ID_CHECK !=
	sizeof(traceUnified) / sizeof(TraceUnified)) {
	DEFAULT_PRINTFN("Internal error: TRACE_LAST_MAJOR_ID_CHECK is incorrect"
			" (is %u, should be %lu)\n", TRACE_LAST_MAJOR_ID_CHECK,
			(uval) (sizeof(traceUnified) / sizeof(TraceUnified)));
    }

    unpackTraceEventToString(event, eventParse, eventMainPrint, myString, 512,
			     printDesc, byteSwap);

    printFn(" %s\n", myString);
}

void
tracePrintEventGeneric(const uval64 *event, uval64 ticksPerSecond,
    	    	       uval printDesc, uval byteSwap, printfn_t printFn)
{
    uval timestamp, majorID, minorID;
    uval64 data = TRACE_SWAP64(event[0]);

    timestamp = TRACE_TIMESTAMP_GET(data);
    majorID = TRACE_MAJOR_ID_GET(data);
    minorID = TRACE_DATA_GET(data);

    printEventHeader(timestamp, ticksPerSecond,
		 traceUnified[majorID].traceEventParse[minorID].eventString,
		 printFn);

    unpackAndPrintTraceEvent(
	event,
	traceUnified[majorID].traceEventParse[minorID].eventParse,
	traceUnified[majorID].traceEventParse[minorID].eventMainPrint,
	printDesc, byteSwap, printFn);
}


void
tracePrintEventTest(const uval64 *event, uval64 ticksPerSecond,
    	    	    uval printDesc, uval byteSwap, printfn_t printFn)
{
    uval timestamp, minorID;
    uval64 data = TRACE_SWAP64(event[0]);

    timestamp = TRACE_TIMESTAMP_GET(data);
    minorID = TRACE_DATA_GET(data);
    if (traceMinorIDPrint != -1) {
	if ((uval)traceMinorIDPrint != minorID) {
	    return;
	}
    }
    printEventHeader(timestamp, ticksPerSecond,
		     traceTestEventParse[minorID].eventString, printFn);

    switch (minorID) {
	// we only need case statements for odd or special events
	// most are handled with formatting from .H files
    default:
	unpackAndPrintTraceEvent(event,
				 traceTestEventParse[minorID].eventParse,
				 traceTestEventParse[minorID].eventMainPrint,
				 printDesc, byteSwap, printFn);
	break;
    }
}

void
tracePrintEventException(const uval64 *event, uval64 ticksPerSecond,
			 uval printDesc, uval byteSwap, printfn_t printFn)
{
    uval timestamp, minorID;
    uval64 data = TRACE_SWAP64(event[0]);

    timestamp = TRACE_TIMESTAMP_GET(data);
    minorID = TRACE_DATA_GET(data);
    if (traceMinorIDPrint != -1) {
	if ((uval)traceMinorIDPrint != minorID) {
	    return;
	}
    }
    printEventHeader(timestamp, ticksPerSecond,
		     traceExceptionEventParse[minorID].eventString, printFn);

    switch (minorID) {
	// we only need case statements for odd or special events
	// most are handled with formatting from .H files
    default:
	unpackAndPrintTraceEvent(event,
			  traceExceptionEventParse[minorID].eventParse,
			  traceExceptionEventParse[minorID].eventMainPrint,
			  printDesc, byteSwap, printFn);
	break;
    }
}

void
tracePrintEventHWPerfMon(const uval64 *event, uval64 ticksPerSecond,
			 uval printDesc, uval byteSwap, printfn_t printFn)
{
    char myString[512];
    char str[256];
    uval numCounters;
    uval timestamp, minorID;
    uval64 data = TRACE_SWAP64(event[0]);

    timestamp = TRACE_TIMESTAMP_GET(data);
    minorID = TRACE_DATA_GET(data);
    if (traceMinorIDPrint != -1) {
	if ((uval)traceMinorIDPrint != minorID) {
	    return;
	}
    }
    printEventHeader(timestamp, ticksPerSecond,
		     traceHWPerfMonEventParse[minorID].eventString, printFn);

    switch (minorID) {
	// we only need case statements for odd or special events
	// most are handled with formatting from .H files
    case TRACE_HWPERFMON_PERIODIC_ALL:
    case TRACE_HWPERFMON_APERIODIC_ALL:
        data = TRACE_SWAP64(event[1]);
        sprintf(str," PC %llx",data);
        strcpy(myString,str);
        data = TRACE_SWAP64(event[2]);
        sprintf(str,"  NEVENTS %lld ",data);
        strcat(myString,str);

	numCounters = data;

        strcat(myString," HWCTRS ");
	for (uval i = 0; i < numCounters; i++) {
            data = TRACE_SWAP64(event[i+3]);
            sprintf(str,"  %lld",data);
            strcat(myString,str);
	}
        printFn(" %s\n", myString);
	break;

    default:
	unpackAndPrintTraceEvent(event,
			  traceHWPerfMonEventParse[minorID].eventParse,
			  traceHWPerfMonEventParse[minorID].eventMainPrint,
			  printDesc, byteSwap, printFn);
	break;
    }
}

void
tracePrintEventDefault(const uval64 *event, uval64 ticksPerSecond,
    	    	       uval printDesc, uval byteSwap, printfn_t printFn)
{
    uval timestamp, minorID;
    uval64 data = TRACE_SWAP64(event[0]);

    timestamp = TRACE_TIMESTAMP_GET(data);
    minorID = TRACE_DATA_GET(data);
    if (traceMinorIDPrint != -1) {
	if ((uval)traceMinorIDPrint != minorID) {
	    return;
	}
    }
    printEventHeader(timestamp, ticksPerSecond,
		     traceDefaultEventParse[minorID].eventString, printFn);

    switch (minorID) {
	// we only need case statements for odd or special events
	// most are handled with formatting from .H files
    default:
	unpackAndPrintTraceEvent(
	    event,
	    traceDefaultEventParse[minorID].eventParse,
	    traceDefaultEventParse[minorID].eventMainPrint,
	    printDesc, byteSwap, printFn);
	break;
    }
}

void
tracePrintEvent(const uval64 *event, uval64 ticksPerSecond,
    	        uval printDesc, uval byteSwap, printfn_t printFn)
{
    uval majorID;

    majorID = TRACE_MAJOR_ID_GET(TRACE_SWAP64(event[0]));
    if (!(traceMajorIDPrintMask & (1<<majorID))) {
	return;
    }
    switch (majorID) {
    case TRACE_DEFAULT_MAJOR_ID:
	tracePrintEventDefault(event, ticksPerSecond, printDesc, byteSwap,
	    	    	       printFn);
	break;
    case TRACE_TEST_MAJOR_ID:
	tracePrintEventTest(event, ticksPerSecond, printDesc, byteSwap,
	    	    	    printFn);
	break;
    case TRACE_EXCEPTION_MAJOR_ID:
	tracePrintEventException(event, ticksPerSecond, printDesc, byteSwap,
	    	    	    	 printFn);
	break;

    case TRACE_HWPERFMON_MAJOR_ID:
	tracePrintEventHWPerfMon(event, ticksPerSecond, printDesc, byteSwap,
	    	    	    	 printFn);
	break;

    default:
	// we just haven't defined a specific function for this majorID
	if (majorID < TRACE_LAST_MAJOR_ID_CHECK) {
	    tracePrintEventGeneric(event, ticksPerSecond, printDesc, byteSwap,
	    	    	    	   printFn);
	} else {
	    DEFAULT_PRINTFN("trace error: undefined major ID %ld\n", majorID);
	}
	break;
    }
}
