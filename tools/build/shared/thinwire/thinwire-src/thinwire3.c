/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: thinwire3.c,v 1.13 2006/04/04 23:55:14 mostrows Exp $
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
#include <signal.h>
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
#include <sys/poll.h>
#include <signal.h>
/*
 * These prototypes should be in <termios.h>, but aren't.
 */
extern int tcgetattr(int, struct termios *);
extern int tcsetattr(int, int, const struct termios *);


char *program_name = "thinwire3";
char *victim_name = NULL;
int hw_flowcontrol = 0;
int verbose = 0;
int debug = 0;
int nchannels;

int speed1 = B9600;

// Assume on AIX that 'highbaud' has been enabled, meaning
// the following apply:
#ifdef PLATFORM_AIX
#define B57600	B50
#define B115200 B110
#endif

int speed2 = B115200;
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

void Message(char *msg, ...)
{
    //    static int count = 0;
    //    if (count++ > 100 ) return;

    va_list ap;
    char buf[256];

    va_start(ap, msg);
    vsprintf(buf, msg, ap);
    va_end(ap);

    fprintf(stdout,"%s: %s.\n", program_name, buf);
    fflush(stdout);
}

void Fatal(char *msg, ...)
{
    va_list ap;
    char buf[256];
    //int i;

    va_start(ap, msg);
    vsprintf(buf, msg, ap);
    va_end(ap);

    fprintf(stderr, "%s: %s.\n", program_name, buf);

    exit(-1);
}

void DumpCharHex(char *hdr, char* buf, int len)
{
    int i;
    int j = 0;
    char c;


    while (len) {
	int n = MIN(len, 16);
	if (j==0) {
	    fprintf(stderr, hdr);
	} else {
	    fprintf(stderr, "          ");
	}
	for (i = 0; i < n; i++) {
	    c = buf[j+i];
	    if ((c < ' ') || (c > '~')) c = '.';
	    putc(c, stderr);
	}
	fprintf(stderr, "%*s | ", 17 - n, " ");
	for (i = 0; i < n; i++) {
	    fprintf(stderr, " %02x", ((int) buf[j+i]) & 0xff);
	}
	putc('\n', stderr);
	len -= n;
	j+= n;
    }
}

void Display(int chan, char direction, char *buf, int len)
{
    char hdr[32];
    hdr[0] = 0;
    sprintf(hdr, "%2.2d %c %4.4d ", chan, direction, len);
    DumpCharHex(hdr, buf, len);
}

/*
 * These following defines need to be identical to their counterparts
 * in vhype/lib/thinwire.c and vtty.h
 */
#define CHAN_SET_SIZE 8
#define MAX_STREAMS 95
#define MAX_PACKET_LEN (4096*4)
#define STREAM_READY (1<<16)

#define MAX_FD 512
struct pollfd polled_fd[MAX_FD] = { {0, }, };

struct iochan {
    int fd;
    int poll_idx;
    int stream_id;
    int status;
    long data; /* private data */
    int (*write)(struct iochan* ic, char *buf, int len, int block);
    int (*read)(struct iochan* ic, char *buf, int len, int block);
    int (*state)(struct iochan *ic, int new_state);
    void (*detach)(struct iochan *ic);
    void (*setSpeed)(struct iochan *ic, char *buf, int len);
};

int last_channel = 0;
static struct iochan channels[MAX_FD];

static struct iochan *streams[MAX_STREAMS] = {NULL,};
static struct iochan *victim = NULL;

static int
default_state(struct iochan* c, int new_state)
{
    c->status = new_state | STREAM_READY;
    return 0;
}

static void
default_setSpeed(struct iochan *ic, char *buf, int len)
{
    /* Return a speed of "0" indicating no change. */
    char reply[6] = { 'S', ' ', ' ', ' ' + 1, '0' };
    ic->write(ic, reply, 6, 1);
}

static void
default_detach(struct iochan *ic)
{
    Message("detach stream %d", ic->stream_id);
    close(polled_fd[ic->poll_idx].fd);
    polled_fd[ic->poll_idx].fd = 0;
    polled_fd[ic->poll_idx].events = 0;
    ic->fd = 0;
    ic->state = NULL;
    ic->status = 0;
    streams[ic->stream_id] = NULL;
}

static struct iochan*
get_channel(int fd)
{
    int i = 0;
    for (; i < MAX_FD && i <= last_channel; ++i) {
	if (channels[i].state == NULL) {
	    channels[i].state = default_state;
	    channels[i].setSpeed = default_setSpeed;
	    channels[i].detach = default_detach;
	    channels[i].poll_idx = i;
	    channels[i].fd = fd;
	    channels[i].status = STREAM_READY;
	    polled_fd[i].fd = fd;
	    if (i == last_channel) {
		++last_channel;
	    }
	    return &channels[i];
	}
    }
    return NULL;
}


static int
bitClear(int fd, int bit)
{
    int ret;
    ret = ioctl(fd, TIOCMBIC, &bit);
    if (ret<0) {
	perror("ioctl(TIOCMBIC): ");
	return -1;
    }
    return ret;
}

static int
bitSet(int fd, int bit)
{
    int ret;
    ret = ioctl(fd, TIOCMBIS, &bit);
    if (ret<0) {
        perror("ioctl(TIOCMBIS): ");
        return -1;
    }
    return ret;
}

volatile int lastStatus;
static int
checkBit(int fd, int bit)
{
    int ret;
    ret = ioctl(fd, TIOCMGET, &lastStatus);
    if (ret<0) {
        perror("ioctl(TIOCMGET): ");
        return 0;
    }
    return lastStatus & bit;
}


/*
 * Definitions for a channel that functions over a serial port,
 * and may manage hw flow control lines.
 */

static int
serial_read(struct iochan* ic, char *buf, int len, int block)
{
    int cnt;
    if (hw_flowcontrol)
	bitSet(ic->fd, TIOCM_RTS);

    cnt = read(ic->fd, buf, len);
    if (cnt < len) {
	ic->status  &= ~POLLIN;
    }

    if (hw_flowcontrol)
	bitClear(ic->fd, TIOCM_RTS);
    return cnt;
}

static int
serial_write(struct iochan *ic, char *buf, int len, int block)
{
    if (hw_flowcontrol) {
	while (checkBit(ic->fd, TIOCM_CTS) == 0);
    }
    return write(ic->fd, buf, len);
}

static void
serial_setSpeed(struct iochan *ic, char *buf, int len)
{
    char reply[32];
    struct termios t;
    int speed;

    memcpy(reply, buf, len);
    reply[len] = 0;

    speed = getSpeed(reply);
    if (debug) {
	Message("Setting serial line speed: %*.*s %d",
		len, len, buf, speed);
    }

    if (speed < 0) {
	Fatal("Invalid speed request: %*.*s %d", len, len, buf);
    }

    memcpy(reply + 5, buf, len);
    reply[0] = 'S';
    reply[1] = ' ' + ((len >> 12) & 63);
    reply[2] = ' ' + ((len >> 6) & 63);
    reply[3] = ' ' + ((len) & 63);
    reply[4] = ' ';

    /* Write response, and set new speed, after draining output */
    ic->write(ic, reply, 5 + len, 1);

    /* Pause to let the other side adjust */
    sleep(1);

    tcgetattr(ic->fd, &t);
    cfsetospeed(&t, speed);
    cfsetispeed(&t, speed);

    if (debug) {
	Message("VictimWrite: %d bytes to fd %d", 5 + len, ic->fd);
	DumpCharHex("raw write:", reply, 5 + len);
    }

    tcsetattr(ic->fd, TCSADRAIN, &t);

    /* Write the reply again, and now both side work at new speed */
    ic->write(ic, reply, 5 + len, 1);
    if (debug) {
	Message("VictimWrite: %d bytes to fd %d", 5 + len, ic->fd);
	DumpCharHex("raw write:", reply, 5 + len);
    }

}

static struct iochan*
serial_channel(int fd)
{
    struct iochan* c = get_channel(fd);

    c->write = serial_write;
    c->read = serial_read;
    c->setSpeed = serial_setSpeed;
    polled_fd[c->poll_idx].events = POLLIN;

    return c;
}

/*
 * Definitions for a basic channel that just does read and write.
 * Read path may be optional.
 */
static int
default_read(struct iochan *ic, char *buf, int len, int block)
{
    int ret = read(ic->fd, buf, len);
    if (ret < len) {
	ic->status &= ~POLLIN;
    }
    return ret;
}

static int
default_write(struct iochan *ic, char *buf, int len, int block)
{
    return write(ic->fd, buf, len);
}

static int
no_read(struct iochan *ic, char *buf, int len, int block)
{
    return 0;
}

static struct iochan*
basic_channel(int fd, int out_only)
{
    struct iochan* c = get_channel(fd);

    c->write = default_write;
    if (out_only) {
	polled_fd[c->poll_idx].events = 0;
	c->read = no_read;
    } else {
	polled_fd[c->poll_idx].events = POLLIN | POLLERR | POLLHUP;
	c->read = default_read;
    }
    return c;
}


static int
stdin_read(struct iochan *ic, char *buf, int len, int block)
{
    int ret = read(0, buf, len);
    if (ret < len) {
	ic->status &= ~POLLIN;
    }
    return ret;
}

static struct iochan*
stdout_channel(int fd, int out_only)
{
    struct iochan* c = get_channel(fd);

    c->write = default_write;
    if (out_only) {
	polled_fd[c->poll_idx].events = 0;
	c->read = no_read;
    } else {
	polled_fd[c->poll_idx].events = POLLIN | POLLERR | POLLHUP;
	c->read = stdin_read;
    }
    return c;
}

static void
SetSocketFlag(int socket, int level, int flag)
{
    int tmp = 1;
    if (setsockopt(socket, level, flag, (char *)&tmp, sizeof(tmp)) != 0) {
	Fatal("setsockopt(%d, %d, %d) failed", socket, level, flag);
    }
}

static int
tcp_listen_state(struct iochan *orig, int state)
{
    int fd;
    struct protoent *protoent;
    //struct iochan *c;
    int id = orig->stream_id;
    if (! (state & POLLIN)) {
	return 0;
    }

    fd = accept(orig->data, 0, 0);
    if (fd < 0) {
	Fatal("accept() failed for stream %d", id);
    }

    protoent = getprotobyname("tcp");
    if (protoent == NULL) {
	Fatal("getprotobyname(\"tcp\") failed");
    }
    SetSocketFlag(fd, protoent->p_proto, TCP_NODELAY);
    Message("accepted connection on stream %d",id);

    polled_fd[orig->poll_idx].events = POLLIN |POLLERR |POLLHUP;
    polled_fd[orig->poll_idx].fd = fd;
    orig->fd = fd;

    orig->state = default_state;
    orig->status = STREAM_READY;
    orig->read = default_read;
    orig->write = default_write;

    return 1;
}

/* Restore the listening stream socket */
static void
tcp_detach_listen(struct iochan *ic)
{
    Message("detach tcp stream %d", ic->stream_id);
    ic->state = tcp_listen_state;
    ic->fd = (int)ic->data;
    ic->status = 0;
    polled_fd[ic->poll_idx].fd = ic->fd;
    polled_fd[ic->poll_idx].events = POLLIN;
}

#define MAX_RETRY_BIND_PORT 20

static struct iochan*
tcp_listen_channel(int port, int stream_id)
{
    struct sockaddr_in sockaddr;
    struct iochan *c;
    int num_bind_retries = 0;
    int fd  = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	Fatal("socket() failed for stream %d", stream_id);
    }

    /* Allow rapid reuse of this port. */
    SetSocketFlag(fd, SOL_SOCKET, SO_REUSEADDR);
#ifndef __linux__
#ifndef PLATFORM_CYGWIN
    SetSocketFlag(fd, SOL_SOCKET, SO_REUSEPORT);
#endif /* #ifndef PLATFORM_CYGWIN */
#endif /* #ifndef __linux__ */

    sockaddr.sin_family = PF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(port);
  retry:
    if (bind(fd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) != 0) {
	if (errno == EADDRINUSE) {
	    if (num_bind_retries>MAX_RETRY_BIND_PORT) {
		Fatal("Too many retries for bind on port %d for stream %d\n",
		      port, stream_id);
	    }
	    Message("Retrying bind on port %d", port);
	    num_bind_retries++;
	    sleep(1);
	    goto retry;
	} else {
	    Fatal("Bind failed");
	}
    }

    listen(fd, 4);

    c = get_channel(fd);

    polled_fd[c->poll_idx].events = POLLIN;

    c->state = tcp_listen_state;
    c->detach = tcp_detach_listen;
    c->stream_id = stream_id;
    c->data = fd;
    c->status = 0;

    Message("IO Channel %d listening on port %d for stream %d",
	    c->poll_idx, port, stream_id);
    return c;
}

static int
check_channel(struct iochan *c, int timeout)
{
    int idx = c->poll_idx;
    int ret = poll(&polled_fd[idx], 1, timeout);
    if (ret) {
	c->state(c, polled_fd[idx].revents & polled_fd[idx].events);
    }
    return ret;
}

static void
check_all_channels(int timeout)
{
    int i = 0;
    int retry;
    do {
	int ret;
	ret = poll(&polled_fd[0], last_channel, timeout);
	retry = 0;
	for (i = 0; i < last_channel; ++i) {
	    if (channels[i].state) {
		retry |= channels[i].state(&channels[i],
					   polled_fd[i].events &
					   polled_fd[i].revents);
	    }
	}
    } while (retry);
}



void
Usage(void)
{
    fprintf(stderr, "Usage: %s [-s x y ] [-verbose] [-debug] <victim> <channel_port> ...\n",
	    program_name);
    fprintf(stderr, "           where <victim> is <host>:<port> or "
	    "<serial_device>\n");
    fprintf(stderr, "\tUse 'thinwire2' for v2 of the thinwire protocol\n");
    fprintf(stderr, "\tFor thinwire2, '-s x y' specifies the initial\n"
	    "\tand final serial port speeds.\n");
    exit(-1);
}

void
ParseCommandLine(int argc, char **argv)
{
    program_name = argv[0];

    ++argv; --argc;
    while (1) {
	if (strcmp(argv[0],"-verbose")==0) {
	    verbose = 1;
	} else if (strcmp(argv[0],"-debug")==0) {
	    debug = 1;
	} else if (strcmp(argv[0],"-hw")==0) {
	    hw_flowcontrol = 1;
	} else if (strcmp(argv[0],"-s")==0) {
	    if (argc<2) {
		Fatal("bad speed specification");
	    }
	    speed1 = getSpeed(argv[1]);
	    speed2 = getSpeed(argv[2]);
	    if (speed1==-1 || speed2==-1) {
		Fatal("bad speed specification: %d %d",argv[1], argv[2]);
	    }
	    argc-=2;
	    argv+=2;
	} else {
	    break;
	}
	argv++; argc--;
    }

    if (argc < 2) {
	Usage();
    }

    victim_name = argv[0];
    nchannels = 0;
    argv++; argc--;

    for (nchannels = 0; nchannels < MAX_STREAMS; ++nchannels) {
	streams[nchannels] = NULL;
    }
    nchannels = 0;
    while (argc && (nchannels < MAX_STREAMS)) {
	int port = strtol(argv[0], NULL, 0);
	if (argv[0][0]==':') {
	    do {
		++nchannels;
	    } while (nchannels % CHAN_SET_SIZE);
	    argv++; argc--;
	    continue;
	}

	if (strcmp(argv[0],"stdout")==0) {
	    streams[nchannels] = stdout_channel(1, 1);
	} else if (strcmp(argv[0],"stderr")==0) {
	    streams[nchannels] = stdout_channel(2, 1);
	} else if (strcmp(argv[0],"stdin")==0) {
	    streams[nchannels] = stdout_channel(1, 0);
	} else if (port > 0) {
	    streams[nchannels] = tcp_listen_channel(port, nchannels);
	} else {
	    Fatal("Unrecognizable stream spec: '%s'", argv[0]);
	}
	if (streams[nchannels]) {
	    streams[nchannels]->stream_id = nchannels;
	}

	++nchannels;
	++argv; --argc;
    }
    if (argc) {
	Fatal("%d stream maximum, %d specified", MAX_STREAMS, nchannels+argc);
    }
}




void VictimConnect(char *victim_name, int speed)
{
    char *p, *host;
    int port, status;
    struct hostent *hostent;
    struct sockaddr_in sockaddr;
    struct sockaddr unixname;
    struct protoent *protoent;
    struct termios serialstate;
    int victim_fd;

    /*
     * Anything with a ':' in it is interpreted as
     * host:port over a TCP/IP socket.
     */
    p = strrchr(victim_name, ':');
    if (p != NULL) {
	*p = '\0';
	host = victim_name;
	port = atoi(p+1);

	hostent = gethostbyname(host);

	if (hostent == NULL) {
	    Fatal("unknown victim host: %s", host);
	}

	Message("connecting to victim (host \"%s\", port %d)", host, port);
	while (1) {
	    victim_fd = socket(PF_INET, SOCK_STREAM, 0);
	    if (debug)
		Message("victim is fd %d", victim_fd);

	    if (victim_fd < 0) {
		Fatal("socket() failed for victim");
	    }
	    /* Allow rapid reuse of this port. */
	    SetSocketFlag(victim_fd, SOL_SOCKET, SO_REUSEADDR);
#ifndef __linux__
#ifndef PLATFORM_CYGWIN
	    SetSocketFlag(victim_fd, SOL_SOCKET, SO_REUSEPORT);
#endif /* #ifndef PLATFORM_CYGWIN */
#endif /* #ifndef __linux__ */
	    /* Enable TCP keep alive process. */
	    SetSocketFlag(victim_fd, SOL_SOCKET, SO_KEEPALIVE);

	    sockaddr.sin_family = PF_INET;
	    sockaddr.sin_port = htons(port);
	    memcpy(&sockaddr.sin_addr.s_addr, hostent->h_addr,
		   sizeof (struct in_addr));

	    if (connect(victim_fd, (struct sockaddr *) &sockaddr,
			sizeof(sockaddr)) != 0) {
		if ( errno == ECONNREFUSED ) {
		    /* close and retry */
		    close(victim_fd);
		    sleep(1);
		} else {
		    /* fatal error */
		    Fatal("connecting to victim failed");
		}
	    } else {
		// connected
		break;
	    }
	}
	Message("connected on fd %d", victim_fd);

	protoent = getprotobyname("tcp");
	if (protoent == NULL) {
	    Fatal("getprotobyname(\"tcp\") failed");
	}

	SetSocketFlag(victim_fd, protoent->p_proto, TCP_NODELAY);

	victim = basic_channel(victim_fd, 0);

    	return;
    }

    /*
     * Perhaps it is a Unix domain socket
     */
    victim_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    unixname.sa_family = AF_UNIX;
    strcpy(unixname.sa_data, victim_name);

    if (connect(victim_fd, &unixname, strlen(unixname.sa_data) +
		sizeof(unixname.sa_family)) >= 0) {
	Message("connected on fd %d", victim_fd);

	victim = basic_channel(victim_fd, 0);
    	return;
    }
    close(victim_fd);

    /*
     * Perhaps it is a serial port
     */
    victim_fd = open(victim_name, O_RDWR|O_NOCTTY);
    if (victim_fd < 0) {
	Fatal("open() failed for victim (device \"%s\"): %d", victim_name, errno);
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
#if 1
    serialstate.c_iflag = 0;
    serialstate.c_oflag = 0;
    serialstate.c_lflag = 0;
    serialstate.c_cflag &= ~(CSIZE | PARENB);
    serialstate.c_cflag |= CLOCAL | CS8;
    serialstate.c_cc[VMIN] = 1;
    serialstate.c_cc[VTIME] = 0;

#else
    cfmakeraw(&serialstate);
    serialstate.c_cflag |= CLOCAL;
#endif

    if (tcsetattr(victim_fd, TCSANOW, &serialstate) < 0) {
	fprintf(stderr, "tcsetattr() failed\n");
    }

    /* Pseudo tty's typically do not support modem signals */
    if (ioctl(victim_fd, TIOCMGET, &status) < 0) {
	victim = basic_channel(victim_fd, 0);
    } else {
	victim = serial_channel(victim_fd);

	/* Allow other side to write to us, those making it readable */
	bitSet(victim_fd, TIOCM_RTS);
    }
}

int VictimRead(char *buf, int len)
{
    int cnt = victim->read(victim, buf, len, 1);

    if (debug) {
	Message("VictimRead: %d/%d bytes from fd %d", cnt, len, victim->fd);
	DumpCharHex("raw read: ", buf, cnt);
    }

    if (cnt < 0) {
	Fatal("read() failed for victim");
    }
    if (cnt == 0) {
	Fatal("EOF on read from victim");
    }
    return cnt;
}

void VictimWrite(char *buf, int len)
{
    int ret;
    if (debug) {
	Message("VictimWrite: %d bytes to fd %d", len, victim->fd);
	DumpCharHex("raw write:", buf, len);
    }

    ret = victim->write(victim, buf, len, 1);
    if (ret != len) {
	Fatal("write() failed for victim socket");
    }
}

int
StreamWrite(int id, char *buf, int len, int block_for_connect)
{
    int n;
    int total = 0;
    int orig = len;

    if (!streams[id]) return -1;

  restart:
    if (! (streams[id]->status & STREAM_READY)) {
	/* Perhaps somebody will connect */
	if (block_for_connect == 0) {
	    check_channel(streams[id], 0);
	} else {
	    check_channel(streams[id], -1);
	    goto restart;
	}
    }

    if (! (streams[id]->status & STREAM_READY)) {
	/* Here we know block_for_connect == 0 ,
	 *  so non-blocking, thus quietly abort */
	return -1;
    }

    if (verbose) {
	Display(id, '>', buf, len);
    }
    while (len) {
	n = streams[id]->write(streams[id], buf, len, 1);
	if (n < 0) {
	    /* Should we go to restart if block_for_connect? */
	    streams[id]->detach(streams[id]);
	    if (block_for_connect) goto restart;
	    Message("Aborted write\n");
	    if (total == 0) {
		return -1;
	    }
	    return total;
	} else {
	    buf += n;
	    len -= n;
	}
	total += n;

    }
    if (debug) {
	Message("StreamWrite: %d/%d bytes to stream %d fd %d",
		total, orig, id, streams[id]->fd);
    }
    return total;
}

int
StreamRead(int id, char *buf, int len, int block_for_connect)
{
    int n;
    n = 0;

    if (!streams[id]) return -1;

  restart:
    if (! (streams[id]->status & STREAM_READY)) {
	/* Perhaps somebody will connect */
	if (block_for_connect == 0) {
	    check_channel(streams[id], 0);
	} else {
	    check_channel(streams[id], -1);
	    goto restart;
	}
    }

    if (! (streams[id]->status & STREAM_READY)) {
	/* Here we know block_for_connect == 0 ,
	 *  so non-blocking, thus quietly abort */
	return n;
    }

    while (block_for_connect && !(streams[id]->status & POLLIN)) {
	check_channel(streams[id], -1);
    }

    if (streams[id]->status & POLLIN) {
	n = streams[id]->read(streams[id], buf, len, 1);
	if (debug) {
	    Message("ChannelRead: %d bytes from stream %d fd %d", len, id,
		    streams[id]->fd);
	}

	if (n <= 0) {
	    streams[id]->detach(streams[id]);

	    if (block_for_connect) goto restart;
	    n = 0;
	}
    }

    if (verbose) {
	Display(id, '<', buf, n);
    }

    return (n);
}


int DoSelect(int base)
{
    int i;
    int retval = 0;
    check_all_channels(0);

    for (i = base; i < base + CHAN_SET_SIZE; i++) {

	if (!streams[i]) continue;

	if ((streams[i]->status & STREAM_READY)
	    && (streams[i]->status & POLLIN)) {
	    retval |= 1 << (i - base);
	}
    }

    if (verbose) {
	fprintf(stderr, "   !(%d) %x\n", base/CHAN_SET_SIZE, retval);
    }
    if (debug) {
	Message("DoSelect returning %x", retval);
    }
    return (retval);
}


int loop = 1;

int ProcessPackets(char *buf, int len)
{
    unsigned int pktlen, chan;
    static char inbuf[5 + MAX_PACKET_LEN];
    int line = 0;
    int ret;

    while (len) {
	if (len < 5) {
	    /* If character is not one of the characters that can
	     * begin a thinwire packet, write it to channel 0 */
	    const char headers[] = "0#A!?S$/";
	    if (!strchr(headers, buf[0])) {
		line = __LINE__;
		goto stray;
	    }
	    break;
	}
	pktlen = ((((unsigned char) buf[1]) - ' ') << 12) |
	    ((((unsigned char) buf[2]) - ' ') << 6) |
	    ((((unsigned char) buf[3]) - ' ') << 0);
	if (pktlen > MAX_PACKET_LEN) {
	    Message("packet length (%d) exceeds maximum (%d)",
		  pktlen, MAX_PACKET_LEN);
	    line = __LINE__;
	    goto stray;
	}

	chan = buf[4] - ' ';
	if (chan >= MAX_STREAMS) {
	    line = __LINE__;
	    goto stray;
	}

	/* 0 -- write command */
	if ('0' == buf[0] || '#' == buf[0]) {
	    /* 0 command cannot follow stray chars */
	    if (line && buf[0] == '0') {
		line = __LINE__;
		goto stray;
	    }

	    if (len < (5 + pktlen)) {
		break;
	    }
	    if (debug) {
		Message("ProcessPackets, got '%c' chan %d, pktlen %d len %d",
			buf[0],chan, pktlen, len);
	    }

	    ret = StreamWrite(chan, buf + 5, pktlen, 1);
	    if (ret < 0) {
		line = __LINE__;
		goto stray;
	    }

	    /* No ack for "#" style writes */
	    if ('0' != buf[0]) {
		buf += (5 + pktlen);
		len -= (5 + pktlen);
	    } else {
		buf += (5 + pktlen);
		len -= (5 + pktlen);
		inbuf[0] = 'A';
		inbuf[1] = ' ' + ((pktlen >> 12) & 0x3f);
		inbuf[2] = ' ' + ((pktlen >>  6) & 0x3f);
		inbuf[3] = ' ' + ((pktlen >>  0) & 0x3f);
		inbuf[4] = ' ' + chan;
		VictimWrite(inbuf, 5);
	    }
	    continue;
	}

	/* A -- read command */
	else if ('A' == buf[0]) {
	    if (debug) {
		Message("ProcessPackets, got 'A', chan %d", chan);
	    }
	    pktlen = StreamRead(chan, inbuf + 5, pktlen, 1);
	    inbuf[0] = '0' + chan;
	    inbuf[1] = ' ' + ((pktlen >> 12) & 0x3f);
	    inbuf[2] = ' ' + ((pktlen >>  6) & 0x3f);
	    inbuf[3] = ' ' + ((pktlen >>  0) & 0x3f);
	    inbuf[4] = chan + ' ';
	    VictimWrite(inbuf, pktlen + 5);
	    buf += 5;
	    len -= 5;
	    continue;
	}

	/* ! -- select command */
	else if ('!' == buf[0]) {
	    unsigned int retval;
	    int i;
	    if (debug) {
		Message("ProcessPackets, got '!', chan %d", chan);
	    }
	    retval = DoSelect(chan);
	    inbuf[0] = '!';
	    inbuf[1] = ' ' + 0;
	    inbuf[2] = ' ' + 0;
	    inbuf[3] = ' ' + 4;
	    inbuf[4] = ' ' + chan;
	    // always encode as big endian
	    for (i = 8; i >= 5; i--) {
		inbuf[i] = retval & 0xFF;
		retval = retval >> 8;
	    }

	    VictimWrite(inbuf, 9);

	    buf += 5;
	    len -= 5;
	    continue;
	}

	/* ! -- select command, 4-bit encoding*/
	else if ('?' == buf[0]) {
	    unsigned int retval;
	    int i;
	    if (debug) {
		Message("ProcessPackets, got '?', chan %d", chan);
	    }
	    retval = DoSelect(chan);
	    inbuf[0] = '?';
	    inbuf[1] = ' ' + 0;
	    inbuf[2] = ' ' + 0;
	    inbuf[3] = ' ' + 8;
	    inbuf[4] = ' ' + chan;
	    // always encode as big endian
	    for (i = 12; i >= 5; i--) {
		inbuf[i] = ' ' + (retval & 0xF);
		retval = retval >> 8;
	    }

	    VictimWrite(inbuf, 13);

	    buf += 5;
	    len -= 5;
	    continue;
	}

	/* S -- speed setting command, 4-bit encoding*/
	else if ('S' == buf[0]) {
	    if (len < (5 + pktlen)) {
		Message("Short packet: %d %d\n", len, pktlen);
		break;
	    }

	    if (debug) {
		Message("ProcessPackets, got 'S', chan %d %*.*s", chan, len, len, buf);
	    }
	    buf += 5;
	    len -= 5;
	    victim->setSpeed(victim, buf, pktlen);

	    buf += pktlen;
	    len -= pktlen;
	    continue;
	}

	/* $ -- exit command */
	else if ('$' == buf[0]) {
	    Fatal("received termination request", buf[0]);
	    continue;
	}

      stray:
	if (debug)
	    Message("Stray char (%d): %c", line, buf[0]);
	StreamWrite(0, buf, 1, 0);
	++buf;
	--len;
	continue;
    }

    return (len);
}

void
sighandler(int sig)
{
    int x;
    Message("Cleaning up");
    for (x = 0; x < last_channel; ++x) {
	struct iochan *ic = &channels[x];
	close(ic->fd);
    }
    exit(0);
}



const char match[]="***thinwire***";
#define STARTERLEN 14

int main(int argc, char **argv)
{
    int leftover, len;
    char buf[5 + (2*MAX_PACKET_LEN)];
    int matchlen = 0;
    sigset_t set;
    ParseCommandLine(argc, argv);

    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    sigprocmask(SIG_BLOCK ,&set, NULL);
    signal(SIGINT, sighandler);

    VictimConnect(victim_name, speed1);

    leftover = 0;

    // For protocol version 2, dump everything to channel 0 until
    // the sentinel string is found.
    Message("Using speeds: %d %d", speed1, speed2);
    for (;;) {
	int x = 0;
	int ret;


	check_channel(victim, 0);
	if (! (streams[0]->status & STREAM_READY)) {
	    check_channel(streams[0], 0);
	}

	if (victim->status & POLLIN) {
	    len = VictimRead(buf, sizeof(buf));
	    while (x < len) {
		if (buf[x] == match[matchlen] ||
		    ((matchlen == 0 || matchlen == STARTERLEN - 1)
		     && buf[x] == 0)) {
		    ++matchlen;
		    if (matchlen == STARTERLEN) {
			++x;
			break;
		    }
		} else {
		    matchlen = 0;
		}
		++x;
	    }
	    StreamWrite(0, buf, x, 0);
	    if (matchlen == STARTERLEN) {
		leftover = len - x;
		memmove(buf, buf+x, leftover);
		Message("Got initialization string. %d %d %d",
			leftover, len, x);
		break;
	    }
	} else if ((streams[0]->status & STREAM_READY) &&
	    (streams[0]->status & POLLIN)) {
	    ret = StreamRead(0, buf, MAX_PACKET_LEN, 1);
	    if (ret == 0) {
		Fatal("Lost stream 0. Exiting\n");
	    }

#if 1
	    {
		// Perform \n -> \n\r conversion on stdin input
		int x = 0;
		int y = 0;
		while (x < ret) {
		    if (buf[x] == '\n') {
			VictimWrite(buf + y, x - y);
			VictimWrite("\n\r",2);
			++x;
			y = x;
		    } else {
			++x;
		    }
		}
		VictimWrite(buf + y, x - y);
	    }
#else
	    VictimWrite(buf, ret);
#endif
	} else {
	    check_all_channels(-1);
	}
    }

    buf[leftover] = 0;
    if (leftover) {
	leftover = ProcessPackets(buf, leftover);
    }
    for (;;) {
	len = leftover + VictimRead(buf + leftover, sizeof(buf) - leftover);

	leftover = ProcessPackets(buf, len);

	if ((leftover > 0) && (leftover < len)) {
	    memmove(buf, buf + len - leftover, leftover);
	}
    }
}

/*
 * Local variables:
 *  c-file-style: "cc-mode"
 * End:
 */
