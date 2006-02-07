/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: spread.C,v 1.4 2005/06/28 19:48:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: program to run x instances of an app on x processors
 *		       For each instance, it sets the environment variable
 *		       NUMCPUS to identify which cpu the process is running
 *		       on.
 *
 * spread [-t TRACE] [-s TIME] <numcpus> <args to exec......>
 *        -t  trace mask to use
 *        -s  expected run-time of processes
 *	  Arguments must be in order as shown
 *
 * **************************************************************************/
#ifdef K42
#include <sys/sysIncs.H>
#include <sys/SystemMiscWrapper.H>
#include <sys/ResMgrWrapper.H>
#include <sys/ProcessWrapper.H>
#include <scheduler/Scheduler.H>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>




int ncpus = 0;

// Pipe is actually array of 4 fd's, 2 pipes.
// First pipe is to read for signal to commence processes
// Second pipe tells parent we're ready to go
int runChild(int cpu, int *pipe, int argc, char *argv[])
{
    int ret;
    fd_set rd;

#ifdef K42
    DREFGOBJ(TheResourceManagerRef)->migrateVP(0, cpu%ncpus);
#endif
    FD_ZERO(&rd);
    FD_SET(pipe[0], &rd);
    fprintf(stderr,"Started %d %s\n",cpu, argv[0]);

    // Notify parent we're ready
    write(pipe[3],"1",1);

    do {
	ret = select(pipe[0]+1, &rd, NULL, NULL, NULL);
    } while (ret!=1);

    char numcpus[32];
    snprintf(numcpus, 32, "%d", cpu);

    setenv("NUMCPUS", numcpus, 1);

    struct timeval start = {0,0};
    struct timeval end = {0,0};
    gettimeofday(&start, NULL);
    pid_t pid = fork();
    if (pid==0) {
	ret = execvp(argv[0], argv);
	exit(ret);
    }

    int status;
    pid_t x;
    do {
	x = waitpid(pid,&status,0);
    } while (x!=pid);
    gettimeofday(&end, NULL);

    end.tv_sec -= start.tv_sec;
    if (end.tv_usec < start.tv_usec) {
	end.tv_sec -= 1;
	end.tv_usec+= 1000000;
    }
    end.tv_usec -= start.tv_usec;
    sleep(1);
    fprintf(stderr, "%2d done: %d %ld.%06ld\n", cpu,
	    status, end.tv_sec, end.tv_usec);
    status = write(pipe[1], &end, sizeof(end));
    if (status!=sizeof(end)) {
	fprintf(stderr, "bad pipe write: %d %d\n",status, errno);
    }
    sleep(2);
    exit(0);
}



int
main(int argc, char *argv[])
{

    int fd[4];

    pipe(&fd[0]);
    pipe(&fd[2]);

    char* trace = NULL;
    int sleepTime = 15;
    if (strcmp(argv[1],"-t")==0) {
	trace = argv[2];
	argv+=2;
	argc-=2;
    }

    if (strcmp(argv[1],"-s")==0) {
	sleepTime = strtoul(argv[2],NULL,0);
	argv+=2;
	argc-=2;
    }

    ncpus = strtoul(argv[1],NULL,0);
    --argc;
    ++argv;

    pid_t children[ncpus];

    // This makes argv[0] "argv[0]" as expected in new process
    --argc;
    ++argv;

    int callbacks=ncpus;
    for (int i=0; i<ncpus; ++i) {
	children[i] = fork();
	if (children[i]==0) {
	    runChild(i,&fd[0],argc, argv);
	}
    }
    sleep(2);

    int ready = 0;
    while (ready<ncpus) {
	char c;
	int err = read(fd[2], &c, 1);
	if (err!=1) {
	    fprintf(stderr, "Read pipe error: %d %s\n", err, strerror(errno));
	    exit(-1);
	}
	++ready;
    }

    if (!trace) {
	trace = getenv("TRACE");
    }
    int trfd = 0;
#ifdef K42
    if (trace) {
	trfd = open("/ksys/traceMask",O_RDWR);
	char buf[64];
	snprintf(buf, 64, "%s\n",trace);
	write(trfd, buf, strlen(buf));
    }
#endif // K42

    write(fd[1],"1",1);
    char dummy;
    sleep(sleepTime);

    int status;
    fprintf(stderr,"About to wait...\n");
    while (callbacks) {
	pid_t x = waitpid(-1,&status,0);
	fprintf(stderr,"wait returned: %d\n",x);
	if (x==-1) {
	    fprintf(stderr,"Wait error: %d %d\n",x, errno);
	}
	--callbacks;
    }

#ifdef K42
    if (trace)
	write(trfd,"0\n",2);
#endif // K42

    read(fd[0],&dummy,1);

    struct timeval t[ncpus];
    int ret = read(fd[0], &t[0], sizeof(struct timeval)*ncpus);
    if (ret!=(int)sizeof(struct timeval)*ncpus) {
	fprintf(stderr,"bad pipe read: %d %d\n", ret, errno);
    }
    struct timeval avg = {0,0};
    for (int i = 0; i<ncpus; ++i) {
	avg.tv_sec += t[i].tv_sec;
	avg.tv_usec += t[i].tv_usec;
    }
    avg.tv_usec += 1000000* (avg.tv_sec%ncpus);
    avg.tv_sec /= ncpus;
    avg.tv_usec /= ncpus;
    avg.tv_sec += (avg.tv_usec/1000000);
    avg.tv_usec %= 1000000;


    printf("cpus: %d time: %ld.%06ld\n", ncpus, avg.tv_sec, avg.tv_usec);
    return 0;
}
