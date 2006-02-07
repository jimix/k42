/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: count page faults caused by mmap + store + sleep
 * **************************************************************************/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>

int main(int argc, char *argv[])
{
    int c, fd, initial, increment, polls;
    char *ptr, name[] = "mmap.XXXXXX";
    struct rusage before, after;

    if (argc != 4) {
      printf("`faults' counts page faults\n\n"
	     "Usage: faults [initial-sleep] [increment-sleep] [polls]\n"
	     " [initial-sleep]   How long to sleep between first two polls.\n"
	     " [increment-sleep] How much to increase sleep each time.\n"
	     " [polls]           How many polls to do (0 for infinity).\n\n"
	     "Example: faults 1 1 16\n\n");
      return 1;
    }

    initial = atoi(argv[1]);
    increment = atoi(argv[2]);
    polls = atoi(argv[3]);

    if (getrusage(RUSAGE_SELF, &before))
	fprintf(stderr, "getrusage: %s\n", strerror(errno)), exit(1);

    if ((fd = mkstemp(name)) < 0)
	fprintf(stderr, "mkstemp: %s\n", strerror(errno)), exit(1);

    if ((ptr =
	 mmap(0, 1024, PROT_WRITE, MAP_SHARED, fd, 0)) == (void *) -1)
	fprintf(stderr, "mmap: %s\n", strerror(errno)), exit(1);

    if (write(fd, "A", 1) != 1)
	fprintf(stderr, "write: %s\n", strerror(errno)), exit(1);

    if (getrusage(RUSAGE_SELF, &after))
	fprintf(stderr, "getrusage: %s\n", strerror(errno)), exit(1);

    printf("%-7s %-7s %-8s %-7s %-7s %-7s %-7s\n",
	   "MAJFLT", "MINFLT", "WRITE", "SLEEP", "MAJFLT", "MINFLT",
	   "NEWFLT");
    printf("%-7li %-7li %-8c %-7i %-7li %-7li %-7li\n", before.ru_majflt,
	   before.ru_minflt, 'A', 0, after.ru_majflt, after.ru_minflt,
	   (after.ru_minflt + after.ru_majflt) - (before.ru_minflt +
						  before.ru_majflt));

    for (c = 'B'; c < polls + 'A' || 0 == polls; c++, initial += increment) {
	sleep(initial);

	if (getrusage(RUSAGE_SELF, &before))
	    fprintf(stderr, "getrusage: %s\n", strerror(errno)), exit(1);

	ptr[0] = c;

	if (getrusage(RUSAGE_SELF, &after))
	    fprintf(stderr, "getrusage: %s\n", strerror(errno)), exit(1);

	printf("%-7li %-7li %-8c %-7i %-7li %-7li %-7li\n",
	       before.ru_majflt, before.ru_minflt,
	       isalnum(c) ? c : '?', initial, after.ru_majflt,
	       after.ru_minflt,
	       (after.ru_minflt + after.ru_majflt) - (before.ru_minflt +
						      before.ru_majflt));
    }

    return 0;
}
