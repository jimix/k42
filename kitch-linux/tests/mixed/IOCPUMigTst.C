/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * This file originates from the linuxthreads/Examples directory from glibc,
 * and hence is covered by the GNU LGPL terms outlined there.
 *
 * $Id: IOCPUMigTst.C,v 1.11 2003/06/04 14:15:54 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#include <scheduler/Scheduler.H>
#include <scheduler/SchedulerTimer.H>
#include <stub/StubTestScheduler.H>

#include <pthread.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER,
                mutex2 = PTHREAD_MUTEX_INITIALIZER,
                mutex3 = PTHREAD_MUTEX_INITIALIZER;

FILE *Fio, *Fcpu, *Fmix;

#define READ_SIZE 4096

#define IO_THR	1
#define CPU_THR 0
#define MIX_THR 0

long mix_time[MIX_THR];
long   mix_io_time[MIX_THR];
long   mix_cpu_time[MIX_THR];
long io_time[IO_THR];
long cpu_time[CPU_THR];

long page_time[3][2000];

int preemptNow = 0;

#undef DEBUG

// 10 ms time slices.
#define TIMESLICE	    Scheduler::TicksPerSecond() / 100
class TimerEventPreempt : public TimerEvent {
public:
    DEFINE_GLOBAL_NEW(TimerEventPreempt);

    virtual void handleEvent() {
	tassert(Scheduler::IsDisabled(),
		err_printf("Dispatcher not disabled.\n"));

	preemptNow = 1;

	// Schedule another timer event so we get called again:
	disabledScheduleEvent(TIMESLICE, TimerEvent::relative);
    }
};


// EP
int setPreemptTime(int ticks)
{
    TimerEventPreempt *p = new TimerEventPreempt;

    p->disabledScheduleEvent(TIMESLICE, TimerEvent::relative);
    return 0;
}


void * cpu_func (void *arg)
{
    unsigned long x=0;
    unsigned long z =0;
    long i =0;
    struct timeval tv1, tv2;

    setPreemptTime(0); // Initiate preemption.

    gettimeofday(&tv1, NULL);

    while (x<12)
    {
	x++;
	z=0;

	while(z<100000000UL)
	{
	    z++;
	    if (preemptNow)
	    {
		preemptNow = 0;
		gettimeofday(&tv2, NULL);
		i += (tv2.tv_sec - tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
		sched_yield();
		preemptNow = 0;
		gettimeofday(&tv1, NULL);
	    }
	}
    }

    gettimeofday(&tv2, NULL);

    i += (tv2.tv_sec - tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);

    // cast to long to be able to hold a 64-bit quantity
    // arg is a (void *) so compiler thinks it is a 64-bit pointer
    cpu_time[(long)arg]=i;

/*
    pthread_mutex_lock(&mutex1);
    fprintf(Fcpu,"tid:%lX,time: %4.2f\n",__k42_linux_threadSelf()->p_tid, ((double)i)/1000000.0);
    fflush(Fcpu);
    pthread_mutex_unlock(&mutex1);
*/

    return NULL;
}


void * mix_func (void *arg)
{
    int fd;
    char buf[READ_SIZE];
    struct timeval tv1, tv2, tv_cpu1, tv_cpu2, tv_io1, tv_io2;
    long i, j, cpu=0, io=0;
    unsigned long z;
    int ret, x;
    int num = (long)arg;
    int turn = num;


//    printf("Mixed thread %d launched\n",num);
    sprintf(buf,"hello%d.out",num);

    gettimeofday(&tv1, NULL);
    fd = open(buf,O_RDWR, S_IWUSR|S_IRUSR);
    if (fd<0)
    {
	printf("Error reading input file. errno=%d\n",errno);
	perror("open");
	return NULL;
    }

    for (i=0; i<24; i++)
    {

	if (turn%2)
	{
	    gettimeofday(&tv_io1, NULL);
	    for (j=0;j<83; j++)
	    {
		ret=read(fd, buf, READ_SIZE);
		if (ret!=READ_SIZE)
	        {
		    printf("Error reading: ret=%d, i=%ld (total chars read=%ld) errno=%d\n",ret,i*j,i*j*READ_SIZE,errno);
		    perror("read");
		    return NULL;
		}
	    }
	    gettimeofday(&tv_io2, NULL);
	    io += (tv_io2.tv_sec - tv_io1.tv_sec)*1000000+(tv_io2.tv_usec-tv_io1.tv_usec);
	}
	else
	{
	    gettimeofday(&tv_cpu1, NULL);

	    x=0;
	    while (x<1)
	    {
		x++;
		z=0;
#ifdef DEBUG
		printf("MIX%d: Doing CPU computation (Migratable: %d)\n",num,__k42_linux_isMig());
#endif /* #ifdef DEBUG */
		while(z<100000000UL)
		{
		    z++;
		    if (preemptNow)
		    {
			preemptNow = 0;
			gettimeofday(&tv_cpu2, NULL);
			cpu += (tv_cpu2.tv_sec - tv_cpu1.tv_sec)*1000000+(tv_cpu2.tv_usec-tv_cpu1.tv_usec);
//			printf("MIX%d: yielding\n",num);
			sched_yield();
//			printf("MIX%d: coming back\n",num);
			preemptNow = 0;
			gettimeofday(&tv_cpu1, NULL);
		    }
		}
	    }
	    gettimeofday(&tv_cpu2, NULL);
	    cpu += (tv_cpu2.tv_sec - tv_cpu1.tv_sec)*1000000+(tv_cpu2.tv_usec-tv_cpu1.tv_usec);
	}
	turn++;
    }
    close(fd);

    gettimeofday(&tv2, NULL);

    i = (tv2.tv_sec - tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);

/*
    pthread_mutex_lock(&mutex3);

    fprintf(Fmix,"tid:%lX,time: %ld, cpu:%4.2f, io:%4.2f (cpu+io):%ld\n",__k42_linux_threadSelf()->p_tid, i, ((double)cpu)/1000000.0, ((double)io)/1000000.0, (cpu+io)/1000000);
    fflush(Fmix);

    pthread_mutex_unlock(&mutex3);
*/
    mix_time[num]=i;
    mix_io_time[num]=io;
    mix_cpu_time[num]=cpu;

#ifdef DEBUG
    printf("Finishing MIXED task\n");
    fflush(stdout);
#endif /* #ifdef DEBUG */

    return NULL;
}


void * io_func (void *arg)
{
    int fd;
    char buf[READ_SIZE];
    long i, ret;
    struct timeval tv1, tv2, ptv1, ptv2;

    sprintf(buf,"hello%ld.out",(long)arg);

#ifdef DEBUG
    printf("IO intensive thread %d launched. Reading file %s\n",(int)arg,buf);
    fflush(stdout);
#endif /* #ifdef DEBUG */

    gettimeofday(&tv1, NULL);

    fd = open(buf, /*O_CREAT|O_TRUNC|*/O_RDWR, S_IWUSR|S_IRUSR);
    if (fd<0)
    {
	printf("Error reading data file. To create the files, use -c flag\n");
	perror("open");
	return NULL;
    }
    for (i=0; i<996; i++) // Can go as high as 2000.
    {
	gettimeofday(&ptv1, NULL);
	ret=read(fd, buf, READ_SIZE);
	gettimeofday(&ptv2, NULL);
	page_time[(long)arg][i]=(ptv2.tv_sec-ptv1.tv_sec)*1000000+(ptv2.tv_usec-ptv1.tv_usec);
	if (ret!=READ_SIZE)
	{
	    printf("Error reading: ret=%ld, i=%ld (total chars read=%ld) errno=%d\n",ret,i,i*READ_SIZE,errno);
	    perror("read");
	    return NULL;
	}
#ifdef DEBUG
	if ((i%100)==0)
	{
	    printf("Reading file (iteration %ld) (VP: %d migratable: %d, pid:%lX, tid:%lX)\n",i,__k42_linux_GetVP(),__k42_linux_isMig(),__k42_linux_threadSelf()->p_pid,__k42_linux_threadSelf()->p_tid);
	    fflush(stdout);
	}
#endif /* #ifdef DEBUG */
    }
    close(fd);

    gettimeofday(&tv2, NULL);
    i = (tv2.tv_sec - tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
    io_time[(long)arg]=i;
/*
    pthread_mutex_lock(&mutex2);
    fprintf(Fio,"tid:%lX,time: %4.2f\n",__k42_linux_threadSelf()->p_tid, ((double)i)/1000000.0);
    fflush(Fio);
    pthread_mutex_unlock(&mutex2);
*/

#ifdef DEBUG
    printf("Finishing IO-intensive task\n");
    fflush(stdout);
#endif /* #ifdef DEBUG */

    return NULL;
}

int main (int argc, char **argv)
{
  int retcode;
  pthread_t th_cpu[CPU_THR], th_io[IO_THR], th_mix[MIX_THR];
  void * retval;
  long i;   // long so that we can cast it to (void *)
  struct timeval tv1, tv2;

/*
  pthread_mutex_init(&mutex1, NULL); //PTHREAD_MUTEX_FAST_NP);
  pthread_mutex_init(&mutex2, NULL); //PTHREAD_MUTEX_FAST_NP);
  pthread_mutex_init(&mutex3, NULL); //PTHREAD_MUTEX_FAST_NP);
*/

  setPreemptTime(0); // Initiate preemption.

#if 0
  // Testing locks and writes to files.
  pthread_mutex_lock(&mutex1);
  printf("Printing locked\n");
  Fio=fopen("out-io.txt","wt");
  fprintf(Fio,"FOOOO\n");
  fflush(Fio);
  fclose(Fio);
  pthread_mutex_unlock(&mutex1);
  return;
#endif /* #if 0 */

  printf("Starting...\n");
  fflush(stdout);

  if (argc==2) {
      printf("Creating hello files in the current directory\n");
      FILE *F;
      char buf[20];
      int i = 0;

      if (argv[1][1]!='c') {
	  printf("Invalid flag\n");
	  return 0;
      }

      while(i<8) {
	  int z = 0;
	  char bigbuf[1024];
	  sprintf(buf,"hello%d.out",i);
	  F = fopen(buf,"wt");
	  memset(bigbuf,'*',1024);
	  while (z<8192) {
	      fprintf(F,bigbuf);
	      z++;
	  }
	  fflush(F);
	  fclose(F);
	  i++;
      }
  }

  Fio = fopen("out-io.txt","at");
  if (!Fio) {
      printf("Error opening IO file\n");
      perror("fopen");
      return -1;
  }
  Fcpu = fopen("out-cpu.txt","at");
  if (!Fcpu) {
      printf("Error opening CPU file\n");
      perror("fopen");
      return -1;
  }
  Fmix = fopen("out-mix.txt","at");
  if (!Fmix)
  {
      printf("Error opening MIX file\n");
      perror("fopen");
      return -1;
  }

  printf("After opening files\n");
  gettimeofday(&tv1, NULL);

  for (i=0; i<CPU_THR; i++)
  {
      retcode = pthread_create(&(th_cpu[i]), NULL, cpu_func, (void *)i);
      fprintf(stderr, "created cpu int %ld (tid:%ld)\n",i,th_cpu[i]);
      if (retcode != 0) fprintf(stderr, "create CPU failed %d\n", retcode);
  }
  for (i=0; i<MIX_THR; i++)
  {
      retcode = pthread_create(&(th_mix[i]), NULL, mix_func, (void *)i);
      fprintf(stderr, "created cpu int %ld (tid:%ld)\n",i,th_mix[i]);
      if (retcode != 0) fprintf(stderr, "create CPU failed %d\n", retcode);
  }
  for (i=0; i<IO_THR; i++)
  {
      retcode = pthread_create(&(th_io[i]), NULL, io_func, (void *)i);
      fprintf(stderr, "created io int %ld (tid:%ld)\n",i,th_io[i]);
      if (retcode != 0) fprintf(stderr, "create IO failed %d\n", retcode);
  }

  fprintf(stderr, "All processes launched. Waiting for them\n");

  for (i=0; i<IO_THR; i++)
  {
      retcode = pthread_join(th_io[i], &retval);
      if (retcode != 0) fprintf(stderr, "join b failed %d\n", retcode);
      fprintf(stderr, "join io succeeded\n");
  }
  for (i=0; i<CPU_THR; i++) {
      retcode = pthread_join(th_cpu[i], &retval);
      if (retcode != 0) fprintf(stderr, "join a failed %d\n", retcode);
      fprintf(stderr, "join cpu succeeded\n");
  }
  for (i=0; i<MIX_THR; i++) {
      retcode = pthread_join(th_mix[i], &retval);
      if (retcode != 0) fprintf(stderr, "join a failed %d\n", retcode);
      fprintf(stderr, "join mix succeeded\n");
  }

  gettimeofday(&tv2, NULL);
  fprintf(stderr,"Done: total time: %ld s\n",tv2.tv_sec-tv1.tv_sec);

  for (i=0; i<IO_THR; i++)
  {
//      int j;
      fprintf(Fio,"%ld: io time: %4.2f\n",i,((double)io_time[i])/1000000.0);
/*
      for(j=0; j<1000; j++)
	  fprintf(Fio,"Page %d, Time: %6.4f\n",j,((double)page_time[i][j])/1000000.0);
*/
  }
  fflush(Fio);

  fprintf(Fcpu,"cpu time: ");
  for (i=0; i<CPU_THR; i++)
      fprintf(Fcpu,"%4.2f, ",((double)cpu_time[i])/1000000.0);
  fprintf(Fcpu,"\n");
  fflush(Fcpu);

  for (i=0; i<MIX_THR; i++)
      fprintf(Fmix,"%ld: Mix time: %4.2f cpu:%4.2f io:%4.2f\n",i,((double)mix_time[i])/1000000.0, ((double)mix_cpu_time[i])/1000000.0, ((double)mix_io_time[i])/1000000.0);
  fflush(Fmix);

  return 0;
}

