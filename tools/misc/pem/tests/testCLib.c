/*
 * (C) Copyright IBM Corp. 2004
 */
/* $Id: testCLib.c,v 1.4 2005/07/01 20:08:19 cascaval Exp $ */
 
/**
 *  C interface for the standalone PEM API tracing
 *  test program
 *
 * @Author: CC
 * @Date: 12/04/04
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "traceRecord.h"
#include "notify.h"
#include "traceTest.h"

int main(int argc, char **argv)
{
  TraceOutputStream *strm;

  uint64 ll1 = 0x123456789ULL;
  uint64 ll2 = 0x2468ace0fULL;
  unsigned char c1 = 255;
  unsigned char c2 = 1;
  unsigned short s = 0x3579;
  unsigned int w = 0x56655665;
  uint64 list[] = { 0x111222233ULL, 0x2233344444ULL, 
		    0x344555666ULL, 0x4455566667ULL,
		    0x555666677ULL, 0x6667777788ULL, 
		    0x778888999ULL, 0x8888999999ULL 
  };
  char *strings[] = { "five", "little", "strings", "to", "test" };
  /* list sizes tests */
  int listSize = 16; int i;
  uint8 listOfBytes[16];
  uint16 listOfShorts[16];
  uint32 listOfInts[16];
  uint64 listOfLongs[16];

  if(argc < 2) {
    fprintf(stderr, "Usage %s <trace filename>\n", argv[0]);
    exit(1);
  }

  strm = openStream(argv[1], O_CREAT|O_TRUNC|O_RDWR,102400);
  if(!strm) {
    fprintf(stderr, "Failed to open trace stream %s (%s)\n", 
	    argv[1], strerror(errno));
    exit(1);
  }

  setTraceStream(strm);

  TraceMONTestTest0();
  TraceMONTestTest1(ll1);
  TraceMONTestTest2(ll1, ll2, ll2);
  TraceMONTestPack(c1, c2, s, w, ll2);
  TraceMONTestString("bubu");
  TraceMONTestStrData(ll1, "bubu1", "bubu2"); 

  TraceMONTestList(w, 8, list);

  TraceMONTestListOfStrings(5, strings);

  for(i = 0; i < listSize; i++) {
    listOfBytes[i] = 'A' + i;
    listOfShorts[i] = i+1;
    listOfInts[i] = i+1;
    listOfLongs[i] = i+1;
  }
  TraceMONTestListOfSmallSizes(listSize, 
			       listOfBytes, listOfShorts,
			       listOfInts,  listOfLongs);

  closeStream(strm);

  return 0;
}
