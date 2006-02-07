/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: pipe_select.c,v 1.9 2003/12/12 21:35:26 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Performs a series of tests to check behavior of select when blocked on
 * pipe file descriptors that are closed out from underneath it.
 ****************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>


#include <sys/types.h>
#include <sys/socket.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <strings.h>

// Wrap-around and check errors on system calls.
#define SCALL(args,err_action)						\
({									\
	int ret = args ;						\
	if ( ret < 0 ) {							\
	    printf("Error on syscall: " #args " -> %i %s\n",errno,	\
			strerror(errno));				\
	    err_action;							\
	}								\
	ret;								\
})

static volatile int sync_thread = 0;

void*
run_close_thread(void* p)
{
    int* fds = (int*)p;
    int ret;
    while (sync_thread==0);
    sleep(1);
    ret=close(*fds);
    return NULL;

}

// Close the "write" pipe while select-waiting on the "read" pipe.
// Will flag the "read" pipe as being readable.
int
run_close_test1()
{

    pthread_t thr;
    int fds[2];
    fd_set set1,set3;
    int ret;
    struct timeval tv;

    SCALL(pipe(fds),return -1);

    FD_ZERO(&set1);
    FD_SET(fds[0],&set1);

    FD_ZERO(&set3);
    FD_SET(fds[0],&set3);

    tv.tv_sec=20;
    tv.tv_usec=0;

    sync_thread = 0;

    pthread_create(&thr,NULL,run_close_thread,&fds[1]);

    sync_thread = 1;

    ret = select(fds[0]+1,&set1,NULL,&set3,&tv);

    if (FD_ISSET(fds[0],&set1) && !FD_ISSET(fds[0],&set3)) {
	return 0;
    }
    fprintf(stderr,"Failed test of close of write pipe\n");
    fprintf(stderr,"File descriptor active: r:%i w:? x:%i\n",
	    FD_ISSET(fds[0],&set1),
	    FD_ISSET(fds[0],&set3));

    return -2;
}


int
run_close_test2()
{
    pthread_t thr;
    int fds[2];
    fd_set set1,set3;
    int ret;
    struct timeval tv;

    SCALL(pipe(fds),return -1);

    FD_ZERO(&set1);
    FD_SET(fds[0],&set1);

    FD_ZERO(&set3);
    FD_SET(fds[0],&set3);

    tv.tv_sec=20;
    tv.tv_usec=0;

    sync_thread = 0;

    pthread_create(&thr,NULL,run_close_thread,&fds[0]);

    sync_thread = 1;

    ret = select(fds[0]+1,&set1,NULL,&set3,&tv);

    if ( ret<0 && errno==EINVAL )
	return 0;

    fprintf(stderr,"Failed test of close of read pipe\n");
    fprintf(stderr,"Select returned: %i %i\n",ret,errno);
    return -2;
}

#if 0
int
run_close_test3()
{
    pthread_t thr;
    int sock;
    fd_set set1,set3;
    int ret;
    struct timeval tv;

    sock=SCALL(socket(AF_INET,SOCK_STREAM,0),return -1);

    sockaddr_in sin = { AF_INET, 0 ,INADDR_ANY};
    SCALL( bind(sock,(sockaddr*)&sin,sizeof(sockaddr_in)) ,return -1);
    SCALL( listen(sock,10), return -1);


    FD_ZERO(&set1);
    FD_SET(sock,&set1);

    FD_ZERO(&set3);
    FD_SET(sock,&set3);

    tv.tv_sec=20;
    tv.tv_usec=0;

    sync_thread = 0;

    pthread_create(&thr,NULL,run_close_thread,&sock);

    sync_thread = 1;

    ret = select(sock+1,&set1,NULL,&set3,&tv);

    if ( ret == 0 )
	return 0;

    fprintf(stderr,"Failed test of close of local socket\n");
    fprintf(stderr,"File descriptor active: r:%i w:? x:%i\n",
	    FD_ISSET(sock,&set1),
	    FD_ISSET(sock,&set3));

    return -2;
}
#endif /* #if 0 */

// Close the "read" pipe while select-waiting on the "read" pipe.
// Select will time out.
int
run_close_test4()
{
    pthread_t thr;
    int fds[2];
    fd_set set1,set3;
    int ret;
    struct timeval tv;

    SCALL(pipe(fds),return -1);

    FD_ZERO(&set1);
    FD_SET(fds[0],&set1);

    FD_ZERO(&set3);
    FD_SET(fds[0],&set3);

    tv.tv_sec=2;
    tv.tv_usec=0;

    sync_thread = 0;

    pthread_create(&thr,NULL,run_close_thread,&fds[0]);

    sync_thread = 1;

    ret = select(fds[0]+1,&set1,NULL,&set3,&tv);

    if (ret<0 && errno==EINVAL) {
	return 0;
    }
    fprintf(stderr,"Failed close read pipe test: r:%i w:? x:%i\n",
	    FD_ISSET(fds[0],&set1),
	    FD_ISSET(fds[0],&set3));

    return -2;
}


// Set of tests to verify semantics of select when interacting with
// pipes.

int
main(int argc, char** argv)
{
    int ret=0;

    ret |= run_close_test1();
    ret |= run_close_test2();
    ret |= run_close_test4();

    return ret;
}
