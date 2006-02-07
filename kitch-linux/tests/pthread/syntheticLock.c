/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: syntheticLock.c,v 1.2 2005/06/16 18:32:32 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Play with pthread locking.
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define __USE_GNU
#include <pthread.h>

#define NTHREADS 2000
void thread_function();

pthread_mutex_t mutex1 = PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP;
//pthread_mutex_t mutex1 = {0, 0, 0, PTHREAD_MUTEX_ADAPTIVE_NP,
//						__LOCK_INITIALIZER};
volatile long long counter = 0;
volatile int flag = 0;

long think_spin, lock_spin;

int main(int argc, char *argv[])
{
    pthread_t thread_id[NTHREADS];
    int i, n;
    struct timeval tv;
    long long t1, t2, c1, c2;
    double throughput;

    if (argc != 4) {
	fprintf(stderr, "Usage: %s <num_threads> "
			    "<think_spin_count> <lock_spin_count>\n", argv[0]);
	exit(-1);
    }
    n = atoi(argv[1]);
    think_spin = atoi(argv[2]);
    lock_spin = atoi(argv[3]);

    assert(n <= NTHREADS);
    for (i = 0; i < n; i++) {
	pthread_create(&thread_id[i], NULL, (void*)&thread_function, NULL);
    }

    sleep(1);

    c1 = counter;
    gettimeofday(&tv, NULL);
    t1 = (1000000L * tv.tv_sec) + tv.tv_usec;

    sleep(10);

    c2 = counter;
    gettimeofday(&tv, NULL);
    t2 = (1000000L * tv.tv_sec) + tv.tv_usec ;

    flag = 1;

    for (i = 0; i < n; i++) {
	pthread_join( thread_id[i], NULL); 
    }
    throughput = (((double) (c2 - c1)) * 1000000) / ((double) (t2 - t1));
    printf("Clients:%d Throughput:%f\n", n, throughput);

    return 0;
}

void thread_function()
{
    long long i;
    while (!flag) {
	for (i = 0; i < think_spin; i++);
	pthread_mutex_lock(&mutex1);
	for (i = 0; i < lock_spin; i++);
	counter++;
	pthread_mutex_unlock(&mutex1);
    }
    pthread_exit(0);
}
