/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: forktest.c,v 1.17 2004/10/01 00:51:41 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for linux personality
 * **************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#ifdef K42
extern int __k42_linux_spawn(const char *,
			     char *const [], char *const [],
			     int);
#endif

static const char *prog;

static void
usage(void)
{
    fprintf(stderr,
	    "Usage: %s [-S] [-s [port]] [-c [port]] [-f <file>] [-d [dir]]\n"
	    "  test fork on streams. Other optional tests:\n", prog);
    fputs(
	"    -U [addr]  UNIX family address\n"
	"    -S         stream test (default if no args).\n"
	"    -s [port]  fork server test on [port] (def 4567) (INET only).\n"
	"    -c [port]  fork client test on [port] (def 4567) (INET only).\n"
	"    -C         spawn socket for client to talk to.\n"
	"    -f <file>  write to <file> by child then read by parent.\n"
	"    -d [dir]   open dir with parent read with child (def cwd).\n"
	"\n", stderr);
}

static int
streamTest(void)
{
    pid_t pid;

    pid = fork();
    if (pid == -1) {
	// fork failed
	(void) fprintf(stderr, "%s: fork() failed: %s\n",
		       prog, strerror(errno));
	return 1;
    } else if (pid == 0) {
	// I am child
	fprintf(stdout, "\nchild[%u]: Where is my Parent[%u]?\n",
		getpid(), getppid());
	exit(0);
    } else {
	// I am parent
	fprintf(stdout, "\nparent[%u]: Here I am child[%u]!\n",
		getpid(), pid);
	fflush(stdout);
    }

    return 0;
}

static void
socketServer(int wrfd)
{
    for (;;) {
	char buf[256];
	unsigned long len;
	char *pp = "\ntype: ";

	len = strlen(pp);
	write(wrfd, pp , len);

	len = read(wrfd, buf, 256);
	if (len == -1) {
	    (void) fprintf(stderr, "%s: read() failed: %s\n",
		       prog, strerror(errno));
	    return;
	}

	write(wrfd, buf, len);
	if (buf[0] == 'q') {
	    break;
	}
    }
}

static int
serverSocketTest(int port)
{
    pid_t pid;
    int rc;
    int fd;
    int wrfd;
    int slen;
    struct sockaddr_in saddr;

    //FIXME: since child and parent cannot run at the same time we
    //cannot have them communicate with each other. Instead, we simply
    //fork a child handle a service similar to inetd(8)

    printf("connect to port %d\n", port);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
	fprintf(stderr, "%s: socket() failed: %s",
		prog, strerror(errno));
	return 1;
    }

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);

    int one = 1;
    rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (rc != 0) {
	fprintf(stderr, "setsockopt(SO_REUSEADDR): %s\n", strerror(errno));
	return 1;
    }

    /* localhost */
    if (inet_aton("0", &saddr.sin_addr) == 0) {		/* INADDR_ANY */
	(void) fprintf(stderr, "%s: inet_aton(\"0\") failed: %s\n",
		       prog, strerror(errno));
	return 1;
    }

    rc = bind(fd, (struct sockaddr *)(&saddr), sizeof(saddr));
    if (rc) {
	(void) fprintf(stderr, "%s: bind() failed: %s\n",
		       prog, strerror(errno));
	return 1;
    }

    rc = listen(fd, 4);
    if (rc) {
	(void) fprintf(stderr, "%s: listen() failed: %s\n",
		       prog, strerror(errno));
	return 1;
    }

    wrfd = accept(fd, (struct sockaddr *)(&saddr), &slen);
    if (wrfd == -1) {
	(void) fprintf(stderr, "%s: accept() failed: %s\n",
		       prog, strerror(errno));
	return 1;
    }

    pid = fork();
    if (pid == -1) {
	// fork failed
	(void) fprintf(stderr, "%s: fork() failed: %s\n",
		       prog, strerror(errno));
	return 1;
    } else if (pid == 0) {
	// I am child
	close (fd);
	socketServer(wrfd);
	exit(0);
    }

    return 0;
}

static void
socketClient(int fd)
{
    int i = 0;

    char const *yow[] = {
	"My Aunt MAUREEN was a military advisor to IKE & TINA TURNER!!\n",
	"TONY RANDALL!  Is YOUR life a PATIO of FUN??\n",
	"I'm not an Iranian!!  I voted for Dianne Feinstein!!\n",
	"It's a lot of fun being alive...  I wonder if my bed is made?!?\n",
	"YOW!!  I am having FUN!!\n",
	NULL
    };


    for (;;) {
	char buf[256];
	unsigned long len;

	len = read(fd, buf, 256);
	if (len == -1) {
	    (void) fprintf(stderr, "%s: read() failed: %s\n",
		       prog, strerror(errno));
	    return;
	} else {
	    buf[len]='\0';
	    printf("server: %s", buf);
	}
	if (yow[i] != NULL) {
	    write(fd, yow[i], strlen(yow[i]));
	    printf("%s", yow[i]);
	    ++i;
	} else {
	    break;
	}
    }
    // get server to quit
    write(fd, "q\n", 2);

    return;
}

static void
spawnEchoServer(const char *name)
{
#ifdef K42
    char *const envp[] = {
	NULL
    };
    char *const argv[] = {
	(char *)name,
	NULL
    };

    (void) __k42_linux_spawn(name, argv, envp, 0);
#endif
}

static int
clientSocketTest(int port)
{
    pid_t pid;
    int fd;
    int wstat;
    struct sockaddr_in saddr;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
	fprintf(stderr, "%s: socket() failed: %s",
		prog, strerror(errno));
	return 1;
    }

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    if (inet_aton("127.1", &saddr.sin_addr) == 0) {	/* INADDR_ANY */
	(void) fprintf(stderr, "%s: inet_aton(\"127.1\") failed: %s\n",
		       prog, strerror(errno));
	return 1;
    }


    if (connect(fd, (struct sockaddr *)&saddr, sizeof (saddr)) == -1) {
	fprintf(stderr, "%s: connect() failed : %s",
		prog, strerror(errno));
	return 1;
    }

    printf("connect success\n");

    pid = fork();
    if (pid == -1) {
	// fork failed
	(void) fprintf(stderr, "%s: fork() failed: %s\n",
		       prog, strerror(errno));
	return 1;
    } else if (pid == 0) {
	// I am child
	sleep(1);
	socketClient(fd);
	exit(0);
    }
    if (waitpid(pid, &wstat, 0) == -1) {
	(void) fprintf(stderr, "%s: wait(%d) failed: %s\n",
		       prog, pid, strerror(errno));
	return 1;
    }
    return 0;
}

static int
unixClient(char* addr)
{
    int fd;

    struct sockaddr_un saddr;
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
	fprintf(stderr, "%s: socket() failed: %s",
		prog, strerror(errno));
	return 1;
    }

    saddr.sun_family = AF_UNIX;
    saddr.sun_path[0] = 0;
    memcpy(saddr.sun_path+1, addr, strlen(addr));

    if (connect(fd, (struct sockaddr *)&saddr, 3+strlen(addr)) == -1) {
	fprintf(stderr, "%s: connect() failed : %s",
		prog, strerror(errno));
	return 1;
    }
    printf("connect success\n");

    socketClient(fd);
    return 0;
}

static int
dirTest(const char *dname) {
    int fd;
    pid_t pid;

    fd = open(dname, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "%s: open(\"%s\") for dir failed: %s\n",
		prog, dname, strerror(errno));
	return 1;
    }

    pid = fork();
    if (pid == -1) {
	// fork failed
	(void) fprintf(stderr, "%s: fork() failed: %s\n",
		       prog, strerror(errno));
	return 1;
    } else if (pid == 0) {
	char strfd[20];

	(void) snprintf(strfd, sizeof(strfd), "%d", fd);

	execl("dir", "dir", "-f", strfd, dname, NULL);
    }

    return 0;

}

static int
fileTest(const char *fname) {
    pid_t pid;
    int fd;
    char buf[128];
    size_t len;

    fd = open(fname, O_RDWR|O_CREAT, 0666);
    if (fd == -1) {
	(void) fprintf(stderr, "%s: child: open(\"%s\") failed: %s\n",
		       fname, prog, strerror(errno));
	return 1;
    }
    len = sprintf(buf, "\nthis was written before fork by parent[%u]\n",
		  getpid());
    write(fd, buf, len + 1);

    pid = fork();
    if (pid == -1) {
	// fork failed
	(void) fprintf(stderr, "%s: fork() failed: %s\n",
		       prog, strerror(errno));
	return 1;
    } else if (pid == 0) {
	// I am child
	len = sprintf(buf, "\nthis was written by child[%u] for parent[%u]\n",
		      getpid(), getppid());
	write(fd, buf, len + 1);
	exit(0);
    } else {
	// FIXME: should wait for child but don't have wait yet I am
	// parent

	if (lseek(fd, 0, SEEK_SET) == -1) {
	    (void) fprintf(stderr, "%s: lseek() failed: %s\n",
			   prog, strerror(errno));
	    return 1;
	}

	if (read(fd, buf, sizeof(buf)) == -1) {
	    (void) fprintf(stderr, "%s: read() failed: %s\n",
			   prog, strerror(errno));
	    return 1;
	}

	fprintf(stdout, "\nparent[%u]: %s\n",
		getpid(), buf);
	fflush(stdout);
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int rc = 1;
    int c;
    int sport = -1;
    int cport = -1;
    int stream = 0;
    char *fname = NULL;
    char *dname = NULL;
    const char *optstring = "SC:s::c::d::f:U:";
    extern char *optarg;

    prog = argv[0];

    if (argc == 1) {
	stream = 1;
    }


    while ((c = getopt(argc, argv, optstring)) != EOF) {
	switch (c) {
	case 'U':
	    unixClient(optarg);
	    exit(1);
	    break;
	case 's':
	    if (optarg == NULL ||
		(sport = strtol(optarg, (char **)NULL, 10)) == 0) {
		sport = 4567;
	    }
	    break;
	case 'c':
	    if (optarg == NULL ||
		(cport = strtol(optarg, (char **)NULL, 10)) == 0) {
		cport = 4567;
	    }
	    break;
	case 'd':
	    if (optarg == NULL) {
		dname = ".";
	    } else {
		dname = optarg;
	    }
	    break;
	case 'f':
	    fname = optarg;
	    break;
	case 'C':
	    spawnEchoServer(optarg);
	    exit(1);
	case 'S':
	    stream = 1;
	    break;
	case '?':
	default:
	    usage();
	    return (1);
	}
    }

    if (stream == 1) {
	rc = streamTest();
    }

    if (sport != -1) {
	rc = serverSocketTest(sport);
    }

    if (cport != -1) {
	rc = clientSocketTest(cport);
    }

    if (dname != NULL) {
	rc = dirTest(dname);
    }
    if (fname != NULL) {
	rc = fileTest(fname);
    }

    return rc;

}
