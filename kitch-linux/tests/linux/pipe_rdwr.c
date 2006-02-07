/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: pipe_rdwr.c,v 1.5 2002/12/31 19:30:17 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Check to see if blocking on pipe writes is performed correctly.
 ****************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#include <sys/poll.h>
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
	    fprintf(stderr, "Error on syscall: " #args " -> %i %s\n",	\
			errno, strerror(errno));			\
	    err_action;							\
	}								\
	ret;								\
})


void*
writer_thread(void* p)
{
    int buf[512];
    int *fds=(int*)p;
    int ret;
    int i;
    int count=0;
    while (count<100) {
	for (i=0; i<512 ; ++i) {
	    buf[i]=count;
	}
	ret = write(fds[1],buf,2048);
	++count;
    }

    return NULL;

}

/*
 * Write 100 2048-byte blocks of data into a pipe.
 * Check to make sure they all come out the same way they were put in.
 */
int
main(int argc, char** argv)
{
    int ret=0;

    int fds[2];
    int buf[512];
    int i,j;
    pthread_t thr;

    SCALL(pipe(fds),return -1);
    pthread_create(&thr,NULL,writer_thread,fds);
    for (i=0; i<100; ++i) {
	ret = read(fds[0],buf,2048);
	if (ret!=2048) {
	    fprintf(stderr,"Failed pipe read: %i %i\n",ret,errno);
	    return -1;
	}
	for (j=0; j<512 ; ++j) {
	    if ( buf[j]!=i ) {
		fprintf(stderr,"Failed: got: %i expected: %i at %i\n",buf[j],i,j);
		return -1;
	    }
	}
    }


    SCALL(pipe(fds),return -1);

    ret = write( fds[1], buf, 2048);
    if (ret != 2048) {
	fprintf(stderr,"Should write 2048, got: %i\n",ret);
	return -1;
    }

    close(fds[1]);


    ret = read( fds[0], buf, 1024);

    if (ret != 1024) {
	fprintf(stderr,"Should read 1024, got: %i\n",ret);
	return -1;
    }

    ret = read( fds[0], buf, 2048);

    if (ret != 1024) {
	fprintf(stderr,"Should read 1024, got: %i\n",ret);
	return -1;
    }

    ret = read( fds[0], buf, 1024);

    if (ret != 0) {
	fprintf(stderr,"Should read 0, got: %i\n",ret);
	return -1;
    }
    close(fds[0]);


    return 0;
}
