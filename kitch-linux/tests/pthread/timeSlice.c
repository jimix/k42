/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: timeSlice.c,v 1.1 2003/09/12 13:36:57 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Play with pthread time slicing.
 * **************************************************************************/
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define NTHREADS 2000
void thread_function();

long thread_counter[NTHREADS];

volatile long long counter = 0;
volatile int flag = 0;

long spin;

int main(int argc, char *argv[])
{
    pthread_t thread_id[NTHREADS];
    long i, n;

    if (argc != 3) {
	fprintf(stderr,
		"Usage: %s <num_threads> <spin_count>\n", argv[0]);
	exit(-1);
    }
    n = atoi(argv[1]);
    spin = atoi(argv[2]);

    assert(n <= NTHREADS);
    for (i = 0; i < n; i++) {
	pthread_create(&thread_id[i], NULL, (void*)&thread_function, (void*)i);
    }

    sleep(1);

    flag = 1;

    sleep(10);

    flag = 2;

    for (i = 0; i < n; i++) {
	pthread_join(thread_id[i], NULL); 
	printf(" %ld", thread_counter[i]);
    }
    printf("\n");

    return 0;
}

void thread_function(long n)
{
    long long i;
    long counter;

    while (flag == 0) {
	sched_yield();
    }

    counter = 0;
    while (flag == 1) {
	for (i = 0; i < spin; i++);
	counter++;
    }

    thread_counter[n] = counter;

    pthread_exit(0);
}
