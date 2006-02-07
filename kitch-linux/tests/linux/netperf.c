/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: netperf.c,v 1.12 2002/11/05 22:25:02 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for linux network performance
 * **************************************************************************/
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>
#include <math.h>
#ifdef K42
#include "pthread.h"
extern int pthread_setconcurrency(int level);
#else /* #ifdef K42 */
#include <pthread.h>
#endif /* #ifdef K42 */


char *host = "9.2.208.162";
volatile int messages=1000;
int delay=200000;
volatile int tsync = 0;
int port =5123;
int concur = 1;
volatile int timeouts = 0;
volatile int delayed = 0;
volatile int dropped = 0;
volatile int delay_count=0;
int *thread_sync=NULL;
int psize = 256;
struct timeval start,end;
int balance = 0;
int divide=0;
volatile int total_delay=0;

void* run_thread(void* arg)
{
    int fd;
    long id = (long)arg;
    long k;
    int lport;
    struct sockaddr_in sin;
    struct timeval tv = {0,0};
    int buf[psize/sizeof(int)];
    int sent=0;
    long long rcvd=0;
    int *d;
    d=(int*)malloc(sizeof(int)*4);
    memset(d,0,sizeof(int)*4);
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    fcntl(fd,F_SETFL,O_NONBLOCK);

    if (fd == -1) {
	printf("socket() failed\n");
	perror("error was");
	return (void*)-1;
    }


    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_family = AF_INET;
    sin.sin_port = 0;
    bind(fd,(struct sockaddr*)&sin,sizeof(sin));

    lport = ntohs(sin.sin_port);
    sin.sin_port = htons(port+id);


    /* localhost */
    if (inet_aton(host, &sin.sin_addr) == 0) {
	perror("inet_aton failed");
	return 0;
    }
    k = messages;

    thread_sync[id] = fd;
    while(tsync==0){
    }

    while(k>0) {
	unsigned long len;
	int rc;
	struct timeval start;
	struct timeval send;
	struct timeval sel;
	struct timeval end;

	fd_set set;
	if(tv.tv_sec==0 && tv.tv_usec==0){
	    buf[0] = sent++;
	    gettimeofday(&start,NULL);
	    rc = sendto(fd, buf , psize, 0,(struct sockaddr*)&sin,
			sizeof(struct sockaddr_in));
	    gettimeofday(&send,NULL);

	    if(timeouts>3000){
		printf("sendto: %i %i\n",fd,rc);
	    }
	    if (rc == -1) {
		perror("sendto error");
		return (void*)-1;
	    }
	    tv.tv_sec = delay / 1000000;
	    tv.tv_usec = delay % 1000000;
	}

	FD_ZERO(&set);
	FD_SET(fd,&set);
	rc = select(fd+1,&set,NULL,NULL,&tv);
	if(rc==1){
	    gettimeofday(&sel,NULL);
	    len = read(fd, buf, psize);
	    gettimeofday(&end,NULL);
	    if (len == -1) {
		printf("Read error %i %s\n",errno,strerror(errno));
		return (void*)-1;
	    }

	    d[0] += (send.tv_sec*1000000 + send.tv_usec)-(start.tv_sec*1000000+start.tv_usec);
	    d[1] += (sel.tv_sec*1000000 + sel.tv_usec)-(send.tv_sec*1000000+send.tv_usec);
	    d[2] += (end.tv_sec*1000000+end.tv_usec)-(sel.tv_sec*1000000 + sel.tv_usec);
	    d[3] += delay - (tv.tv_sec*1000000 + tv.tv_usec);
	    total_delay+=d[3];
	    delay_count++;
	    // If we had a delay, don't reset tv ---
	    // we want to poll again to get the next packet
	    // which is potentially in the socket
	    if( buf[0]<(sent-1)){
		--dropped;
		++delayed;
	    }else{
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	    }

	    ++rcvd;
	    --k;
	    if(balance){
		k=--messages;
	    }
	}else{
	    ++dropped;
	    ++timeouts;
	}
    }
    gettimeofday(&end,NULL);
    return (void*)d;
}

void usage(){
    fprintf(stderr,"Usage: netperf [-h <host>] [-p <port>]"
	    " [-d <delay>] [-c <count>] [-t <threads>] [ -C <#VP>]\n");
    fputs(	"    -h <host>    Host to send packets to."
		" (Must be in number/dots format.)(9.2.208.157)\n"
		"    -p <port>    Port to send packets to. (5123)\n"
		"    -c <count>   Number of packets to send per thread.\n"
		"    -s <bytes>   Bytes per packet.(256)\n"
		"    -t <threads> Number of threads. (1)\n"
		"    -a		  Use application thread scope\n"
		"    -C <#VP>	  Number of VP's to use. (1)\n"
		"    -b           Balance workload among threads.\n"
		"    -B           Balance workload among threads.\n"
		"    -d <delay>   Timeout to retransmit (useconds).(200000)\n"
		"    -x           Use tread() instead of select().\n",
		stderr);
}
int
#ifdef K42
main(int argc, char *argv[])
#else /* #ifdef K42 */
main(int argc, char *argv[])
#endif /* #ifdef K42 */
{
    unsigned long c;
    int port = 0;
    const char *optlet = "ah:c:t:d:p:C:s:bBx";
    extern char *optarg;
    int t=1;
    int squares=0;
    int res[4]={0,};
    int total;
    pthread_t **thr;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM);

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'a':
     	    pthread_attr_setscope(&attr,PTHREAD_SCOPE_PROCESS);
	    break;
	case 'h':
	    host = optarg;
	    break;
	case 'b':
	    balance = 1;
	    break;
	case 'B':
	    divide = 1;
	    break;
	case 'c':
	    messages = strtol(optarg,(char**)NULL,10);
	    break;
	case 't':
	    t = strtol(optarg,(char**)NULL,10);
	    break;
	case 'd':
	    delay = strtol(optarg,(char**)NULL,10);
	    break;
	case 'p':
	    port = strtol(optarg,(char**)NULL,10);
	    break;
	case 'C':
	    concur = strtol(optarg,(char**)NULL,10);
	    break;
	case 's':
	    psize = strtol(optarg,(char**)NULL,10);
	    break;
	default:
	    usage();
	    return 1;
	}
    }

    if(divide)
	messages /= t;
#ifdef K42
    pthread_setconcurrency(concur);
#endif /* #ifdef K42 */
    thr = (pthread_t**)malloc(sizeof(pthread_t*)*t);
    thread_sync = (int*)malloc(sizeof(int)*t);
    memset(thread_sync,0,sizeof(int)*t);
    for (c=0; c<t; ++c) {
	thr[c] = (pthread_t*)malloc(sizeof(pthread_t));
	pthread_create(thr[c],&attr,
		       run_thread,(void*)c);
    }
    for (c=0; c<t; ++c) {
	while(thread_sync[c]==0){
	    sched_yield();
	}
    }
    gettimeofday(&start,NULL);
    tsync = 1;
    sched_yield();

    for (c=0; c<t; ++c) {
	    int *retval;
	    pthread_join(*thr[c],(void*)&retval);

	    res[0]+=retval[0];
	    res[1]+=retval[1];
	    res[2]+=retval[2];
	    res[3]+=retval[3];
    }
    if(start.tv_usec>end.tv_usec){
	end.tv_usec+=1000000;
	end.tv_sec-=1;
    }
    total = end.tv_usec-start.tv_usec + 1000000* (end.tv_sec-start.tv_sec);
    printf("Run time: %li.%06li %4.2f %4.2f\n",
	   end.tv_sec-start.tv_sec,
	   end.tv_usec-start.tv_usec,
	   ((double)(16*messages*t*(psize+36))/(double)(1000000))/(total/1000000),
	   ((double)(2*messages*t))/((double)total/1000000));


    res[0]/=(messages);
    res[1]/=(messages);
    res[2]/=(messages);
    res[3]/=(messages);
    total = res[0]+res[1]+res[2];


    if(timeouts>0){
	printf("Timeouts: %i\n",timeouts);
	printf("Drops   : %i\n",dropped);
	printf("Delays  : %i\n",delayed);
    }
    total_delay/=delay_count;
    printf("Average times: %i.%06i %i.%06i %i.%06i %i.%06i -> %i.%06i\n",
	   res[0] / 1000000,
	   res[0] % 1000000,
	   res[1] / 1000000,
	   res[1] % 1000000,
	   res[2] / 1000000,
	   res[2] % 1000000,
	   res[3] / 1000000,
	   res[3] % 1000000,
	   total / 1000000,
	   total % 1000000);
    printf("Sum squares: %i\n",squares);
    return 0;


}
