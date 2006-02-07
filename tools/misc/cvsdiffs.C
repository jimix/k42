/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: cvsdiffs.C,v 1.8 2002/11/25 21:51:40 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
print_usage()
{
  printf("cvsdiffs filename\n");
  printf("  prints all the diffs from your potentially modified version back to\n");
  printf("  the original.\n");
  exit(0);
}

int
main(int argc, char **argv)
{
  char popen_str[256];
  char scan_str[256];
  char sys_str[256];
  char rev_str[256];
  FILE *fp;
  int maj_rev = -1;
  int min_rev = -1;
  int i,j;

  if (argc != 2) {
    print_usage();
  }

  if (strcmp(argv[1], "--help") == 0) {
    print_usage();
  }

  sprintf(popen_str,"cvs status %s", argv[1]);

  fp = popen(popen_str, "r");
  if(fp == NULL) {
    fprintf(stderr, "unknown cvs file %s\n", argv[1]);
    exit(1);
  }


  while (fscanf(fp,"%s", scan_str) != EOF) {
    if (strcmp(scan_str, "revision:") == 0) {
      fscanf(fp,"%s", scan_str);

      if (strcmp(scan_str, "No") == 0) {
	fprintf(stderr, "unknown cvs file: %s\n", argv[1]);
	exit(-1);
      }
      i=j=0;
      while (scan_str[i] != '.') {
	rev_str[j] = scan_str[i];
	i++;
	j++;
      }
      rev_str[j] = '\0';
      maj_rev = atoi(rev_str);
      i++;
      j=0;
      while (i<(int)strlen(scan_str)) {
	rev_str[j] = scan_str[i];
	i++;
	j++;
      }
      min_rev = atoi(rev_str);

      break;
    }

  }
  fclose(fp);

  sprintf(sys_str, "cvs diff %s", argv[1]);
  system(sys_str);

  for (i=min_rev; i>1; i--) {
    sprintf(sys_str, "cvs diff -r 1.%d -r 1.%d %s", i, i-1, argv[1]);
    system(sys_str);
  }

  return 0;
}
