/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: serrorParse.C,v 1.2 2004/07/13 21:15:23 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>

int
print_usage()
{
  printf("serrorParse\n");
  printf("  breaks the serror number into it's components\n");
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
      printf("enter serror value: ");
      fgets(valStr, 64, stdin);
      sscanf(valStr, "%llx", &val);
      printf(" unique identifier: %lld\n",  (val & 0x7fffffffffff0000ULL)>>16);
      printf(" class component:    %lld\n", (val & 0x000000000000ff00ULL)>>8);
      printf(" general errno:  %lld\n",      val & 0x00000000000000ffULL);
  }
}
