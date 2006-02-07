/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: marc.c,v 1.19 2004/07/22 20:30:26 marc Exp $
 *****************************************************************************/

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>

#define MAP_LARGEPAGE 0x1000000
#define PAGE_SIZE 0x1000
#define LARGE_PAGE_SIZE 0x1000000
#define MAPPING_SIZE 0x3000000

int
main(int argc, char* argv[])
{
    void *p;
    unsigned int size;
    int i;
    size = 1024*32;;
    for (i=0; i < 6; i++) {
	printf("%x ", size);
	p = malloc(size-16);
	printf("%p\n", p);
    }
    
    size = 1024;
    do {
	printf("%x ", size);
	p = malloc(size-16);
	printf("%p\n", p);
	size *= 2;
    } while ((size != 0) && (p != 0));
    return 0;
}

#if 0
int
main(int argc, char* argv[])
{
    void* first;
    unsigned long *p;
    pid_t child;
    int status;
    first = mmap(
	0, MAPPING_SIZE, PROT_READ|PROT_WRITE,
	MAP_PRIVATE|MAP_ANONYMOUS|MAP_LARGEPAGE, 0, LARGE_PAGE_SIZE);
    printf("%lx\n", (unsigned long)first);
    p = (unsigned long*)first;
    for(;p<(unsigned long*)(first+MAPPING_SIZE);) {
	*p = (unsigned long)p;
	p += PAGE_SIZE/sizeof(*p);
    }
    
    if ((child=fork())) {
	wait(&status);
    }
    p = (unsigned long*)first;
    for(;p<(unsigned long*)(first+MAPPING_SIZE);) {
	if(*p != (unsigned long)p) {
	    printf("mismatch %p %lx in %s\n", p, *p,
		   child?"child":"parent");
	    exit(1);
	}
	p += PAGE_SIZE/sizeof(*p);
    }
    return 0;
}

int
getprivate()
{
    int q;
    q = msgget(IPC_PRIVATE,0666);
    printf("msqid %d\n", q);
    return q;
}



void
doit(int dummy)
{
    return;
}

int
main(int argc, char* argv[])
{
    int q, rc, i;
    struct msgbuf {long mtype; char mtext[120];} mbuf;
    struct msgbuf *msgbufp = &mbuf;
    
    if (argc == 1) goto tell;

    if (*argv[1] == 'r') {
	q = atoi(argv[2]);
	rc = msgctl(q, IPC_RMID, 0);
	if (rc) perror("msgctl");
	return 0;
    }
    if (*argv[1] == 'p') {
	q = getprivate();
	if(q < 0) return 1;
	return 0;
    }
    if (*argv[1] == 't') {
	if((q=getprivate())<0) return 1;
	msgbufp->mtype = 1;
	if (fork() ){
	    sleep(1);			// force initial child block
	    /*parent*/
	    for(i=0;i<10;i++) {
		sprintf(msgbufp->mtext, "%d\n", i);
		rc = msgsnd(q, msgbufp, 120, 0);
		if(rc != 0) {
		    perror("msgsnd");
		    exit(1);
		}
	    }
	    // hang on type 2
	    rc = msgrcv(q, msgbufp, 120, 2, 0);
	    printf("parent done\n");
	    exit(0);
	} else {
	    /*child*/
	    for(i=0;i<10;i++) {
		rc = msgrcv(q, msgbufp, 120, 0, 0);
		if(rc < 0) {
		    perror("msgrcv");
		    break;
		}
		printf("msgrcv %d %d %s", i, rc, msgbufp->mtext); 
		if(i==0) sleep(1);	// allow parent to queue rest
	    }	    
	    rc = msgctl(q, IPC_RMID, 0);
	    if (rc) perror("msgctl");
	    printf("child done\n");
	    exit(0);
	}
    }
  tell:
    printf("r rmid\np make private\n");
    return 0;

    void* first;
    first = mmap(
	0, 0x10000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    printf("%lx\n", (unsigned long)first);
    first = mmap(
	0, 0x10000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
    printf("%lx\n", (unsigned long)first);
    return 0;
    int status;
    if(fork()) {
	wait(&status);
	printf("p status %x\n",status);
	return 0;
    }
    if(fork()) {
	wait(&status);
	printf("c status %x\n",status);
	return 0;
    }
    return 0;

    void *first, *second;
    first = mmap(0, 4096, PROT_WRITE, MAP_ANONYMOUS, 0, 0);
    second = mmap(first, 4096, PROT_WRITE, MAP_ANONYMOUS, 0, 0);
    printf("%lx %lx\n", (unsigned long)first, (unsigned long)second);
    return 0;
    if((child=fork())) {
	//parent
	printf("parent waiting\n");
	wait(0);
	printf("parent saw child death\n");
    } else {
	execl("/bin/grep", "grep", "marc", "/etc/passwd", 0);
	perror("exec failed");
    }
    return 0;
    if((child=fork())) {
	//parent
	printf("parent sleeping\n");
	sleep(2);
	printf("parent killing\n");
	kill(child, SIGUSR1);
	wait(0);
	printf("parent saw child death\n");
    } else {
	signal(SIGUSR1, doit);
	pause();
	printf("child after pause\n");
    }
    return 0;
}


int
main(int argc, char* argv[])
{
    pid_t me, mep;
    me = getpid();
    mep = getpgrp();
    printf("%d %d\n", me, mep);
    kill(-me, SIGKILL);
    while(1) {
	printf("killed\n");
    }
}

int
main(int argc, char* argv[])
{
    unsigned long b[2], *p;
    p = (unsigned long*)(((unsigned long)b)+2);
    printf("%p %p\n", b, p);
    b[0]=b[1]=0;
    *p = 0xf0f1f2f3f4f5f6f7;
    printf("%lx %lx\n", b[0], b[1]);
    return 0;
}
    

int
main(int argc, char* argv[])
{
    pid_t parent, child;
    parent = getpid();
    child = fork();
    if (child) {
	printf("in parent %d %d\n", parent, child);
	sleep(1);
    } else {
	parent = getppid();
	child = getpid();
	printf("in child %d %d\n", parent, child);
	sleep(2);
	parent = getppid();
	child = getpid();
	printf("in child %d %d\n", parent, child);
    }
    return 0;
}

int
main(int argc, char* argv[])
{
    int rc, i;
    int sv[2];
    char buf[128];

    if (argc == 1) goto tell;

    if (*argv[1] == 't') {
	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
	printf("%d %d %d\n", rc, sv[0], sv[1]);
	if (fork() ){
	    /*parent*/
	    close(sv[1]);
	    sleep(1);
	    for(i=0;i<10;i++) {
		sprintf(buf, "%d\n", i);
		rc = send(sv[0], (void*)buf, 120, 0);
		if(rc < 0) {
		    perror("parent send");
		    exit(1);
		}
		if (rc != 120) printf("send rc %d\n", rc);
	    }
	    rc = recv(sv[0], (void*)buf , 120, MSG_NOSIGNAL);
	    if(rc != 0) {
		perror("parent recv");
	    }
	    printf("parent done\n");
	    exit(0);
	} else {
	    /*child*/
	    close(sv[0]);
	    for(i=0;i<10;i++) {
		rc = recv(sv[1], (void*)buf, 120, 0);
		if(rc < 0) {
		    perror("recv");
		    break;
		}
		printf("recv %d %d %s", i, rc, buf); 
		if(i==0) sleep(1);
	    }	    
	    printf("child done\n");
	    exit(0);
	}
    }

  tell:
    printf("%s t\n", argv[0]);
    return 1;
}

#endif
