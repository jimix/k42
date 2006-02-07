/*
 * (C) Copyright IBM Corp. 2004
 */
/* $Id: testCppLib.cc,v 1.6 2005/07/01 19:58:55 cascaval Exp $ */
 
/**
 *  C++ interface for the standalone PEM API tracing
 *  test program
 *
 * @Author: CC
 * @Date: 12/23/04
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "PemEvents.H"

using namespace std;
using namespace PEM;

int main(int argc, char **argv)
{
  TraceOutputStream strm;
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

  if(argc < 2) {
    fprintf(stderr, "Usage %s <trace filename>\n", argv[0]);
    exit(1);
  }

  strm.open(argv[1], 128*1024);
  if (strm.fail()) {
    fprintf(stderr,"tracing_initialize() failed to open %s !\n", argv[1]);
    exit(1);
  }

  /* write a header */
  TraceHeader header(PEM_TRACE_VERSION, (BYTE_ORDER == LITTLE_ENDIAN),
		     0, 128*1024);
  header.setInitTimestamp(0);
  header.write(strm);

  MONLayer::Test::Test0_Event e0(1); e0.write(strm);
  cout << "Test0 ok" << endl;
  MONLayer::Test::Test1_Event e1(2, ll1); e1.write(strm);
  cout << "Test1 ok" << endl;
  MONLayer::Test::Test2_Event e2(3, ll1, ll2, ll1); e2.write(strm);
  cout << "Test2 ok" << endl;
  MONLayer::Test::Pack_Event e3(4, c1, c2, s, w, ll2); e3.write(strm);
  cout << "Pack ok" << endl;
  MONLayer::Test::String_Event e4(5, "bubu"); e4.write(strm);
  cout << "String ok" << endl;
  MONLayer::Test::StrData_Event e5(6, ll1, "bubu1", "bubu2"); e5.write(strm);
  cout << "StrData ok" << endl;

  MONLayer::Test::List_Event e6(7, w, 8, (uint64 *)list); e6.write(strm);
  cout << "List ok" << endl;

  MONLayer::Test::ListOfStrings_Event e7(8, 5, strings); e7.write(strm);
  cout << "ListOfStrings ok" << endl;
  MONLayer::Test::MixedSequence_Event e9(4, c1, ll2, s, c2, w); e9.write(strm);
  cout << "MixedSequence ok" << endl;


  int listSize = 16;
  uint8 listOfBytes[16];
  uint16 listOfShorts[16];
  uint32 listOfInts[16];
  uint64 listOfLongs[16];
  for(int i = 0; i < listSize; i++) {
    listOfBytes[i] = 'A' + i;
    listOfShorts[i] = i+1;
    listOfInts[i] = i+1;
    listOfLongs[i] = i+1;
  }
  MONLayer::Test::ListOfSmallSizes_Event e10(10, listSize, 
					     listOfBytes, 
					     listOfShorts,
					     listOfInts,
					     listOfLongs);
  e10.write(strm);
  cout << "ListOfSmallSizes ok" << endl;

  strm.close();

  return 0;
}
