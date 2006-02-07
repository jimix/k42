/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceTool.C,v 1.38 2005/06/28 19:48:34 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "sys/errno.h"
#include "sys/mman.h"
#include "sys/stat.h"
#include "sys/dir.h"
#include <fcntl.h>

#ifdef K42
# include <sys/sysIncs.H>
# include <sys/systemAccess.H>
#else
# include "sys/hostSysTypes.H"
#endif

#include "trace/traceUtils.H"
#include "trace/trace.H"
#include "trace/traceIncs.H"
#include "trace/traceUnified.H"
#include "trace/traceControl.h"
#include "trace/traceHWPerfMon.h"
#include "traceCommon.H"

static void
print_usage()
{
    printf("traceTool [--help] [--file filename] [--live] [--mp N]\n");
    printf("          [--mpSync N] [--mpBase baseFilname] [--out filename]\n");
    printf("          [--overrideGarbled] [--strip] [--v] \n");
    printf("  traceTool parses a binary trace file and outputs an ASCII\n");
    printf("   representation as defined by the traceParse entries\n");
    printf("   associated with each of the trace events\n");
    printf("  The --help option prints out this usage information.\n");
    printf("  The --file filename option specifies the binary trace file or\n");
    printf("   files to be used, the default is trace-out.0.trc\n");
    printf("  The --live option reads events directly from the running\n");
    printf("   system's trace event buffers (only valid on K42)\n");
    printf("  The --mp N option is used to ask for N output ascii\n");
    printf("   files to be generated.  By default it uses as input\n");
    printf("   trace-out.X.trc, where X is 0 through N - 1, and outputs\n");
    printf("   trace-out.X.txt, see --mpBase for modifying filenames.\n");
    printf("   This option overrides --file and --out\n");
    printf("  The --mpBase filename option is used to specify an\n");
    printf("   alternative base that generates all mp files\n");
    printf("   e.g. trace-lock specific files trace-lock.0.trc through\n");
    printf("   trace-lock.N.trc, used with --mpSync\n");
    printf("  The --mpSync N option indicates to synchronize time across\n");
    printf("   N processors, it does so by setting time zero, to be the\n");
    printf("   smallest timestamp acroos the N files specified.  See\n");
    printf("   the --mpBase option for how to specify the files\n");
    printf("   By default this sets --mp # to be equal to be the N\n");
    printf("   specified here.  If specified after this option --mp # will\n");
    printf("   take precedence.\n");
    printf("  The --out option specifies which file the ASCII output will\n");
    printf("   written to, by default it is trace-out.0.txt\n");
    printf("  The --overrideGarbled option will cause traceProfile to\n");
    printf("   attempt to continue even if it comes across a completely\n");
    printf("   garbled trace stream.  User beware.  Default is off\n");
    printf("  The --strip option specifies that descriptive strings should\n");
    printf("   be ignored, print only raw trace data in ASCII format.\n");
    printf("  The --v [0|1] verbose print out info about the trace 1 by def\n");
}

static bool byteSwap = false, printDesc = true;
static char *outFilename = NULL, *baseFilename = "trace-out";
static FILE *fpOut;

// FIXME this is really only a heuristic and even more doesn't handle the
//       situation where the timestamps are scattered across a wrap
static uval32 minTimestamp = (uval32)-1;

static int
getFirstEventTime(const traceFileHeaderV3 *headerInfo, const uval64 *event,
		  uval len, bool byteSwap, bool first, bool last, bool verbose)
{
    uval32 timestamp = TRACE_TIMESTAMP_GET(TRACE_SWAP64(*event));
    if (timestamp < minTimestamp) {
	minTimestamp = timestamp;
    }

    return 1;
}

static uval32
syncTraceStreams(uval numbProcs, uval live, const char *baseFilename)
{
    uval proc;
    char filename[PATH_MAX];
    int rc;

    if (live) {
	rc = processTraceBuffers(getFirstEventTime, 0);
	if (rc != 1) {
	    exit(-1);
	}
    } else {
	for (proc = 0; proc < numbProcs; proc++) {
	    rc = snprintf(filename, sizeof(filename), "%s.%ld.trc",
			  baseFilename, proc);
	    if (rc == -1) {
		exit(-1);
	    }

	    rc = processTraceFile(filename, getFirstEventTime, &byteSwap, 0);
	    if (rc != 1) {
		exit(-1);
	    }
	}
    }

    return minTimestamp;
}

static int
print_fn(const char *format, ...)
{
    va_list ap;
    int r;

    va_start(ap, format);
    r = vfprintf(fpOut, format, ap);
    va_end(ap);

    return r;
}

static int
printTraceEvent(const traceFileHeaderV3 *headerInfo, const uval64 *event,
		uval len, bool byteSwap, bool first, bool last, bool verbose)
{
    char namebuf[PATH_MAX];
    char *filename;
    VPNum proc = TRACE_SWAP64(headerInfo->physProc);
    int rc;

    if (first) {
	if (minTimestamp != (uval32)-1) {
	    setLastTimeAndWrapAdjust(minTimestamp,
				     ((sval64)((-1)*(sval64)minTimestamp)));
	} else {
	    setLastTimeAndWrapAdjust(NEG_ONE_64, 0);
	}

    	if (outFilename == NULL) {
	    rc = snprintf(namebuf, sizeof(namebuf), "%s.%ld.txt",
			  baseFilename, proc);
	    if (rc == -1) {
		return -1;
	    }
	    filename = namebuf;
	} else {
	    filename = outFilename;
	}

	fpOut = fopen(filename, "w");
	if (fpOut == NULL) {
	    printf("Error: failed to open %s for writing\n", filename);
	    return -1;
	}
    }

    tracePrintEvent(event, TRACE_SWAP64(headerInfo->ticksPerSecond), printDesc,
		    byteSwap, print_fn);

    if (last) {
	fclose(fpOut);
    }

    return 0;
}

int
main(int argc, char **argv)
{
#ifdef K42
    NativeProcess();
#endif

    char *inFilename = NULL;
    char *filename;
    char namebuf[PATH_MAX];
    uval proc, numbProcs = 1, numbOutFiles = 1;
    bool live = false;
    int rc, index, verbose;
    int vOn;

    verbose = true;
    overrideGarbled = 0;

    for (index = 1; index < argc; index++) {
	if (strcmp(argv[index], "--file") == 0) {
	    inFilename = argv[++index];
	} else
	if (strcmp(argv[index], "--help") == 0) {
	    print_usage();
	    return 0;
	} else
	if (strcmp(argv[index], "--live") == 0) {
#ifdef K42
	    live = true;
#else
	    printf("Error: --live not valid for this version of the tool\n");
	    return -1;
#endif
	} else
	if (strcmp(argv[index], "--mp") == 0) {
	    sscanf(argv[++index], "%ld", &numbOutFiles);
	} else
	if (strcmp(argv[index], "--mpBase") == 0) {
	    baseFilename = argv[++index];
	} else
	if (strcmp(argv[index], "--mpSync") == 0) {
	    sscanf(argv[++index], "%ld", &numbProcs);
	} else
	if (strcmp(argv[index], "--overrideGarbled") == 0) {
	    overrideGarbled = 1;
	} else
	if (strcmp(argv[index], "--out") == 0) {
	    outFilename = argv[++index];
	} else
	if (strcmp(argv[index], "--strip") == 0) {
	    printDesc = false;
	} else
	if (strcmp(argv[index], "--v") == 0) {
	    sscanf(argv[++index], "%d", &vOn);
	    if (vOn == 0) {
		verbose = false;
	    } else {
		verbose = true;
	    }
	} else {
	    printf("Error: unknown option %s\n", argv[index]);
	    print_usage();
	    return -1;
	}
    }

    if (numbProcs > numbOutFiles) {
	numbOutFiles = numbProcs;
    }

    if (numbProcs > 1) {
        if (inFilename != NULL || outFilename != NULL) {
	    printf("Warning: tracing multiple processors, --filename and --out "
		   "arguments ignored\n");
	    inFilename = outFilename = NULL;
	}
	syncTraceStreams(numbProcs, live, baseFilename);
    }

    if (live) {
	rc = processTraceBuffers(printTraceEvent, true);
    } else {
	for (proc = 0; proc < numbOutFiles; proc++) {
    	    if (inFilename == NULL) {
		rc = snprintf(namebuf, sizeof(namebuf), "%s.%ld.trc",
			      baseFilename, proc);
		if (rc == -1) {
		    return -1;
		}
		filename = namebuf;
	    } else {
	    	filename = inFilename;
	    }

	    rc = processTraceFile(filename, printTraceEvent, &byteSwap, verbose);
	    if (rc != 0 && !overrideGarbled) {
		printf("Error encountered. Exiting, see --overrideGarbled\n");
		return rc;
	    }
	}
    }

    return 0;
}
