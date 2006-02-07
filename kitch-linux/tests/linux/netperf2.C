/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: netperf2.C,v 1.16 2004/06/28 17:01:16 rosnbrg Exp $
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
#include <netinet/tcp.h>
#include <pthread.h>
#include <strings.h>

static char *def_host = "9.2.208.162";
static volatile int messages=1000;
static int delay=200000;
static volatile int tsync = 0;
static volatile int tterm = 0;	// thread termination flag
static int concur = 1;
static int psize = 256;
static char *hosts[16];
static int numHosts = 0;
static int duration = 0;
static int yield_interval = 0;


struct Time: public timeval{
    Time(int sec=0,int usec=0) {tv_sec = sec; tv_usec=usec;};
    Time(const struct timeval &tv) {
	tv_sec = tv.tv_sec;
	tv_usec = tv.tv_usec;
    };

    Time(const Time &t) { tv_sec = t.tv_sec; tv_usec = t.tv_usec; };

    Time operator+(const struct timeval &tv) const{
	Time result;
	result.tv_sec = tv_sec + tv.tv_sec;
	result.tv_usec = tv_usec + tv.tv_usec;
	if (result.tv_usec>1000000) {
	    ++result.tv_sec;
	    result.tv_usec -=1000000;
	}
//	printf("+ Result is: %li.%06li\n",result.tv_sec,result.tv_usec);
	return result;
    }
    Time operator-(const struct timeval &tv) const{
	Time result;
	result.tv_sec = tv_sec - tv.tv_sec;
	result.tv_usec = tv_usec - tv.tv_usec;
	if (result.tv_usec<0) {
	    --result.tv_sec;
	    result.tv_usec +=1000000;
	}
//	printf("+ Result is: %li.%06li\n",result.tv_sec,result.tv_usec);
	return result;
    }
    Time operator/(const int d) const{
	Time result = *this;
	result.tv_usec+=(result.tv_sec % d)*1000000;
	result.tv_sec /= d;
	result.tv_usec /= d;
	if (result.tv_usec>1000000) {
	    result.tv_usec -=1000000;
	    result.tv_sec += 1;
	}
//	printf("/ Result is: %li.%06li\n",result.tv_sec,result.tv_usec);
	return result;
    }

    Time operator/=(const int d) {
	Time &result = *this;
	result.tv_usec+=(result.tv_sec % d)*1000000;
	result.tv_sec /= d;
	result.tv_usec /= d;
	if (result.tv_usec>1000000) {
	    result.tv_usec -=1000000;
	    result.tv_sec += 1;
	}
//	printf("/ Result is: %li.%06li\n",result.tv_sec,result.tv_usec);
	return result;
    }
};

typedef struct {
    int proc;
    char* host;
    int port;
    int retval;
    volatile int *sync;
    volatile int *term;
    volatile int state;
    unsigned int size;
    int count;
    int sent;
    unsigned long long outstanding;
    Time total;
    Time endtime;
    int option;
    unsigned long long bytes_sent;
    unsigned long long bytes_received;
    int yield_interval;
} arg_t;
#define TCPNODELAY 1

static arg_t *args=NULL;
volatile static int debug = 0;

typedef struct {
    struct timeval t;
    int seq;
    char buf[0];
} message_t;

#define ERR_CHECK(CALL, ARGS...)					\
	if (CALL(ARGS)<0) {	 					\
		perror(#CALL " failed");				\
		args->retval = -1;					\
		return args;						\
	}

void* runTcpTest(void* argp)
{
    arg_t* args = (arg_t*)argp;
    struct sockaddr_in sin;
    message_t *msg;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
	printf("socket() failed\n");
	perror("error was");
	args->retval = -1;
	return args;
    }

    if (args->option&TCPNODELAY) {
	int val =1;

	ERR_CHECK(setsockopt,fd, IPPROTO_TCP,
		  TCP_NODELAY,(char *)&val,sizeof(int));
    }
    if (inet_aton(args->host, &sin.sin_addr) == 0) {
	perror("inet_aton failed");
	args->retval = -1;
	return args;
    }
    sin.sin_family=AF_INET;
    sin.sin_port = htons(args->port);

    ERR_CHECK(connect, fd, (struct sockaddr*)&sin, sizeof(sin));

    if (args->size<sizeof(message_t)) {
	args->size = sizeof(message_t);
    }

    msg = (message_t*)malloc(args->size);

    ERR_CHECK(fcntl,fd,F_SETFL,O_NONBLOCK);

    args->state = 1;
    while (!(*args->sync)) {
	sched_yield();
    }
    int msgs = args->count;
    Time end;
    unsigned mustRead = args->size;
    unsigned mustWrite = args->size;
    long sent = 0;
    long received =0;

    args->bytes_sent = 0;
    args->bytes_received = 0;

    int loop_count = 0;
    while ((msgs > 0) && !(*args->term)) {
	int rc;
	int selrc;
	struct timeval tv = {1,0};
	fd_set rset;
	fd_set wset;

	FD_ZERO(&rset);
	FD_SET(fd,&rset);
	FD_ZERO(&wset);
	FD_SET(fd,&wset);

	selrc = select(fd+1, &rset, &wset, NULL,&tv);

	if (tv.tv_sec==0 && tv.tv_usec==0) {
	    printf("Timeout: %d\n", args->sent);
	}

	if (selrc >0 && FD_ISSET(fd, &rset)) {
	    rc=read(fd,(((char*)msg) + args->size - mustRead),
		    mustRead);


	    if (rc<0) {
		if (errno==EWOULDBLOCK) {
		    printf("Read EWOULDBLOCK!!!! %d\n",selrc);
		    break;
		}
		printf("Read error: %d\n",errno);
		args->retval = -1;
		return args;
	    }
	    args->bytes_received += rc;
	    mustRead -=rc;
	    if (mustRead==0) {
		gettimeofday(&end,NULL);
		args->total = args->total + end - msg->t;
		--msgs;
		mustRead = args->size;
		received += args->size;
	    }
	}

	if (selrc>0 && FD_ISSET(fd, &wset)) {
	    while (1) {
		if (mustWrite==0) {
		    mustWrite = args->size;
		}
		if (mustWrite == args->size) {
		    msg->seq = args->sent++;
		    gettimeofday(&msg->t,NULL);
		}

		rc = write(fd, msg ,mustWrite);

		if (rc>0) {
		    args->bytes_sent += rc;
		    sent += rc;
		    mustWrite -= rc;
		} else {
		    break;
		}
	    };
	    if (rc<0 && errno !=EWOULDBLOCK && errno!=0) {
		printf("Write error: %d %d\n",rc,errno);
	    }
	    args->outstanding += (sent - received);

	}

	if (args->yield_interval > 0) {
	    if ((loop_count++ % args->yield_interval) == 0) {
		sched_yield();
	    }
	}

    }
    args->endtime = end;
    args->retval = 0;
    return args;
}


static void
usage() {
    fprintf(stderr,"Usage: netperf [-h <host>] [-p <port>]"
	    " [-d <delay>] [-c <count>] [-t <threads>] [ -C <#VP>]\n");
    fputs(	"    -h <host>    Host to send packets to."
		" (Must be in number/dots format.)(9.2.208.157)\n"
		"    -p <port>    Port to send packets to. (5123)\n"
		"    -c <count>   Number of packets to send per thread.\n"
		"    -s <bytes>   Bytes per packet.(256)\n"
		"    -n           No delay (TCP option)\n"
		"    -t <threads> Number of threads. (1)\n"
		"    -a		  Use application thread scope\n"
		"    -C <#VP>	  Number of VP's to use. (1)\n"
		"    -d <delay>   Timeout to retransmit (useconds).(200000)\n"
		"    -S <seconds> Test duration (overrides -c).\n"
		"    -y <count>   Yield interval in packets.\n",
		stderr);
}

extern "C" int netperf2main(int argc, char *argv[]);
extern "C" void breakpoint();
int
main(int argc, char *argv[])
{
    long c;
    int port = 0;
    const char *optlet = "ah:c:t:d:p:C:s:nbS:y:";
    extern char *optarg;
    int t=1;    int option = 0;
    double total=0;
    debug = 0;
    Time latency;
    pthread_t **thr;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM);

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
#ifdef K42
	    case 'b':
		breakpoint();
		break;
#endif /* #ifdef K42 */
	    case 'a':
		pthread_attr_setscope(&attr,PTHREAD_SCOPE_PROCESS);
		break;
	    case 'h':
		hosts[numHosts] = optarg;
		numHosts++;
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
	    case 'n':
		option|=TCPNODELAY;
		break;
	    case 'S':
		duration = strtol(optarg,(char**)NULL,10);
		messages = 1000000000; // duration determines when test ends
		break;
	    case 'y':
		yield_interval = strtol(optarg,(char**)NULL,10);
		break;
	    default:
		usage();
		return 1;
	}
    }

    if (numHosts == 0) {
	hosts[0] = def_host;
	numHosts = 1;
    }
    thr = (pthread_t**)malloc(sizeof(pthread_t*)*t);
    args =(arg_t*)malloc(sizeof(arg_t)*t);
    memset(args,0,sizeof(arg_t)*t);
    for (c=0; c<t; ++c) {
	args[c].proc = c % concur;
	args[c].sync = &tsync;
	args[c].term = &tterm;
	args[c].count = messages;
	args[c].size = psize;
	args[c].port = port;
	args[c].host = hosts[c%numHosts];
	args[c].option = option;
	args[c].yield_interval = yield_interval;
	thr[c] = (pthread_t*)malloc(sizeof(pthread_t));
	pthread_create(thr[c],&attr,
		       runTcpTest,(void*)&args[c]);
    }
    for (c=0; c<t; ++c) {
	while (args[c].state==0) {
	    sched_yield();
	}
    }
    tterm = 0;
    tsync = 1;
    Time start;
    gettimeofday(&start,NULL);
    sched_yield();

    if (duration > 0) {
	sleep(duration);
	tterm = 1;
    }

    for (c=0; c<t; ++c) {
	    int *retval;
	    pthread_join(*thr[c],(void**)&retval);
    }

    Time end;
    gettimeofday(&end,NULL);

    end = end - start;
    total = end.tv_usec + 1000000 * end.tv_sec;
    unsigned long long bytes_sent = 0;
    unsigned long long bytes_received = 0;
    for (c=0; c<t; ++c) {
	bytes_sent += args[c].bytes_sent;
	bytes_received += args[c].bytes_received;
    }
    unsigned long long bytes = bytes_sent + bytes_received;
    total = bytes/total; //(=b/us)

    double MBs = total * 0.95367431640625; //convert to MB/s
    double Mbps = total * 8;
    double pps = 1024*1024 * Mbps / psize;
    printf("Run time: %li.%06li %.0f MB/s %.0f Mbps %.0f msgs/s\n",
	   end.tv_sec, end.tv_usec,MBs,Mbps,pps);

    for (c=0; c<t; ++c) {
	Time t = args[c].endtime - start;
	unsigned long long msgs =
	    ((args[c].bytes_sent+args[c].bytes_received) / psize);
	args[c].total /= msgs;
	printf("Thrd %ld: time %ld.%06ld, sent %lldK, rcvd %lldK, "
	       "latency %ld.%06ld, window %lld\n",
	       c, t.tv_sec, t.tv_usec,
	       args[c].bytes_sent/1024, args[c].bytes_received/1024,
	       args[c].total.tv_sec, args[c].total.tv_usec,
	       args[c].outstanding/msgs);
	latency = args[c].total + latency;
    }
    latency/=t;
    printf("Latency avg: %li.%06li\n",
	   latency.tv_sec, latency.tv_usec);

    return 0;


}
