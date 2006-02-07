/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: test1.c,v 1.4 2000/05/11 11:28:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Stress test for pthreads -- see what happens when we
 *                     try to create a whole bunch of threads.
 * **************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

void *
test_thread (void *v_param)
{
    while(1) {
	usleep(10);
	sched_yield();
    }
}

#define MAX_THREADS 2000

int
main (void)
{
  unsigned long count;
  pthread_t threads[MAX_THREADS];
  int status;


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
  for (count = 0; count < MAX_THREADS; ++count) {
      status = pthread_join (threads[count], NULL);

      if (status != 0)
	{
	  printf ("join status = %d, count = %lu: %s\n", status, count,
		  strerror (errno));
	  return 1;
	}
      else
	{
	  printf ("join count = %lu\n", count);
	}
      usleep (10);
  }
  return 0;
}
