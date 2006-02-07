/******************************************************************************
 * This file originates from the linuxthreads/Examples directory from glibc,
 * and hence is covered by the GNU LGPL terms outlined there.
 *
 * $Id: ex1.c,v 1.4 2000/05/08 17:56:53 cbcoloha Exp $
 *****************************************************************************/
/* Creates two threads, one printing 10000 "a"s, the other printing
   10000 "b"s.
   Illustrates: thread creation, thread joining. */

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "pthread.h"

void * process(void * arg)
{
  int i;
  fprintf(stderr, "Starting process %s\n", (char *) arg);
  for (i = 0; i < 20 /* 10000 */; i++) {
    write(1, (char *) arg, 1);
    usleep(10); /* K42 */
  }
  return NULL;
}

int main(void)
{
  int retcode;
  pthread_t th_a, th_b;
  void * retval;

  /*K42 */fprintf(stderr, "about to create a\n");
  retcode = pthread_create(&th_a, NULL, process, (void *) "a");
  /*K42 */fprintf(stderr, "created a\n");
  if (retcode != 0) fprintf(stderr, "create a failed %d\n", retcode);
  /*K42 */fprintf(stderr, "about to create b\n");
  retcode = pthread_create(&th_b, NULL, process, (void *) "b");
  /*K42 */fprintf(stderr, "created b\n");
  if (retcode != 0) fprintf(stderr, "create b failed %d\n", retcode);
  /*K42 */fprintf(stderr, "about to join a\n");
  retcode = pthread_join(th_a, &retval);
  if (retcode != 0) fprintf(stderr, "join a failed %d\n", retcode);
  /*K42 */fprintf(stderr, "join a succeeded\n");
  /*K42 */fprintf(stderr, "about to join b\n");
  retcode = pthread_join(th_b, &retval);
  if (retcode != 0) fprintf(stderr, "join b failed %d\n", retcode);
  /*K42 */fprintf(stderr, "join b succeeded\n");
  return 0;
}
