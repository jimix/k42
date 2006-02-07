/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: runprog.c,v 1.14 2004/10/01 00:51:42 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: run linux-hello using fork_exec() call
 * **************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ptrace.h>

static const char *prog;


#define STRINGME(s) # s
#define HOSTTYPE(x) "HOSTTYPE=" STRINGME(x)

extern int __k42_linux_spawn(const char *,
			     char *const [], char *const [],
			     int);

static void
usage(void)
{
    fprintf(stderr, "Usage:\n"
	    "  %s [-sdDU] [-p <port>] [-[i]f file] [-u <uid>] "
	    "<program> [[prog arg1] [prog arg2] .. ]\n"
	    "    -s        use k42 spawn instead of execvp(3)\n"
	    "    -d        dup stress test\n"
	    "    -p <port> use socket <port> stdin, stdout and stderr \n"
	    "    -f <file> use file for stdin, stdout, stderr\n"
	    "    -i        use file in -f for stdin ONLY\n"
	    "    -u <uid>  run program under specified <uid> (must be root) \n"
	    "    -D        turn on system debugging for children\n"
	    "    -U        turn on user-level debugging for children\n"
	    "\n", prog);
}


static int
dupsock(int port)
{
    int fd;
    int sfd;
    struct sockaddr_in saddr;
    int slen;


    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	return fd;
    }
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);

    int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0 ) {
	fprintf(stderr, "setsockopt(SO_REUSEADDR): %s\n", strerror(errno));
	return -1;
    }

    /* localhost */
    if (inet_aton("0", &saddr.sin_addr) == 0) {		/* INADDR_ANY */
	perror("inet_aton() failed");
	return -1;
    }

    if (bind(fd, (struct sockaddr *)&saddr, sizeof saddr) != 0) {
	perror("bind failed");
	return -1;
    }

    if (listen(fd, 4) == -1) {
	perror("listen failed");
	return -1;
    }

    printf ("connect to port %d\n", port);

    if ((sfd = accept(fd, (struct sockaddr *)&saddr, &slen)) == -1) {
	perror("accept failed");
	return -1;
    }

    if (dup2(sfd, 0) == -1) {
	fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
		prog, sfd, 0, strerror(errno));
	return -1;
    }
    if (dup2(sfd, 0) == -1) {
	fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
		prog, sfd, 0, strerror(errno));
	return -1;
    }
    if (dup2(sfd, 0) == -1) {
	fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
		prog, sfd, 0, strerror(errno));
	return -1;
    }

    close (sfd);

    return sfd;
}

static int
runprog(int spawn, int dup_test, int uid, char *argv[])
{
    int ret;

    if (dup_test) {
	if (dup2(0, 100) == -1) {
	    fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
		    prog, 0, 100, strerror(errno));
	    return -1;
	}
	if (dup2(1, 1000) == -1) {
	    fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
		    prog, 1, 1000, strerror(errno));
	    return -1;
	}
	if (dup2(2, 1999) == -1) {
	    fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
		    prog, 2, 1999, strerror(errno));
	    return -1;
	}
    }

    if (uid >= 0) {
	if (setreuid(uid, uid) != 0) {
	    fprintf(stderr, "%s: setreuid(%d, %d) failed: %s\n",
		    prog, uid, uid, strerror(errno));
	    return -1;
	}
    }

    if (spawn) {
	/* Make up an environment for testing */
	char *fakeEnvp[] = {
	    "OSTYPE=K42",
	    HOSTTYPE(TARGET_MACHINE),
	    "PATH=/:/bin:/usr/bin",
	    "HOSTNAME=simos",
	    "USER=root",
	    NULL};
	ret = __k42_linux_spawn(argv[0], argv, fakeEnvp, 1);
    } else {
	ret = execvp(argv[0], &argv[0]);
    }

    return ret;
}

int
main(int argc, char *argv[])
{
    int c;
    const char *optstring = "+p:f:u:sdiDU";
    extern char *optarg;
    extern int optind;
    int port = 0;
    char *fname = NULL;
    int uid = -1;
    int use_spawn = 0;
    int dup_test = 0;
    int stdin_only = 0;
    int ret = 1;

    prog = argv[0];


    while ((c = getopt(argc, argv, optstring)) != EOF) {
	switch (c) {
	case 'p':
		port = strtol(optarg, (char **)NULL, 10);
	    break;
	case 'f':
		fname = optarg;
	    break;
	case 'u':
		uid = strtol(optarg, (char **)NULL, 10);
	    break;
	case 's':
		use_spawn = 1;
	    break;
	case 'd':
		dup_test = 1;
	    break;
	case 'i':
		stdin_only = 1;
	    break;
	case 'D':
 	    /* Yes we know it is an elipsis but the docs say... */
 	    ptrace(PTRACE_TRACEME, 0, 0, 0);
	    break;
	case 'U':
 	    /* Yes we know it is an elipsis but the docs say... */
 	    ptrace(PTRACE_ATTACH, 0, 0, 0);
	    break;
	case '?':
	default:
	    usage();
	    return (1);
	}
    }

    if (stdin_only && fname == NULL) {
	fprintf(stderr, "%s: WARNING: option `-i' without `-f' is ignored\n",
			prog);
	stdin_only = 0;
    }

    if (port ==0 && fname == NULL) {
	ret = runprog(use_spawn, dup_test, uid, &argv[optind]);
    } else {
	if (port != 0) {
	    if (dupsock(port) == -1) {
 		fprintf(stderr, "%s: dupsock(%d) failed: %s\n",
			prog, port, strerror(errno));
		return 1;
	    }
	    ret = runprog(use_spawn, dup_test, uid, &argv[optind]);
	}

	if (fname != NULL) {
	    int ffd;
	    struct stat sb;
	    if (stat(fname, &sb) == -1 ||
		!S_ISDIR(sb.st_mode)) {
		if ((ffd = open(fname, O_RDWR|O_CREAT, 0666)) == -1) {
		    fprintf(stderr, "%s: open(\"%s\") failed: %s\n",
			    prog, fname, strerror(errno));
		    return 1;
		}
	    } else {
		if ((ffd = open(fname, O_RDONLY)) == -1) {
		    fprintf(stderr, "%s: open(\"%s\") failed: %s\n",
			    prog, fname, strerror(errno));
		    return 1;
		}
	    }
	    if (dup2(ffd, 0) == -1) {
		fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
			prog, ffd, 0, strerror(errno));
		return -1;
	    }
	    if (!stdin_only) {
		if (dup2(ffd, 1) == -1) {
		    fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
			    prog, ffd, 1, strerror(errno));
		    return -1;
		}
		if (dup2(ffd, 2) == -1) {
		    fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
			    prog, ffd, 2, strerror(errno));
		    return -1;
		}
	    }
//	    close(ffd);
	    ret = runprog(use_spawn, dup_test, uid, &argv[optind]);
	}
    }

    return (ret);
}
