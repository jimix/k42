/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: thinwire.c,v 1.55 2005/02/19 16:19:27 mostrows Exp $
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
int thinwire_ver = 1;
int stdout_channel = -1;
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

/*
 * These following defines need to be identical to their counterparts
 * in vhype/lib/thinwire.c and vtty.h
 */
#define CHAN_SET_SIZE 8
#define MAX_CHANNELS 95
#define MAX_PACKET_LEN (4096*4)

#define STDOUT_MARKER -1

#define	TYPE_TCP	0
#define	TYPE_UXSOCK	1
#define	TYPE_SERIAL	2
#define	TYPE_PSEUDOTTY	3

int verbose = 0;
int debug = 0;
char *program_name;
char *victim;
int victim_fd;
int victim_type;
int nchannels;

struct {
    int port;
    int listen_socket;
    int input;
    int output;
} channel[MAX_CHANNELS];
void VictimWrite(char *buf, int len);


int bitClear(int fd, int bit)
{
    int ret;
    if (victim_type != TYPE_SERIAL) return 0;

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
    if (victim_type != TYPE_SERIAL) return 0;

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
    if (victim_type != TYPE_SERIAL) return 0;

    ret = ioctl(fd, TIOCMGET, &lastStatus);
    if (ret<0) {
        perror("ioctl(TIOCMGET): ");
        return 0;
    }
    return lastStatus & bit;
}


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
    int i;

    va_start(ap, msg);
    vsprintf(buf, msg, ap);
    va_end(ap);

    fprintf(stderr, "%s: %s.\n", program_name, buf);

    shutdown(victim_fd, 2);
    close(victim_fd);
    for (i = 0; i < nchannels; i++) {
	if (channel[i].listen_socket >= 0) {
	    shutdown(channel[i].listen_socket, 2);
	    close(channel[i].listen_socket);
	}
	if (channel[i].input >= 0) {
	    shutdown(channel[i].input, 2);
	    close(channel[i].input);
	}
    }

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

void Usage(void)
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

void ParseCommandLine(int argc, char **argv)
{
    program_name = argv[0];

    if (strstr(program_name,"thinwire2")) {
	thinwire_ver = 2;
    }

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

    victim = argv[0];
    nchannels = 0;
    argv++; argc--;

    for (nchannels = 0; nchannels < MAX_CHANNELS; ++nchannels) {
	channel[nchannels].port = 0;
	channel[nchannels].listen_socket = -1;
	channel[nchannels].input = -1;
	channel[nchannels].output = -1;
    }
    nchannels = 0;
    while (argc && (nchannels < MAX_CHANNELS)) {
	if (argv[0][0]==':') {
	    do {
		++nchannels;
	    } while (nchannels % CHAN_SET_SIZE);
	} else if (strcmp(argv[0],"stdout")==0) {
	    if (stdout_channel == -1) {
		stdout_channel = nchannels;
		channel[nchannels].input = 0;
	    }
	    channel[nchannels].port = STDOUT_MARKER;
	    channel[nchannels].output= 1;
	    ++nchannels;
	} else {
	    channel[nchannels].port = atoi(argv[0]);
	    ++nchannels;
	}
	argv++; argc--;
    }
    if (argc) {
	Fatal("%d channel maximum, %d specified", MAX_CHANNELS, nchannels+argc);
    }
}


void SetSocketFlag(int socket, int level, int flag)
{
    int tmp = 1;
    if (setsockopt(socket, level, flag, (char *)&tmp, sizeof(tmp)) != 0) {
	Fatal("setsockopt(%d, %d, %d) failed", socket, level, flag);
    }
}

int AcceptConnections(int chan)
{
    int ret = 0;
    struct protoent *protoent;

    if (channel[chan].input < 0 && channel[chan].port > 0) {
	fd_set fds;
	struct timeval tv = { 0, 0};
	FD_ZERO(&fds);
	FD_SET(channel[chan].listen_socket, &fds);

	ret = select(channel[chan].listen_socket+1, &fds, NULL, NULL, &tv);
	if (ret <= 0) return 0;

	channel[chan].input = accept(channel[chan].listen_socket, 0, 0);

	if (channel[chan].input < 0) {
	    Fatal("accept() failed for channel %d", chan);
	}
	channel[chan].output = channel[chan].input;

	protoent = getprotobyname("tcp");
	if (protoent == NULL) {
	    Fatal("getprotobyname(\"tcp\") failed");
	}
	SetSocketFlag(channel[chan].input,
		      protoent->p_proto, TCP_NODELAY);
	Message("accepted connection on channel %d (port %d)",
		chan, channel[chan].port);
	ret = 1;
    }
    return ret;
}

void ConnectionLost(int chan)
{
    Message("connection lost on channel %d (port %d)",
	    chan, channel[chan].port);
    close(channel[chan].input);
    channel[chan].input = -1;
    channel[chan].output = -1;
}

void VictimConnect(int speed)
{
    char *p, *host;
    int port, status;
    struct hostent *hostent;
    struct sockaddr_in sockaddr;
    struct sockaddr unixname;
    struct protoent *protoent;
    struct termios serialstate;

    /*
     * Anything with a ':' in it is interpreted as
     * host:port over a TCP/IP socket.
     */
    p = strrchr(victim, ':');
    if (p != NULL) {
	*p = '\0';
	host = victim;
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

	victim_type = TYPE_TCP;

    	return;
    }

    /*
     * Perhaps it is a Unix domain socket
     */
    victim_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    unixname.sa_family = AF_UNIX;
    strcpy(unixname.sa_data, victim);
    if (connect(victim_fd, &unixname, strlen(unixname.sa_data) +
		sizeof(unixname.sa_family)) >= 0) {
	Message("connected on fd %d", victim_fd);

	victim_type = TYPE_UXSOCK;
    	return;
    }
    close(victim_fd);

    /*
     * Perhaps it is a serial port
     */
    victim_fd = open(victim, O_RDWR|O_NOCTTY);
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
    if (ioctl(victim_fd, TIOCMGET, &status) < 0)
	victim_type = TYPE_PSEUDOTTY;
    else
	victim_type = TYPE_SERIAL;

}

void VictimReConnect(int speed)
{
    struct termios serialstate;

    if (victim_type == TYPE_SERIAL || victim_type == TYPE_PSEUDOTTY) {
	if (tcgetattr(victim_fd, &serialstate) < 0) {
	    Fatal("tcgetattr() failed for victim: %s",victim);
	}
	if (victim_type == TYPE_SERIAL) {
	    bitClear(victim_fd,TIOCM_DTR);
	    if (debug) Message("DTR down\n");
	    /* Wait for DSR to go down */
	    while (checkBit(victim_fd,TIOCM_DSR));
	    if (debug) Message("DSR down\n");
	}

	/*
	 * Baud rate.
	 */

	cfsetospeed(&serialstate, speed);
	cfsetispeed(&serialstate, speed);

	if (tcsetattr(victim_fd, TCSANOW, &serialstate) < 0) {
	    fprintf(stderr, "tcsetattr() failed\n");
	}
	if (victim_type == TYPE_SERIAL) {
	    bitSet(victim_fd,TIOCM_DTR);
	    if (debug) Message("DTR up\n");
	    while (!checkBit(victim_fd,TIOCM_DSR));
	    if (debug) Message("DSR up\n");
	}
    }
}

int VictimRead(char *buf, int len)
{
    int cnt;
    bitSet(victim_fd, TIOCM_RTS);

    cnt = read(victim_fd, buf, len);

    if (debug) {
	Message("VictimRead: %d/%d bytes from fd %d", cnt, len, victim_fd);
	DumpCharHex("raw read: ", buf, cnt);
    }

    if (cnt < 0) {
	Fatal("read() failed for victim");
    }
    if (cnt == 0) {
	Fatal("EOF on read from victim");
    }

    bitClear(victim_fd, TIOCM_RTS);
    return (cnt);
}

void VictimWrite(char *buf, int len)
{
    if (debug) {
	Message("VictimWrite: %d bytes to fd %d", len, victim_fd);
	DumpCharHex("raw write:", buf, len);
    }
    bitClear(victim_fd, TIOCM_RTS);
    if (hw_flowcontrol) {
	while (checkBit(victim_fd, TIOCM_CTS) == 0);
    }
    if (write(victim_fd, buf, len) != len) {
	Fatal("write() failed for victim socket");
    }
}

void ChannelListen(int chan)
{
    struct sockaddr_in sockaddr;
    if (channel[chan].port == 0) {
	return;
    }

    if (channel[chan].port == STDOUT_MARKER) {
	Message("connecting channel %d to stdout", chan);
	return;
    }

    while (1) {
	channel[chan].listen_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (channel[chan].listen_socket < 0) {
	    Fatal("socket() failed for channel %d", chan);
	}

	/* Allow rapid reuse of this port. */
	SetSocketFlag(channel[chan].listen_socket, SOL_SOCKET, SO_REUSEADDR);
#ifndef __linux__
#ifndef PLATFORM_CYGWIN
	SetSocketFlag(channel[chan].listen_socket, SOL_SOCKET, SO_REUSEPORT);
#endif /* #ifndef PLATFORM_CYGWIN */
#endif /* #ifndef __linux__ */

	sockaddr.sin_family = PF_INET;
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	sockaddr.sin_port = htons(channel[chan].port);
	if (bind(channel[chan].listen_socket,
		 (struct sockaddr *) &sockaddr, sizeof(sockaddr)) != 0) {
	    if (errno == EADDRINUSE) {
		close(channel[chan].listen_socket);
		sleep(1);
	    } else {
		Fatal("Bind failed");
	    }
	} else {
	    break;
	}
    }

    if (listen(channel[chan].listen_socket, 4) < 0) {
	Fatal("listen() failed for channel %d", chan);
    }

    channel[chan].input = -1;
    channel[chan].output = -1;
}

void ChannelWrite(int chan, int len, char *buf, int allow_fail)
{
    int n;
    if (channel[chan].output < 0 && channel[chan].listen_socket < 0) {
	return;
    }
    while (len > 0) {
	if (channel[chan].output < 0) {

	    if (!allow_fail)
		Message("awaiting connection on channel %d (port %d)",
			chan, channel[chan].port);

	    while (allow_fail==0  && channel[chan].output < 0) {
		AcceptConnections(chan);
	    }
	    if (channel[chan].output < 0 && allow_fail) return;
	}
	if (verbose) {
	    Display(chan, '>', buf, len);
	}
	if (debug) {
	    Message("ChannelWrite: %d bytes to channel %d fd %d", len, chan,
		    channel[chan].input);
	}

	n = write(channel[chan].output, buf, len);
	if (n < 0) {
	    ConnectionLost(chan);
	} else {
	    buf += n;
	    len -= n;
	}
    }
}

int ChannelRead(int chan, int len, char *buf)
{
    int n;

    n = 0;
    if (channel[chan].input < 0 && channel[chan].listen_socket < 0) {
	return 0;
    }

    while (channel[chan].input < 0) {
	AcceptConnections(chan);
    }

    n = read(channel[chan].input, buf, len);

    if (debug) {
	Message("ChannelRead: %d bytes from channel %d fd %d", len, chan,
		channel[chan].input);
    }

    if (n <= 0) {
	ConnectionLost(chan);
	n = 0;
    }

    if (verbose) {
	Display(chan, '<', buf, n);
    }

    return (n);
}


int DoSelect(int base)
{
    struct timeval timeout;
    fd_set fds;
    int i;
    int retry = 0;
    int maxsock = 0;
    unsigned totalrec;
    unsigned retval = 0;
  restart:
    retry = 0;
    maxsock = 0;
    retry = 0;
    FD_ZERO(&fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    for (i = base; i < base+CHAN_SET_SIZE; i++) {
	int s = -1;
	if (channel[i].input >= 0) {
	    s = channel[i].input;
	} else if (channel[i].listen_socket > 0) {
	    s = channel[i].listen_socket;
	}

	if (s >= 0) {
	    FD_SET(s, &fds);
	    maxsock = MAX(s, maxsock);
	}
    }
    ++maxsock;
    totalrec = select(maxsock, &fds, 0, 0, &timeout);
    if (totalrec == 0) {
	return 0;
    }

    for (i = base; i< base+CHAN_SET_SIZE; i++) {
	int s = channel[i].input;

	if (s >=0) {
	    if (FD_ISSET(s, &fds)) {
		retval |= 1<<(i-base);
	    }
	    continue;
	}

	s = channel[i].listen_socket;
	if (s >= 0 && FD_ISSET(s, &fds)) {
	    if (AcceptConnections(i)) {
		/* got new connection, must restart */
		retry = 1;
	    }
	}
    }

    if (retry) goto restart;

    if (verbose) {
	fprintf(stderr, "   !(%d) %x\n", base/CHAN_SET_SIZE, retval);
    }
    if (debug) {
	Message("DoSelect returning %x", retval);
    }
    return (retval);
}



int ProcessPackets(char *buf, int len)
{
    int pktlen, chan;
    static char inbuf[5 + MAX_PACKET_LEN];

    for (;;) {
	if (len < 5) break;
	pktlen = ((((unsigned char) buf[1]) - ' ') << 12) |
	    ((((unsigned char) buf[2]) - ' ') << 6) |
	    ((((unsigned char) buf[3]) - ' ') << 0);
	if (pktlen > MAX_PACKET_LEN) {
	    while (len>0) {
		Message("packet: '%c'(%02x) '%c'(%02x) '%c'(%02x) "
			"'%c'(%02x) '%c'(%02x)",
			buf[0], buf[0], buf[1], buf[1], buf[2], buf[2],
			buf[3], buf[3], buf[4], buf[4]);
		buf+=5;
		len-=5;
	    }
	    Fatal("packet length (%d) exceeds maximum (%d)",
		  pktlen, MAX_PACKET_LEN);
	}
	chan = buf[4] - ' ';

	/* 0 -- write command */
	if ('0' == buf[0] || '#' == buf[0]) {
	    if (len < (5 + pktlen)) {
		break;
	    }
	    if (debug) {
		Message("ProcessPackets, got '0' chan %d, pktlen %d len %d",
			chan, pktlen, len);
	    }

	    ChannelWrite(chan, pktlen, buf + 5, 0);

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
	}

	/* A -- read command */
	else if ('A' == buf[0]) {
	    if (debug) {
		Message("ProcessPackets, got 'A', chan %d", chan);
	    }
	    pktlen = ChannelRead(chan, pktlen, inbuf + 5);
	    inbuf[0] = '0' + chan;
	    inbuf[1] = ' ' + ((pktlen >> 12) & 0x3f);
	    inbuf[2] = ' ' + ((pktlen >>  6) & 0x3f);
	    inbuf[3] = ' ' + ((pktlen >>  0) & 0x3f);
	    inbuf[4] = chan + ' ';
	    VictimWrite(inbuf, pktlen + 5);
	    buf += 5;
	    len -= 5;
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
	}

	/* $ -- exit command */
	else if ('$' == buf[0]) {
	    Fatal("received termination request", buf[0]);
	}

	/* / -- echo/dump command */
	else if ('/' == buf[0]) {
	    /*
	     * If the victim gets into real trouble, it wants to dump
	     * characters out over the serial line as simply as possible.
	     * A "/   " sequence (slash with three spaces for the length)
	     * puts us into "echo" mode where we simply read characters
	     * from the victim and print them.  We currently have no
	     * termination sequence for echo mode.
	     */
	    Message("received a \"/\" from victim.  Entering \"echo\" mode");
	    if (len > 5) {
		fprintf(stdout, "%.*s", len-5, buf+5);
		fflush(stdout);
	    }
	    for (;;) {
		len = VictimRead(inbuf, sizeof(inbuf));
		if (len > 0) {
		    fprintf(stdout, "%.*s", len, inbuf);
		    fflush(stdout);
		}
	    }
	}

	/* unknown command */
	else {
	    int i = 0;
	    for (;i<len; len+=8) {
		Message("buf: '%c'(%x) '%c'(%x) '%c'(%x) '%c'(%x) "
			"'%c'(%x) '%c'(%x) '%c'(%x) '%c'(%x)",
			buf[i],buf[i],
			buf[i+1],buf[i+1],
			buf[i+2],buf[i+2],
			buf[i+3],buf[i+3],
			buf[i+4],buf[i+4],
			buf[i+5],buf[i+5],
			buf[i+6],buf[i+6],
			buf[i+7],buf[i+7]);
	    }

	    Fatal("unknown channel indicator '%c' %x", buf[0], buf[0]);
	}
    }

    return (len);
}

const char match[]="\000**thinwire**\000";
#define STARTERLEN 14

int main(int argc, char **argv)
{
    int chan, leftover, len;
    char buf[5 + (2*MAX_PACKET_LEN)];
    int matchlen = 0;
    ParseCommandLine(argc, argv);

    // Version 2 specifies a different initial speed
    if (thinwire_ver==2) {
	VictimConnect(speed1);
    } else {
	VictimConnect(speed2);
    }
    Message("Version: %d\n",thinwire_ver);
    for (chan = 0; chan < nchannels; chan++) {
	ChannelListen(chan);
    }

    leftover = 0;

    // For protocol version 2, dump everything to channel 0 until
    // the sentinel string is found.
    if (thinwire_ver==2) {
	Message("Using speeds: %d %d", speed1, speed2);
	for (;;) {
	    int x = 0;
	    int max_sock = 0;
	    int ret;

	    fd_set fds;
	    FD_ZERO(&fds);
	    if (channel[0].input >= 0) {
		FD_SET(channel[0].input,&fds);
		max_sock = MAX(channel[0].input, max_sock);
	    } else if (channel[0].listen_socket >= 0) {
		FD_SET(channel[0].listen_socket,&fds);
		max_sock = MAX(channel[0].listen_socket, max_sock);
	    }

	    /* Allow other side to write to us, those making it readable */
	    bitSet(victim_fd, TIOCM_RTS);

	    max_sock = MAX(victim_fd, max_sock);
	    FD_SET(victim_fd, &fds);
	    ++max_sock;
	    ret = select(max_sock, &fds, NULL, NULL, NULL);

	    if (FD_ISSET(victim_fd, &fds)) {
		len = VictimRead(buf, sizeof(buf));
		while (x < len) {
		    if (buf[x] == match[matchlen]) {
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
		ChannelWrite(0, x, buf, 1);
		if (matchlen == STARTERLEN) {
		    leftover = len - x;
		    memmove(buf, buf+x, leftover);
		    Message("Got initialization string. %d %d %d", leftover, len, x);
		    break;
		}
	    }

            if (channel[0].input >= 0 && FD_ISSET(channel[0].input, &fds)) {
                ret = ChannelRead(0, MAX_PACKET_LEN, buf);
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
            } else if (channel[0].port != STDOUT_MARKER &&
		       FD_ISSET(channel[0].listen_socket, &fds)) {
		AcceptConnections(0);
	    }
	}
	if (speed2 != speed1) {
	    VictimReConnect(speed2);
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
