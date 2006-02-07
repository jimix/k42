/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: socket.c,v 1.22 2004/10/01 00:51:42 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for linux personality
 * **************************************************************************/
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

static const char *prog;

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-dCslqrkno] [-p <port>]\n"
	    "  -d         daemonize, fork(2) after listen(2).\n"
	    "  -C         if daemonize then do not start client.\n"
	    "  -s         dump stat info of each socket.\n"
	    "  -l         loop forever (until given 'Q').\n"
	    "  -q         be quiet.\n"
	    "  -k         set SO_KEEPALIVE.\n"
	    "  -n         set TCP_NODELAY (disables Nagle algorithm).\n"
	    "  -o         Set all options (same as -rkn).\n"
	    "  -u         UDP test.\n"
	    "  -p <port>  listen on given port (default 4567).\n"
	    "\n", prog);
}

static int
dump_stat(int fd, const char *where)
{
    struct stat stat_buf;
    int ret;

    ret = fstat(fd, &stat_buf);

    if (ret == -1) {
        fprintf(stderr,"fstat: failed: %s\n",
                strerror(errno));
    }
    printf("%s:\n", where);
    printf ("\tst_dev\t\t%lu\n"
            "\tst_ino\t\t%lu\n"
            "\tst_mode\t\t\\%o\n"
            "\tst_nlink\t%u\n"
            "\tst_uid\t\t%u\n"
            "\tst_gid\t\t%u\n"
            "\tst_rdev\t\t%ld\n"
            "\tst_size\t\t%lu\n"
            "\tst_blksize\t%lu\n"
            "\tst_blocks\t%lu\n"
            "\tst_atime\t%s"
            "\tst_mtime\t%s"
            "\tst_ctime\t%s",
            (long)stat_buf.st_dev,
            stat_buf.st_ino,
            stat_buf.st_mode,
            (unsigned)stat_buf.st_nlink,
            stat_buf.st_uid,
            stat_buf.st_gid,
            (long)stat_buf.st_rdev,
            stat_buf.st_size,
            stat_buf.st_blksize,
            stat_buf.st_blocks,
            ctime(&stat_buf.st_atime),
            ctime(&stat_buf.st_mtime),
            ctime(&stat_buf.st_ctime));

    return ret;
}

int
udp_test()
{
    struct sockaddr_in saddr;
    struct sockaddr_in sock;
    socklen_t socklen = sizeof(sock);
    struct sockaddr_in peer;
    socklen_t peerlen = sizeof(peer);
    char buf[1024];
    char tstr[]="success";
    int s[2];
    int rc;
    int i;

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(0);
    if (inet_aton("127.1", &saddr.sin_addr) == 0) { /* LOOPBACK */
	fprintf(stderr,"inet_aton failed: %s\n", strerror(errno));
	return 1;
    }

    for (i = 0; i < 2; i++) {
        s[i] = socket(PF_INET, SOCK_DGRAM, 0);
        if (s[i] == -1) {
            fprintf(stderr, "socket(): failed: %s\n",
                    strerror(errno));
            return 1;
        }

	int one = 1;
	rc = setsockopt(s[i], SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (rc != 0) {
	    fprintf(stderr, "setsockopt(SO_REUSEADDR): %s\n", strerror(errno));
	    return 1;
	}

        rc = bind(s[i], (struct sockaddr *)&saddr, sizeof (saddr));
        if (rc == -1) {
            fprintf(stderr, "bind(): failed: %s\n",
                    strerror(errno));
            return 1;
        }
    }
    rc = getsockname(s[1], (struct sockaddr *)&sock, &socklen);
    if (rc == -1) {
        fprintf(stderr, "getsockname(): failed: %s\n",
                strerror(errno));
        return 1;
    }

    rc = sendto(s[0], tstr, sizeof (tstr), 0,
		(struct sockaddr *)&sock, socklen);
    if (rc == -1) {
        fprintf(stderr, "sendto(): failed: %s\n",
                strerror(errno));
        return 1;
    }

    rc = getpeername(s[0], (struct sockaddr *)&peer, &peerlen);
    /* should fail */
    if (rc != -1) {
	fprintf(stderr, "getpeername(): should have failed!\n");
	return 1;
    } else if (errno != ENOTCONN ) {
        fprintf(stderr, "getpeername(): failed: %s\n",
                strerror(errno));
        return 1;
    }

    rc = recv(s[1], buf, sizeof (buf), 0);
    if (rc == -1) {
        fprintf(stderr, "recv(): failed: %s\n",
                strerror(errno));
        return 1;
    }
    if (memcmp(tstr, buf, sizeof (tstr)) != 0) {
	fprintf(stderr, "recv(): got bad data\n");
	return 1;
    }

    /*
     * Test 2
     */

    rc = sendto(s[0], tstr, sizeof (tstr), 0,
		(struct sockaddr *)&sock, socklen);
    if (rc == -1) {
        fprintf(stderr, "sendto(): failed: %s\n",
                strerror(errno));
        return 1;
    }
    buf[0]='\0';
    peerlen = sizeof (peer);
    rc = recvfrom(s[1], buf, sizeof (buf), 0,
		  (struct sockaddr *)&peer, &peerlen);
    if (rc == -1) {
        fprintf(stderr, "recvfrom(): failed: %s\n",
                strerror(errno));
        return 1;
    }

    if (memcmp(tstr, buf, sizeof (tstr)) != 0) {
	fprintf(stderr, "recv(): got bad data\n");
	return 1;
    }
    rc = getsockname(s[0], (struct sockaddr *)&sock, &socklen);
    if (rc == -1) {
        fprintf(stderr, "getsockname(): failed: %s\n",
                strerror(errno));
        return 1;
    }

    /* check if they match */
    if (memcmp(&peer, &sock, peerlen) != 0) {
	fprintf(stderr, "peer and sock do not match\n");
	return 1;
    }
    return 0;
}


union sock_addr_u{
    struct sockaddr_in in;
    struct sockaddr_un un;
};

int
main(int argc, char *argv[])
{
    int rc;
    int c;
    int port = 0;
    int fd, wrfd, slen;
    union sock_addr_u saddr;

    const char *optlet = "f:uknoqlCdsp:";
    extern char *optarg;
    int daemonize = 0;
    pid_t pid;
    int do_stat = 0;
    int run_client_opt = 1;
    int run_client = 0;
    int loop = 0;
    int quiet = 0;
    int one = 1;
    int so_keepalive = 0;
    int tcp_nodelay = 0;
    prog = argv[0];
    int family = AF_INET;

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'f':
	    if (strncmp(optarg,"UNIX",4)==0) {
		family = AF_UNIX;
	    }
	    break;
	case 'k':
	    so_keepalive = 1;
	    break;
	case 'n':
	    tcp_nodelay = 1;
	    break;
	case 'o':
	    so_keepalive = 1;
	    tcp_nodelay = 1;
	    break;
	case 'q':
	   quiet = 1;
	    break;
	case 'd':
	    daemonize = 1;
	    break;
	case 'C':
	    run_client_opt = 0;
	    break;
	case 'l':
	    loop = 1;
	    break;
	case 's':
	    do_stat = 1;
	    break;
	case 'p':
	    port = strtol(optarg, (char **)NULL, 10);
	    break;
	case 'u':
	    family = AF_INET;
	    return udp_test();
	    break;
	case '?':
	default:
	    usage();
	    return (1);
	}
    }

    if (daemonize && run_client_opt) {
	run_client = 1;
    }

    if (port == 0) {
	port = 4567;
    }

    fd = socket(family, SOCK_STREAM, 0);
    if (fd == -1) {
	fprintf(stderr, "socket() failed for port %d: %s\n",
		port, strerror(errno));
	return 1;
    }

    if (do_stat) {
	dump_stat(fd, "socket()");
    }

    if (family == AF_UNIX) {
	saddr.un.sun_family = AF_UNIX;
    } else {
	saddr.in.sin_family = AF_INET;
	rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (rc != 0) {
	    fprintf(stderr, "setsockopt(SO_REUSEADDR): %s\n",
		    strerror(errno));
	    return 1;
	}

	if (so_keepalive) {
	    rc = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
	    if (rc != 0) {
		fprintf(stderr, "setsockopt(SO_KEEPALIVE): %s\n",
			strerror(errno));
		return 1;
	    }
	}
    }
    while (port <= UINT16_MAX) {
	int size;
	if (family == AF_UNIX) {
	    saddr.un.sun_path[0] = 0;
	    slen = size = sizeof(saddr.un.sun_family) + 1
		+ sprintf(&saddr.un.sun_path[1], "socket_%d", port);
	    printf("Unix addr: %d %s\n", slen, saddr.un.sun_path+1);
	} else {
	    slen = size = sizeof(saddr.in);
	    saddr.in.sin_port = htons(port);
	    /* localhost */
	    if (inet_aton("0", &saddr.in.sin_addr) == 0) {/* INADDR_ANY */
		perror("inet_aton failed");
		return 1;
	    }
	}

	rc = bind(fd, (struct sockaddr *)&saddr, slen);
	if (rc != 0) {
	    printf("error: %d\n",errno);
	    if (errno == EINVAL) {
		++port;
		continue;
	    }
	    perror("bind failed");
	    return 1;
	}
	break;
    }

    rc = listen(fd, 4);
    if (rc) {
	perror("listen failed");
	return 1;
    }

    if (daemonize) {
	pid = fork ();

	if (pid == -1) {
	    fprintf(stderr, "%s: fork() failed: %s\n",
		    prog, strerror(errno));
	    return (1);
	} else if (pid != 0) {
	    printf("%s: daemon ready to accept connection on port: %u\n",
		   prog, port);
	    if (run_client == 1) {
		char sport[5];
		snprintf(sport, sizeof (sport), "%u", port);
		printf("%s: Communicate with echo daemon.\n\n", prog);
		if (family==AF_INET) {
		    execl("/tests/linux/forktest", "forktest", "-c",
			  sport, NULL);
		} else {
		    execl("/tests/linux/forktest", "forktest", "-U",
			  saddr.un.sun_path+1, NULL);
		}
	    } else {
		return (0);
	    }
	}
    } else {
	    printf("%s: ready to accept connection on port: %u\n",
		   prog, port);
    }

again:
    wrfd = accept(fd, (struct sockaddr *)&saddr, &slen);
    if (wrfd == -1) {
	fprintf(stderr, "accept failed: %s.\n",
		strerror(errno));
	return 1;
    }

    if (family==AF_INET && tcp_nodelay) {
	/* disable nagle */
	rc = setsockopt(wrfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	if (rc != 0) {
	    fprintf(stderr, "setsockopt(TCP_NODELAY): %s\n", strerror(errno));
	    return 1;
	}
    }


    if (!quiet) {
	printf("accept success\n");
    }

    if (do_stat) {
	dump_stat(wrfd, "accept()");
    }

    for (;;) {
	char buf[256];
	unsigned long len;
	char *pp = "\ntype: ";

	len = strlen(pp);
	write(wrfd, pp , len);

	len = read(wrfd, buf, 256);
	if (len == -1) {
	    fprintf(stderr, "read error: %s\n",
		    strerror(errno));
	    return -1;
	}
	if (len == 0) {
	    fprintf(stderr, "%s: EOF on socket.  Quitting.\n", argv[0]);
	    break;
	}

	write(wrfd, buf, len);
	if ((len >= 2) && (buf[len-1] == '\n')) {
	    if (buf[len-2] == 'q') {
		break;
	    } else if (buf[len-2] == 'Q') {
		loop = 0;
		break;
	    }
	}
    }

    if (loop) {
	close(wrfd);
	goto again;
    }

    printf("socket server quitting\n");

    return 0;
}
