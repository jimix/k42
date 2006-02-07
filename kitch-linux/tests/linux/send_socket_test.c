/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: send_socket_test.c,v 1.4 2005/04/28 02:25:28 okrieg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/
/***********************************************************************/
/* Testing file descriptors sent across a socket to another process
   Parent forks two children and then an extra pair of connected
   sockets The parent sends one end to the first child, the other to
   the second child. The test confirms that the children can talk
   to each other */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>

#define MSG1 "This is a message."
#define MSG2 "This is the message response"

int recv_fd(int from_fd)
{
    struct iovec iov[1];
    struct msghdr msg;
    char buf[1];
    char cmsgbuf[1000];
    struct cmsghdr *cmsg;
    int ret;
    int newfd;

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    msg.msg_flags = MSG_WAITALL;

    printf("recv_fd: About to recvmsg: %i\n", from_fd);
    ret = recvmsg(from_fd, &msg, 0);
    if (ret<0)
    {
	perror("recvmsg in child failed (recv_fd)\n");
	exit(1);
    }
    
    printf("Finished recvmsg: %i\n", from_fd);
    
    cmsg = CMSG_FIRSTHDR(&msg);
    newfd = *((int*)CMSG_DATA(cmsg));
    printf("recv_fd: New FD is %i\n", newfd);
    return newfd;
}

void send_fd(int dest_fd, int fd)
{
    struct iovec iov[0];
    struct cmsghdr *cmsg;
    struct msghdr msg;
    char buf[1];
    int sent;
    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);

    printf("sending fd %i\n", fd);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = malloc(CMSG_SPACE(sizeof(fd)));
    msg.msg_controllen = CMSG_SPACE(sizeof(fd));
    msg.msg_flags = 0;

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    *(int*)CMSG_DATA(cmsg) = fd;

    msg.msg_controllen = CMSG_SPACE(sizeof(fd));


    printf("Getting messages\n");
    cmsg = CMSG_FIRSTHDR(&msg);
    printf("msghdr: %p\n", &msg);
    while (cmsg) {
	printf("Got cmsg entry: %p\n", cmsg);
	cmsg = CMSG_NXTHDR(&msg, cmsg);
    }



/*     printf("dest_fd: %i, sending %i\n", dest_fd, fd); */
/*     printf("controllen %i\n", msg.msg_controllen); */
    printf("send_fd: About to sendmsg on %i, sending %i\n", dest_fd, fd);
    if ( (sent=sendmsg(dest_fd, &msg, 0)) <0)
    {
	perror("sendmsg (send_fd) failed\n");
	exit(1);
    }
    printf("send_fd: sendmsg finished\n");
/*     printf("sent %i bytes\n", sent); */
}


void child1(int parent)
{
    printf("child1: recv_fd %i\n", parent);
    int fd = recv_fd(parent);
    int fd2 = recv_fd(parent);		
    int fd3 = recv_fd(parent);
    int fd4 = recv_fd(parent);
    char buf[100];
    close(parent);

    /* TEST of getting stdout from parent, and writing*/
    printf("child1: received fds: %i %i %i %i\n", fd, fd2, fd3, fd4);

    close(2);

    printf("writing to stdout after close from child1\n");

    dup2(fd3, 2);
    
    fprintf(stderr, "writing to stderr from child1\n");

    write(fd3, "child1: yikes, I am writing to stdout from parent\n", 
	  strlen("child1: yikes, I am writing to stdout from parent\n")+1); 

    /* test of dup2 to our stdout, from parent, then send message  other child*/
    dup2(fd, 2);
    printf("child1: fprintf to child2 %s\n", "HELLo to child 2 test 1");
    fprintf(stderr, "HELLo to child 2 test 1");
    read(fd, buf, sizeof(buf));
    printf("child1: got %s\n", buf);
    printf("child1: fprintf to child2 %s\n", "HELLo to child 2 test 2");
    fprintf(stderr, "HELLo to child 2 test 2");
    read(fd, buf, sizeof(buf));
    printf("child1: got %s\n", buf);

    struct iovec iov[1];
    strcpy(buf, MSG1);
    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);

    struct msghdr msg;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    printf("child1: sending message to: %i\n", fd);
    if (sendmsg(fd, &msg, 0)<0)
    {
	perror("sendmsg from child1 failed\n");
	exit(1);
    }

    memset(buf, 0, sizeof(buf));
    printf("child1: receiving message: %i\n", fd);
    if (recvmsg(fd, &msg, 0)<0)
    {
	perror("recvmsg in child1 failed");
	exit(1);
    }
	
    if (strcmp(buf, MSG2)!=0)
    {
	printf("Unexpected message from child1: %s\n", buf);
	exit(1);
    }
    close(fd);
    close(2);
    printf("child1 is done!\n");
}

void child2(int parent)
{
    printf("child2: recv_fd %i\n", parent);
    int fd = recv_fd(parent);
    int fd2 = recv_fd(parent);
    int fd3 = recv_fd(parent);
    int fd4 = recv_fd(parent);
    char buf[100];
    close(parent);

    printf("child2: received fds: %i %i %i %i\n", fd, fd2, fd3, fd4);


    read(fd, buf, 100);
    printf("child2 got from child 1: %s\n", buf);
    write(fd, "ack", 4);
    read(fd, buf, 100);
    printf("child2 got from child 1: %s\n", buf);
    write(fd, "ack", 4);

    struct iovec iov[1];
    struct msghdr msg;

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    printf("child2: receiving message: %i\n", fd);
    if (recvmsg(fd, &msg, 0)<0)
    {
	perror("recvmsg in child2 failed");
	exit(1);
    }
	
    if (strcmp(buf, MSG1)!=0)
    {
	printf("Unexpected message from child1: %s\n", buf);
	exit(1);
    }

    strcpy(buf, MSG2);
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    printf("child2: sending message: %i\n", fd);
    if (sendmsg(fd, &msg, 0)<0)
    {
	perror("sendmsg in child2 failed");
	exit(1);
    }

    int i;
    i = read(fd, buf, 100);
    
    printf("child2 is done!, got back from read %d\n", i);
    exit(0);
}


int main()
{
    int first_pair[2], second_pair[2], third_pair[3];
    int first_child, second_child, recv_child;
    int i, status;

    /* Create socket pair to talk to first child */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, first_pair)<0)
    {
	perror("socketpair failed\n");
	exit(1);
    }

    /* create first child */
    if ( (first_child = fork()) == -1 )
    {
	perror("Fork failed\n");
	exit(1);
    }
    else if (first_child == 0)
    {
	close(first_pair[0]);
	child1(first_pair[1]);
	exit(0);
    }
    else
    {
	close(first_pair[1]);
    }

    /* Create socket pair to talk to second child */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, second_pair)<0)
    {
	perror("socketpair failed\n");
	exit(1);
    }

    /* create second child */
    if ( (second_child = fork()) == -1 )
    {
	perror("Fork failed\n");
	exit(1);
    }
    else if (second_child == 0)
    {
	close(second_pair[0]);
	child2(second_pair[1]);
	exit(0);
    }
    else
    {
	close(second_pair[1]);
    }

    /* Create socket pair for children to talk to each other with */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, third_pair)<0)
    {
	perror("socketpair failed\n");
	exit(1);
    }

    /* Send one half of pair to the first child */
    printf("Sending first fd\n");
    send_fd(first_pair[0], third_pair[0]);
    send_fd(first_pair[0], 0);
    send_fd(first_pair[0], 1);
    send_fd(first_pair[0], 2);

    /* Send other half of pair to the second child */
    printf("Sending second fd\n");
    send_fd(second_pair[0], third_pair[1]);
    send_fd(second_pair[0], 0);
    send_fd(second_pair[0], 1);
    send_fd(second_pair[0], 2);

    printf("Closing sockets\n");

    close(third_pair[0]);
    close(third_pair[1]);

    for (i=0; i<2; i++)
    {
	printf("Waiting for child\n");
	recv_child = wait(&status);
	if ( (recv_child!=first_child && recv_child!=second_child)
	     || !WIFEXITED(status) || WEXITSTATUS(status)!=0 )
	{
	    printf("Child %i failed\n", recv_child);
	    exit(1);
	} else if (recv_child == first_child) {
	    printf("parent: first child done\n");
	} else {
	    printf("parent: second child done\n");
	}
    }

    printf("Test succeeded\n");
    exit(0);
}
