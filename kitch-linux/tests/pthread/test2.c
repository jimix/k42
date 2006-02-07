/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: test2.c,v 1.4 2000/05/11 11:28:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Test for pthreads -- see if migration works.
 * **************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

extern int pthread_setconcurrency(int level);

void *
test_thread (void *v_param)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    // Loop around doing a whole bunch of nothing, consuming CPU.
    // Hopefully the migration manager will load balance these
    // threads:
    while(1) {
        pthread_testcancel();
	sched_yield();
    }
}

#define MAX_THREADS 10
int
main (void)
{
  unsigned long count;
  pthread_t threads[MAX_THREADS];
  int status;

  // Create a bunch of threads:
  for (count = 0; count < MAX_THREADS; ++count)
    {
      status = pthread_create (&threads[count], NULL, test_thread, NULL);
      if (status != 0)
	{
	  printf ("create status = %d, count = %lu: %s\n", status, count,
		  strerror (errno));
	  return 1;
	}
      else
	{
	  printf ("create count = %lu\n", count);
	}
      usleep (10);
    }
  // Tell pthreads to use more than one VP:
  pthread_setconcurrency(4);

  // Go to sleep, allowing our threads to move around some:
  sleep(20);

  // Kill off all of the threads:
  for (count = 0; count < MAX_THREADS; ++count) {
      status = pthread_cancel (threads[count]);

      if (status != 0)
	{
	  printf ("cancel status = %d, count = %lu: %s\n", status, count,
		  strerror (errno));
	  return 1;
	}
      else
	{
	  printf ("cancel count = %lu\n", count);
	}
      usleep (10);
  }
  return 0;
}
