/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: send_file_socket_test.c,v 1.2 2005/04/27 03:57:21 okrieg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/
/* Test sending a file descriptor belonging to a file across a socket
   to another process file descriptors sent across a socket to another
   process. */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>

#define MSG1 "This is a message."
#define MSG2 "This is the message response"

const char *test_message = "A test message.\n";

int recv_fd(int from_fd)
{
    struct iovec iov[1];
    struct msghdr msg;
    char buf[1];
    char cmsgbuf[1000];
    struct cmsghdr *cmsg;
    int ret;

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    msg.msg_flags = MSG_WAITALL;

    ret = recvmsg(from_fd, &msg, 0);
    if (ret<0)
    {
	perror("recvmsg in child failed (recv_fd)\n");
	exit(1);
    }
    
    cmsg = CMSG_FIRSTHDR(&msg);
    return *((int*)CMSG_DATA(cmsg));
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
 
/*     printf("dest_fd: %i, sending %i\n", dest_fd, fd); */
/*     printf("controllen %i\n", msg.msg_controllen); */
    if ( (sent=sendmsg(dest_fd, &msg, 0)) <0)
    {
	perror("sendmsg (send_fd) failed\n");
	exit(1);
    }
/*     printf("sent %i bytes\n", sent); */
}


void do_child(int parent)
{
    int fd = recv_fd(parent);
    char buf[1000];

    struct iovec iov[1];
    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);

    struct msghdr msg;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    memset(buf, 0, sizeof(buf));
    if (recvmsg(parent, &msg, 0)<0)
    {
	perror("recvmsg in child failed");
	exit(1);
    }
	
    if (strcmp(buf, MSG1)!=0)
    {
	printf("Unexpected sync message from parent: %s\n", buf);
	exit(1);
    }

    /* Read data from file */
    if (lseek(fd, 0, SEEK_SET)!=0) 
    {
	printf("Seek failed\n");
	exit(1);
    }

    memset(buf, 0, sizeof(buf));
    if (read(fd, buf, strlen(test_message))!=strlen(test_message)) 
    {
	perror("Read from test file failed\n");
	exit(1);
    }

    if (strcmp(test_message, buf)!=0) 
    {
	printf("Test Message does not match");
	exit(1);
    }
    exit(0);
}

int main()
{
    int first_pair[2];
    int child;
    int status;
    char *tmpfile;
    int file_fd;
    char buf[1000];

    /* Create socket pair to talk to first child */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, first_pair)<0)
    {
	perror("socketpair failed\n");
	exit(1);
    }

    /* create first child */
    if ( (child = fork()) == -1 )
    {
	perror("Fork failed\n");
	exit(1);
    }
    else if (child == 0)
    {
	do_child(first_pair[1]);
	exit(0);
    }
    else
    {
	close(first_pair[1]);
    }

    tmpfile = tmpnam(NULL);
    file_fd = open(tmpfile, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);
    if (file_fd<0) 
    {
	perror("Could not open temporary file\n");
	exit(1);
    }
    
    /* Send file descriptor to child process */
    send_fd(first_pair[0], file_fd);

    /* Write data to test file */
    if (write(file_fd, test_message, strlen(test_message))
	!=strlen(test_message))
    {
	perror("Failed to write test message to file\n");
	exit(1);
    }
    fsync(file_fd);

    /* Send synchronisation message to child */
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

    if (sendmsg(first_pair[0], &msg, 0)<0)
    {
	perror("synchronisation message to child failed\n");
	exit(1);
    }

    wait(&status);
    if (!WIFEXITED(status) || WEXITSTATUS(status)!=0) {
	printf("Child exited with non zero exit status\n");
	exit(1);
    }

    printf("Test succeeded\n");
    exit(0);
}
