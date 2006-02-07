/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simple_sendrecvmesg.c,v 1.1 2004/11/15 04:21:22 cyeoh Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>

#define MSG1 "This is a message."
#define MSG2 "This is the message response"



int main()
{
    int sockets[2], child;
    char buf[100];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets)<0)
    {
	perror("socketpair failed\n");
	exit(1);
    }

    if ( (child = fork()) == -1 )
    {
	perror("Fork failed\n");
	exit(1);
    }
    else if (child == 0)
    {
	/* Child */
	int dupfd;
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

	// Check dup works
	dupfd = dup(sockets[0]);

	if (sendmsg(dupfd, &msg, 0)<0)
	{
	    perror("sendmsg from child failed\n");
	    exit(1);
	}

	memset(buf, 0, sizeof(buf));
	if (recvmsg(sockets[0], &msg, 0)<0)
	{
	    perror("recvmsg in parent failed");
	    exit(1);
	}
	
	if (strcmp(buf, MSG2)!=0)
	{
	    printf("Unexpected message from parent: %s\n", buf);
	    exit(1);
	}
	
    }
    else
    {
	/* Parent */
	struct iovec iov[1];
	struct msghdr msg;
	int pid, status;

	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	if (recvmsg(sockets[1], &msg, 0)<0)
	{
	    perror("recvmsg in parent failed");
	    exit(1);
	}
	
	if (strcmp(buf, MSG1)!=0)
	{
	    printf("Unexpected message from client: %s\n", buf);
	    exit(1);
	}

	strcpy(buf, MSG2);
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	if (sendmsg(sockets[1], &msg, 0)<0)
	{
	    perror("sendmsg in parent failed");
	    exit(1);
	}


	pid = wait(&status);

	if (pid != child || !WIFEXITED(status) || WEXITSTATUS(status)!=0 )
	{
	    printf("Child did not succeed\n");
	    exit(1);
	}
	else
	{
	    printf("simple_sendrecvmesg test succedeed\n");
	    exit(0);
	}
    }



    exit(0);
}
