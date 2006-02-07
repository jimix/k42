/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: newmemclone.C,v 1.3 2005/06/28 19:44:15 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:  program to display DispatchQueue statistics
 ****************************************************************************/

#define K42_STUFF

#ifdef K42_STUFF
#include <sys/sysIncs.H>
#include <stub/StubRegionDefault.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>
#include <sys/systemAccess.H>
#endif

#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

extern int      optind, opterr;
extern char     *optarg;

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define STRIDE    PAGE_SIZE

int     bytes=0;
int     sleepsec=0;
int     verbose=0;
int     forkcnt=1;
int     repeatcount=1;
int     do_bzero=0;
int     mypid;
int     timeThreads=0;
int     verboseTimes=0;
int     useTraceBuffer=0;
int     useOptimizedBzero=0;
int     useSingleRegion=0;
int     regionDirective=0;
int     preFaultAPage=0;
char     *regionStart=0;

struct ThreadTimes {
    long long pad1[16];
    long long start;
    long long end;
    long long pad2[16];
} *threadTimes;

// pthread_mutex_t lock;
volatile int    go, state[128];

#define perrorx(s)      (perror(s), exit(1))

#ifdef K42_STUFF

#define CLL 128

void
k42AlignedBZero(void * t, uval count)
{
    register uval p=(uval)t;

    for (p=(uval)t;p<(((uval)t)+count);p+=CLL) {
	__asm__ ("dcbz 0,%0" : : "r" (p));
    }
}

char *
k42CreateRegion(uval size, uval perProcSize)
{
    SysStatus rc;
    uval regionAddr;
    ObjectHandle frOH;
    uval directive = regionDirective;

    printf("create K42 Region directive=0x%lx: ",directive);
    switch(directive & 0x3) {
    case 0:
        printf("FCMDefault");
        break;
    case 1:
        printf("FCMDefaultMultiRep");
        break;
    case 2:
        printf("FCMPartitioned");
        switch ((directive & 0x18)>>3) {
        case 0:
            printf("FCMPartitioned(%ld,"
                   "PageListDisposition::CENRALIZED", perProcSize);
            break;
        case 1:
            printf("FCMPartitioned(%ld,"
                   "PageListDisposition::DISTRIBUTED", perProcSize);
            break;
        case 2:
            printf("FCMPartitioned(%ld,"
                   "PageListDisposition::GC", perProcSize);
            break;
        default:
            printf("unknown FCM Partition option specified by directive\n");
            exit(0);
        }
        break;
    default:
        printf("unknown FCM type specified\n");
        exit(0);
    };

    if (directive & 0x4) {
        printf(" RegionDefaultMulitRep\n");
    } else {
        printf(" RegionDefault\n");
    }

    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    rc = StubFRComputation::_Create(frOH);

    rc = StubRegionDefault::
#if 0	// FIXME:  Test interface not committed yet
    _CreateFixedLenExtTest
#else
    _CreateFixedLenExt
#endif
    (
        regionAddr,                            // start address
        size,                                  // size of region
        SEGMENT_SIZE,                          // alignment requirement
        frOH,                                  // fr
        0,                                     // file offset
        (uval)(AccessMode::writeUserWriteSup), // access requirement
        0,                                     // Xhandle target
        RegionType::K42Region                  // region type
#if 0	// FIXME:  Test interface not committed yet
        , directive                            // test directive to control
                                               // type of FCM and Region created
        , perProcSize                          // in the case of a partitioned
                                               // fcm we use this to determine
                                               // partition size 
#endif
    );

    passert(_SUCCESS(rc), err_printf("Region Creation failed woops\n"));

    // don't need the object handle any more
    Obj::ReleaseAccess(frOH);

    SystemExit(&saveArea);

    return (char *)regionAddr;
}

#endif  /* K42_STUFF */

void* test(void*);
char* preTest(long);
void  postTest(long);
void  work(char *, long);
void  launch(void);
void  printTimes(void);

int
main (int argc, char *argv[])
{
        int                     i,c, stat, er=0;
        static  char            optstr[] = "p:b:f:r:s:R:vztTBZPH";

        // Initialized bytes to a value of 4 pages
        bytes=4*PAGE_SIZE;

        opterr=1;
        while ((c = getopt(argc, argv, optstr)) != EOF)
                switch (c) {
                case 'p':
                    bytes = atoi(optarg)*PAGE_SIZE;
                    break;
                case 'b':
                        printf("-b not supported directly use -p to specify"
                               " size in pages\n");
                        exit(0);
                        bytes = atoi(optarg);
                        break;
                case 'f':
                        forkcnt = atoi(optarg);
                        break;
                case 'r':
                        repeatcount = atoi(optarg);
                        break;
                case 's':
                        sleepsec = atoi(optarg);
                        break;
                case 'R':
                        useSingleRegion++;
                        regionDirective = atoi(optarg);
                        break;
                case 'v':
                        verbose++;
                        break;
                case 'z':
                        do_bzero++;
                        break;
                case 'H':
                        er++;
                        break;
                case 't':
                        timeThreads++;
                        break;
                case 'T':
                        verboseTimes++;
                        break;
                case 'B':
                        useTraceBuffer++;
                        break;
                case 'Z':
                        useOptimizedBzero++;
                        break;
                case 'P':
                        preFaultAPage++;
                        break;
                case '?':
                        er = 1;
                        break;
                }

        if (er) {
                printf("usage: %s %s\n", argv[0], optstr);
                exit(1);
        }

        if (timeThreads) {
            threadTimes=(struct ThreadTimes *)
                malloc(sizeof(struct ThreadTimes)*forkcnt);
        }

        mypid = getpid();
        setpgid(0, mypid);


        for (i=0; i<repeatcount; i++) {
                if (fork() == 0)
                        launch();
                while (wait(&stat) > 0);
        }
        exit(0);
}

void
printTimes()
{
    long long sum, total, mn, mx;
    double avg;
    struct ThreadTimes *ts;
    int i;

    if (timeThreads) {
        sum=0; total=0;
        mn = threadTimes[0].end - threadTimes[0].start;
        mx = mn;

        for (i=0;i<forkcnt; i++) {
            ts=&(threadTimes[i]);
            total=ts->end - ts->start;
            if (total<mn) mn=total;
            if (total>mx) mx=total;
            sum+=total;
            if (verboseTimes) 
                printf("%d: start=%lld end=%lld total=%lld(%f s)\n",
                       i, ts->start, ts->end, total,
                       (double)total/(double)1000000);
        }
        avg=(double)sum/(double)forkcnt;

        printf("min=%lld max=%lld range=%lld(%f s) avg=%f(%f s)\n",
               mn, mx, mx-mn,
               ((double)(mx-mn))/(double)1000000,
               avg, avg/(double)1000000);
            
    }
}

#ifdef K42_STUFF
#include <sys/arch/powerpc/asmConstants.H>
#define kernelInfoLocal 0xe0000000000fe000ULL
#endif

char *
pretest(long id)
{
    char    *p = NULL;

    if (useTraceBuffer) {
#       ifdef K42_STUFF
        char * traceArray = *((char **)(kernelInfoLocal+KI_TI_traceArray));
        long long indexMask = *((long long *)
                                (kernelInfoLocal+KI_TI_indexMask));
        if (bytes >= ((indexMask+1) * 8)) {
            printf("oops not enough trace buffer space set env correctly\n");
            exit(0);
        }
        p = traceArray;
#       endif
    } else {
        if (useSingleRegion) {
            p = regionStart + (bytes * id);
        } else {
            p = (char *)malloc(bytes);
        }
    }

#   ifdef K42_STUFF
    if (verbose) {
        long long proc=*((long long *)(kernelInfoLocal + (8 + 8  + 8 + 8)));
        printf("%ld: preTest: p=%p physProc=%lld\n" , id, p, proc);
    }
#   endif

#   ifdef K42_STUFF
    if (useOptimizedBzero) { 
        if (((((uval)p)|((uval)bytes))&(CLL-1)) != 0) {
            printf("bad alignment PageCopy::Memset\n");
            exit(0);
        }
    }
#   endif

    if (preFaultAPage) {
        *p='p';
    }

    return p;
}

void
postTest(long id)
{
    if (sleepsec != 0) sleep(sleepsec);

#   ifdef K42_STUFF
    if (verbose) {
        long long proc=*((long long *)(kernelInfoLocal + (8 + 8  + 8 + 8)));
        printf("%ld: postTest:  physProc=%lld\n" , id, proc);
    }
#   endif 

    state[id] = 2;
}

void
work(char *p,long id)
{
    char *pe;
    struct timeval tv;
    struct ThreadTimes *ts=0;

    if (timeThreads) {
        ts=&(threadTimes[id]);
        gettimeofday(&tv, NULL);
        ts->start=(1000000L * tv.tv_sec) + tv.tv_usec;
    }

    if (do_bzero) {
#       ifdef K42_STUFF
        if (useOptimizedBzero)
            k42AlignedBZero(p, (uval)bytes);
        else
            bzero(p,bytes);
#       else
          bzero(p, bytes);
#       endif
    } else {
        for(pe=p+bytes; p<pe; p+=STRIDE)
            *p = 'r';
    }

    if (timeThreads) {
        gettimeofday(&tv, NULL);
        ts->end=(1000000L * tv.tv_sec) + tv.tv_usec;
    }
}

void
createRegion(int size, int numThreads)
{
#   ifdef K42_STUFF
       regionStart=k42CreateRegion(size*numThreads,size);
#   else
       regionStart=(char *)malloc(size*numThreads);
#   endif
}

void
launch()
{
        pthread_t ptid[128];
        long     j;
        char    *p;

        /* if we are supposed to be using a single region then allocate it */
        /* now */
        if (useSingleRegion) {
            createRegion(bytes,forkcnt);
        }

        /* initialize go variable used to synchronize with other threads */
        go = 0;

        /* launch threads for other processors */
        for (j=1; j<forkcnt; j++) {
            state[j]=0;
            if (pthread_create(&ptid[j], NULL, test, (void*) (long)j) < 0)
                perrorx("pthread create");
        }

        /* set up test for this processor */
        p=pretest(0);

        /* wait for all threads to finish pretest and line up*/
        for (j=1;j<forkcnt;j++)
            while(state[j] == 0);

        /* tell other threads to start the real work */
        go = 1;

        /* do the work for this processor on our thread */
        work(p,0);

        /* just to be semetric we do a post test for this thread */
        postTest(0);

        /* wait for other threads to complete */
        for (j=1; j<forkcnt; j++)
            while(state[j] == 1);
                
        printTimes();

        /* tell other threads they can terminate now */
        go = 2;

        for (j=1; j<forkcnt; j++)
                pthread_join(ptid[j], NULL);

        exit(0);
}

void*
test(void *arg)
{
    long    id;
    id = (long) arg;
    char *p;

    p=pretest(id);

    state[id] = 1;

    while(!go);

    work(p,id);

    postTest(id);

    while(go!=2);

    pthread_exit(0);
}
