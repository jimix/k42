/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceParse.C,v 1.1 2005/01/27 20:48:14 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#ifdef K42
# include <sys/sysIncs.H>
# include <sys/ProcessLinuxClient.H>
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

// this is a hack it's taken from trace.H

// must be a power of 2 and must be greater that 1024 since the length of
// an entry is 2^10 (TARCE_TIMESTAMP_SHIFT - TRACE_LENGTH_SHIFT) * 8 (number
// of bytes per entry) / 8 (bytes per entry), but for bin packing should
// be greater than 4096

#if 0

#define TRACE_LOG_NUMBER_ENTRIES_BUFFER 6 // must be >= 10
#define TRACE_NUMBER_ENTRIES_BUFFER (1<<TRACE_LOG_NUMBER_ENTRIES_BUFFER)
#define TRACE_BITS_FOR_ENTRIES 24
#define TRACE_SIZE_FOR_ENTRIES (1<<TRACE_BITS_FOR_ENTRIES)

#define TRACE_INDEX_GET(x) ((x)&(TRACE_SIZE_FOR_ENTRIES-1))
#define TRACE_BUFFER_GET(x) ((x)>>TRACE_BITS_FOR_ENTRIES)
#define TRACE_BUFFER_AND_INDEX_SET(b,i) (((b)<<TRACE_BITS_FOR_ENTRIES)|(i))

// defines used to create and manage first word in event
#define TRACE_TIMESTAMP_SHIFT (32)
#define TRACE_TIMESTAMP_VEC (0xffffffff)
#define TRACE_TIMESTAMP_MASK (0xffffffff00000000ULL)
#define TRACE_TIMESTAMP_GET(x)(((x)&TRACE_TIMESTAMP_MASK)>>TRACE_TIMESTAMP_SHIFT)

#define TRACE_LENGTH_SHIFT (24)
#define TRACE_LENGTH_VEC (0xff)
#define TRACE_LENGTH_MASK (0x00000000ff000000ULL)
#define TRACE_LENGTH_GET(x) (((x)&TRACE_LENGTH_MASK)>>TRACE_LENGTH_SHIFT)

#define TRACE_LAYER_ID_SHIFT (20)
#define TRACE_LAYER_ID_VEC (0xf)
#define TRACE_LAYER_ID_MASK (0x0000000000f00000ULL)
#define TRACE_LAYER_ID_GET(x) (((x)&TRACE_MAJOR_ID_MASK)>>TRACE_MAJOR_ID_SHIFT)

#define TRACE_MAJOR_ID_SHIFT (14)
#define TRACE_MAJOR_ID_VEC (0xfc)
#define TRACE_MAJOR_ID_MASK (0x00000000000fc000ULL)
#define TRACE_MAJOR_ID_GET(x) (((x)&TRACE_MAJOR_ID_MASK)>>TRACE_MAJOR_ID_SHIFT)

#define TRACE_DATA_SHIFT (0)
#define TRACE_DATA_VEC (0x3fff)
#define TRACE_DATA_MASK (0x0000000000003fffULL)
#define TRACE_DATA_GET(x) (((x)&TRACE_DATA_MASK)>>TRACE_DATA_SHIFT)

#endif

int
print_usage()
{
  printf("traceParse\n");
  printf("  breaks the hex number up as if it was the first work in a trace event\n");
  exit(0);
}

int
main(int argc, char **argv)
{
  unsigned long long val;
  char valStr[64];

#if 0
  if (argc != 2) {
    print_usage();
  }

  if (strcmp(argv[1], "--help") == 0) {
    print_usage();
  }
  printf("argv[1] %s\n", argv[1]);

  sscanf(argv[1], "%llx", &val);
#endif

  while (1) {
      printf("enter first trace word: ");
      fgets(valStr, 64, stdin);
      sscanf(valStr, "%llx", &val);
      printf(" timestamp is: %llx\n", TRACE_TIMESTAMP_GET(val));
      printf(" length is:    %llx\n", TRACE_LENGTH_GET(val));
      printf(" major id is:  %llx\n", TRACE_MAJOR_ID_GET(val));
      printf(" data is:      %llx\n", TRACE_DATA_GET(val));
      printf(" event is:     %s\n",
	     traceUnified[TRACE_MAJOR_ID_GET(val)].traceEventParse[TRACE_DATA_GET(val)].eventString);
  }

  return 0;
}
