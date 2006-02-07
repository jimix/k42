/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: socket2.c,v 1.1 2004/04/09 20:07:47 aabauman Exp $
 *****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>

/* test program which shows up an error in K42's handling of connect()
 * for datagram sockets -- it doesn't remember the peer's address when you
 * later do a sendmsg()
 *
 * Simple use: ./socket2 localhost
 * Expected result: no error messages, garbage UDP packet sent to named host
 */

int
main(int argc, char *argv[])
{
	struct sockaddr_in sockaddr;
	struct hostent *hostent;
	struct msghdr msghdr;
	struct iovec iovecs[2];
	uint16_t port = 9; /* discard */
	uint64_t seqnum = 0x37;
	char buf[1000];
	int rc, fd;
	ssize_t len;

	if (argc != 2) {
		printf("Usage: %s <host>\n", argv[0]);
		return 1;
	}

	hostent = gethostbyname(argv[1]);
	if (hostent == NULL) {
		herror("Error: gethostbyname");
		return 1;
	}

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		perror("Error: socket");
		return 1;
	}

	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	sockaddr.sin_addr = *((struct in_addr *)hostent->h_addr);
	memset(&sockaddr.sin_zero, 0, sizeof(sockaddr.sin_zero));

	rc = connect(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	if (rc != 0) {
		perror("Error: connect");
		return 1;
	}

	memset(&msghdr, 0, sizeof(msghdr));
	msghdr.msg_iov = iovecs;
	msghdr.msg_iovlen = 2;

#if 0 /* this makes it work on K42 */
    	msghdr.msg_name = &sockaddr;
	msghdr.msg_namelen = sizeof(sockaddr);
#endif

	iovecs[0].iov_base = &seqnum;
	iovecs[0].iov_len = sizeof(seqnum);

	iovecs[1].iov_base = buf;
	iovecs[1].iov_len = sizeof(buf);

	len = sendmsg(fd, &msghdr, 0);
	if (len <= 0) {
		perror("Error: sendmsg");
		return 1;
	}

	return 0;
}
