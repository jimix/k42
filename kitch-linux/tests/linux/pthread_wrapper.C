/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 ******************************************************************************/

/* wrapper for pthreads to place events in the traces when acquiring and releasing locks */

#include <sys/sysIncs.H>

static inline KernelInfo volatile& kil() {
    KernelInfo volatile * x = (KernelInfo volatile *)0xe000000000000000ULL;
    return *x;
}

#define kernelInfoLocal kil()

#include <trace/traceDbg.h>

extern "C" {
#include <pthread.h>
#include <stdio.h>
}


extern int __pthread_mutex_lock(pthread_mutex_t *mutex);
extern int __pthread_mutex_unlock(pthread_mutex_t *mutex);
/*
extern int __pthread_create_2_1(pthread_t *__restrict __thread,
			   __const pthread_attr_t *__restrict __attr,
			   void *(*__start_routine) (void *),
			    void *__restrict __arg);

extern "C" 
int pthread_create (pthread_t *__restrict __thread,
			   __const pthread_attr_t *__restrict __attr,
			   void *(*__start_routine) (void *),
			   void *__restrict __arg) __THROW
{
  int ret;
  ret = __pthread_create_2_1(__thread,__attr,__start_routine,__arg);
  printf("thread created %p\n", (void*)__thread);
  return ret;
}
*/

extern "C" 
int pthread_mutex_lock(pthread_mutex_t *mutex) __THROW
{
  int ret;
  //printf("Grabbing lock %p\n",mutex);
  TraceOSDbgd1A((uval)mutex);
  ret =__pthread_mutex_lock(mutex);
  //  printf("Lock acquired %p\n", mutex);
  TraceOSDbgd1B((uval)mutex);
  return ret;
}

extern "C" 
int pthread_mutex_unlock(pthread_mutex_t * mutex) __THROW
{
  int ret;
  ret=   __pthread_mutex_unlock(mutex);
  //  printf("Releasing lock %p\n", mutex);
  TraceOSDbgd2B((uval)mutex,0);
  return ret;
}


