/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ttywire.c,v 1.1 2005/02/10 15:30:41 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: thinwire (de)multiplexor program
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#ifdef PLATFORM_AIX
#include <termios.h>
#else
#include <sys/termios.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>
#ifndef TCP_NODELAY
#include <tiuser.h>	// needed on AIX to get TCP_NODELAY defined
#endif /* #ifndef TCP_NODELAY */

/*
 * These prototypes should be in <termios.h>, but aren't.
 */
extern int tcgetattr(int, struct termios *);
extern int tcsetattr(int, int, const struct termios *);

int hw_flowcontrol = 0;
int speed = B9600;

// Assume on AIX that 'highbaud' has been enabled, meaning
// the following apply:
#ifdef PLATFORM_AIX
#define B57600	B50
#define B115200 B110
#endif

struct speed {
    int val;
    const char* name;
};

#define _EVAL(a) a
#define SPEED(x) { _EVAL(B##x) , #x }
struct speed speeds[]={
    SPEED(0),
    SPEED(50),
    SPEED(75),
    SPEED(110),
    SPEED(134),
    SPEED(150),
    SPEED(200),
    SPEED(300),
    SPEED(600),
    SPEED(1200),
    SPEED(1800),
    SPEED(2400),
    SPEED(4800),
    SPEED(9600),
    SPEED(19200),
    SPEED(38400),
    SPEED(57600),
    SPEED(115200),
#if defined(B230400)
    SPEED(230400),
#endif
    { 0, NULL}
};
int getSpeed(const char* name) {
    int i = 0;
    while (speeds[i].name) {
	if (strcmp(speeds[i].name, name)==0) {
	    return speeds[i].val;
	}
	++i;
    }
    return -1;
}

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif /* #ifndef MIN */
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* #ifndef MAX */

/*
 * These following defines need to be identical to their counterparts
 * in vhype/lib/thinwire.c and vtty.h
 */

int verbose = 0;
char *program_name;
char *victim;
char *tty_name;
int sock_port;
int victim_fd;
int victim_type;
int nchannels;
int nconnections;


int bitClear(int fd, int bit)
{
    int ret;
    if (!hw_flowcontrol) return 0;

    ret = ioctl(fd, TIOCMBIC, &bit);
    if (ret<0) {
        perror("ioctl(TIOCMBIC): ");
        return -1;
    }
    return ret;
}

int bitSet(int fd, int bit)
{
    int ret;
    if (!hw_flowcontrol) return 0;

    ret = ioctl(fd, TIOCMBIS, &bit);
    if (ret<0) {
        perror("ioctl(TIOCMBIS): ");
        return -1;
    }
    return ret;
}

volatile int lastStatus;
int checkBit(int fd, int bit)
{
    int ret;
    if (!hw_flowcontrol) return 0;

    ret = ioctl(fd, TIOCMGET, &lastStatus);
    if (ret<0) {
        perror("ioctl(TIOCMGET): ");
        return 0;
    }
    return lastStatus & bit;
}

void Fatal(char *msg, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, msg);
    vsprintf(buf, msg, ap);
    va_end(ap);

    fprintf(stderr, "ttywire: %s.\n", buf);
    exit(-1);
}

void Usage(void)
{
    fprintf(stderr, "Usage: %s [-s x] [-verbose] <tty dev> <port num>\n",
	    program_name);
    fprintf(stderr, "           where \"x\" is baudrate\n");
    exit(-1);
}



void SetSocketFlag(int socket, int level, int flag)
{
    int tmp = 1;
    if (setsockopt(socket, level, flag, (char *)&tmp, sizeof(tmp)) != 0) {
	Fatal("setsockopt(%d, %d, %d) failed", socket, level, flag);
    }
}

int TTYConnect(char* tty_name, int speed)
{
    int victim_fd;
    int status;
    struct termios serialstate;

    /*
     * Anything with a ':' in it is interpreted as
     * host:port over a TCP/IP socket.
     */

    /*
     * Perhaps it is a serial port
     */
    victim_fd = open(tty_name, O_RDWR|O_NOCTTY);
    if (victim_fd < 0) {
	Fatal("open() failed for victim (device \"%s\"): %d", victim, errno);
    }

    if (tcgetattr(victim_fd, &serialstate) < 0) {
	Fatal("tcgetattr() failed for victim");
    }

    /*
     * Baud rate.
     */
    cfsetospeed(&serialstate, speed);
    cfsetispeed(&serialstate, speed);

    /*
     * Raw mode.
     */
    serialstate.c_iflag = 0;
    serialstate.c_oflag = 0;
    serialstate.c_lflag = 0;
    serialstate.c_cflag &= ~(CSIZE | PARENB);
    serialstate.c_cflag |= CLOCAL | CS8;
    serialstate.c_cc[VMIN] = 1;
    serialstate.c_cc[VTIME] = 0;

    if (tcsetattr(victim_fd, TCSANOW, &serialstate) < 0) {
	fprintf(stderr, "tcsetattr() failed\n");
    }

    /* Pseudo tty's typically do not support modem signals */
    if (ioctl(victim_fd, TIOCMGET, &status) < 0) {
	hw_flowcontrol = 0;
    }

    return victim_fd;
}


int listen_fd;
int sock_fd;
int tty_fd;


void
SockSelectSetup(fd_set *rd_fds, int *max)
{
    while (sock_fd < 0) {
	sock_fd = accept(listen_fd, NULL, NULL);
    }
    FD_SET(sock_fd, rd_fds);
    *max = MAX(*max, sock_fd + 1);
}

void
StdOutSelectSetup(fd_set *rd_fds, int *max)
{
    FD_SET(0, rd_fds);

    *max = MAX(*max, 1);
}

void
TTYSelectSetup(fd_set *rd_fds, int *max)
{
    FD_SET(tty_fd, rd_fds);

    *max = MAX(*max, tty_fd + 1);

    /* Allow other side to write to us, those making it readable */
    bitSet(tty_fd, TIOCM_RTS);
}

int
SockTryRead(fd_set *rd_fds, char* buf, int size)
{
    if (!FD_ISSET(sock_fd, rd_fds)) {
	return 0;
    }

    int ret = read(sock_fd, buf, size);
    if (ret <= 0) {
	sock_fd = -1;
	ret = 0;
    }
    return ret;
}

int
SockWrite(char* buf, int size)
{
  retry:
    while (sock_fd < 0) {
	sock_fd = accept(listen_fd, NULL, NULL);
    }

    int ret = write(sock_fd, buf, size);
    if (ret <= 0) {
	goto retry;
    }
    return ret;
}

int
StdOutTryRead(fd_set *rd_fds, char *buf, int size)
{
    if (!FD_ISSET(0, rd_fds)) {
	return 0;
    }
    int ret = read(0, buf, size);
    if (ret <= 0) {
	Fatal("Failed to read stdin\n");
    }
    return ret;

}

int
TTYTryRead(fd_set *rd_fds, char *buf, int size)
{
    if (!FD_ISSET(tty_fd, rd_fds)) {
	return 0;
    }
    int ret = read(tty_fd, buf, size);
    if (ret <= 0) {
	Fatal("Failed to read stdin\n");
    }
    return ret;

}

int
StdOutWrite(char *buf, int size)
{
    int ret = write(1, buf, size);
    if (ret <= 0) {
	Fatal("Failed to write stdout\n");
    }
    return ret;

}

int
TTYWrite(char *buf, int size)
{
    int ret = write(tty_fd, buf, size);
    if (ret <= 0) {
	Fatal("Failed to write to tty\n");
    }
    return ret;

}


void (*selectSetup)(fd_set *rd_fds, int *max);
int (*doWrite)(char *buf, int size);
int (*tryRead)(fd_set *rd_fds, char *buf, int size);
char buf[4096];

int main(int argc, char **argv)
{
    int len;
    char *tty_name;
    int sock_port;
    int speed = B9600;

    ++argv; --argc;
    while (argc >= 2) {
	if (strcmp(argv[0],"-verbose")==0) {
	    verbose = 1;
	} else if (strcmp(argv[0],"-hw")==0) {
	    hw_flowcontrol = 1;
	} else if (strcmp(argv[0],"-s")==0) {
	    if (argc < 1) {
		Fatal("bad speed specification");
	    }
	    speed = getSpeed(argv[1]);
	    if (speed==-1) {
		Fatal("bad speed specification: %d %d",argv[1]);
	    }
	    argc--;
	    argv++;
	} else {
	    break;
	}
	argv++; argc--;
    }

    if (argc < 2) {
	Usage();
    }

    tty_name = argv[0];

    tty_fd = TTYConnect(tty_name, speed);

    if (strcmp(argv[1],"stdout") == 0) {
	selectSetup = StdOutSelectSetup;
	tryRead = StdOutTryRead;
	doWrite = StdOutWrite;
    } else {
	sock_port = atoi(argv[1]);
	sock_fd = -1;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
	    Fatal("Can't open socket\n");
	}

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htonl(sock_port);

	int ret = bind(listen_fd, (struct sockaddr*)&sin, sizeof(sin));
	if (ret < 0) {
	    Fatal("Can't bind to port: %d\n", sock_port);
	}

	listen(listen_fd, 4);
	selectSetup = SockSelectSetup;
	tryRead = SockTryRead;
	doWrite = SockWrite;
    }

    for (;;) {
	int max_fd;
	int ret;

	fd_set fds;
	FD_ZERO(&fds);

	TTYSelectSetup(&fds, &max_fd);
	(*selectSetup)(&fds, &max_fd);

	ret = select(max_fd, &fds, NULL, NULL, NULL);

	len = TTYTryRead(&fds, buf, sizeof(buf));
	if (len > 0) {
	    (*doWrite)(buf, len);
	}

	len = (*tryRead)(&fds, buf, sizeof(buf));
	if (len > 0) {
#if 1
	    // Perform \n -> \n\r conversion on stdin input
	    int x = 0;
	    int y = 0;
	    while (x < len) {
		if (buf[x] == '\n') {
		    TTYWrite(buf + y, x - y);
		    TTYWrite("\n\r",2);
		    ++x;
		    y = x;
		} else {
		    ++x;
		}
	    }
	    TTYWrite(buf + y, x - y);
#else
	    TTYWrite(buf, len);
#endif
	}
    }
}
