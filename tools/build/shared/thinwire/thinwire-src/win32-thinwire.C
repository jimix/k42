/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: win32-thinwire.C,v 1.3 2000/05/11 11:30:16 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: thinwire (de)multiplexor program
 * **************************************************************************/

#define _BSD		// needed on AIX to get fd_set typedef'd
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <termios.h>

#ifndef TCP_NODELAY
#include <tiuser.h>	// needed on AIX to get TCP_NODELAY defined
#endif

#include <windows.h>

/*
 * These prototypes should be in <termios.h>, but aren't.
 */
extern "C" int tcgetattr(int, struct termios *);
extern "C" int tcsetattr(int, int, const struct termios *);

#define MAX_CHANNELS 10
#define MAX_PACKET_LEN 4096

int verbose = 0;
char *program_name;
char *victim;
int victim_fd = -1;
HANDLE victim_handle = INVALID_HANDLE_VALUE;
int victim_encoded;
int nchannels;
int nconnections;

struct {
    int port;
    int listen_socket;
    int data_socket;
} channel[MAX_CHANNELS];

void Message(char *msg, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, msg);
    vsprintf(buf, msg, ap);
    va_end(ap);

    printf("%s: %s.\n", program_name, buf);
}

void Fatal(char *msg, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, msg);
    vsprintf(buf, msg, ap);
    va_end(ap);

    fprintf(stderr, "%s: %s.\n", program_name, buf);
    exit(-1);
}

void Display(int chan, char direction, char *buf, int len)
{
    int i;
    char c;

    fprintf(stderr, "%2d %c %4d \"", chan, direction, len);
    if (len > 20) len = 20;
    for (i = 0; i < len; i++) {
	c = buf[i];
	if ((c < ' ') || (c > '~')) c = '.';
	putc(c, stderr);
    }
    fprintf(stderr, "\"%*s ", 20 - len, "");
    for (i = 0; i < len; i++) {
	fprintf(stderr, "%02x", ((int) buf[i]) & 0xff);
    }
    putc('\n', stderr);
}

void Usage(void)
{
    fprintf(stderr, "Usage: %s [-verbose] <victim> <channel_port> ...\n",
							    program_name);
    fprintf(stderr, "           where <victim> is <host>:<port> or "
							"<serial_device>\n");
    exit(-1);
}

void ParseCommandLine(int argc, char **argv)
{
    int chan;

    program_name = argv[0];
    argv++; argc--;

    if ((argc > 0) && (strcmp(argv[0], "-verbose") == 0)) {
	verbose = 1;
	argv++; argc--;
    }

    if (argc < 2) {
	Usage();
    }

    victim = argv[0];
    argv++; argc--;

    nchannels = argc;
    nconnections = 0;
    if (nchannels > MAX_CHANNELS) {
	Fatal("%d channel maximum, %d specified", MAX_CHANNELS, nchannels);
    }
    for (chan = 0; chan < nchannels; chan++) {
	channel[chan].port = atoi(argv[chan]);
    }
}

void SetSocketFlag(int socket, int level, int flag)
{
    int tmp = 1;
    if (setsockopt(socket, level, flag, (char *)&tmp, sizeof(tmp)) != 0) {
	Fatal("setsockopt(%d, %d, %d) failed", socket, level, flag);
    }
}

void AcceptConnections(int await_victim)
{
    fd_set fds;
    int maxfd, nfds, chan;
    struct timeval timeout;
    struct protoent *protoent;

    if (victim_fd < 0) {
	await_victim = 0;
    }

    do {
	FD_ZERO(&fds);
	maxfd = 0;
	if (await_victim) {
	    FD_SET(victim_fd, &fds);
	    maxfd = victim_fd;
	}
	if (nconnections < nchannels) {
	    for (chan = 0; chan < nchannels; chan++) {
		if (channel[chan].data_socket < 0) {
		    FD_SET(channel[chan].listen_socket, &fds);
		    if (channel[chan].listen_socket > maxfd) {
			maxfd = channel[chan].listen_socket;
		    }
		}
	    }
	}
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;
	nfds = select(maxfd + 1, &fds, 0, 0, &timeout);
	if (nfds < 0) {
	    Fatal("select() failed");
	}
	if (nconnections < nchannels) {
	    for (chan = 0; chan < nchannels; chan++) {
		if (FD_ISSET(channel[chan].listen_socket, &fds)) {
		    channel[chan].data_socket =
			accept(channel[chan].listen_socket, 0, 0);
		    if (channel[chan].data_socket < 0) {
			Fatal("accept() failed for channel %d", chan);
		    }

		    protoent = getprotobyname("tcp");
		    if (protoent == NULL) {
			Fatal("getprotobyname(\"tcp\") failed");
		    }
		    SetSocketFlag(channel[chan].data_socket,
				    protoent->p_proto, TCP_NODELAY);

		    Message("accepted connection on channel %d (port %d)",
						    chan, channel[chan].port);
		    nconnections++;
		}
	    }
	}
    } while (await_victim && !FD_ISSET(victim_fd, &fds));
}

void ConnectionLost(int chan)
{
    Message("connection lost on channel %d (port %d)",
					chan, channel[chan].port);
    close(channel[chan].data_socket);
    channel[chan].data_socket = -1;
    nconnections--;
}

void VictimConnect(void)
{
    char *p, *host;
    int port;
    struct hostent *hostent;
    struct sockaddr_in sockaddr;
    struct protoent *protoent;
    COMMTIMEOUTS cto;
    DCB dcb;

    p = strrchr(victim, ':');
    if (p != NULL) {
	victim_encoded = 0;
	*p = '\0';
	host = victim;
	port = atoi(p+1);

	hostent = gethostbyname(host);

	if (hostent == NULL) {
	    Fatal("unknown victim host: %s", host);
	}

	victim_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (victim_fd < 0) {
	    Fatal("socket() failed for victim");
	}

	/* Allow rapid reuse of this port. */
	SetSocketFlag(victim_fd, SOL_SOCKET, SO_REUSEADDR);

	/* Enable TCP keep alive process. */
	SetSocketFlag(victim_fd, SOL_SOCKET, SO_KEEPALIVE);

	sockaddr.sin_family = PF_INET;
	sockaddr.sin_port = htons(port);
	memcpy(&sockaddr.sin_addr.s_addr, hostent->h_addr,
						sizeof (struct in_addr));

	Message("connecting to victim (host \"%s\", port %d)", host, port);
	while (connect(victim_fd, (struct sockaddr *) &sockaddr,
						    sizeof(sockaddr)) != 0) {
	    sleep(1);
	}
	Message("connected");

	protoent = getprotobyname("tcp");
	if (protoent == NULL) {
	    Fatal("getprotobyname(\"tcp\") failed");
	}

	SetSocketFlag(victim_fd, protoent->p_proto, TCP_NODELAY);
    } else {
	victim_encoded = 1;
	victim_handle = CreateFile(victim, (GENERIC_READ|GENERIC_WRITE), 0,
					NULL, OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL, 0);
	if (victim_handle == INVALID_HANDLE_VALUE) {
	    Fatal("CreateFile(\"%s\", ...) failed, win32 error %d",
						    victim, GetLastError());
	}

//	if (!SetupComm(victim_handle,
//		(4 + (2*MAX_PACKET_LEN)),
//		(4 + (2*MAX_PACKET_LEN)))) {
//	    Fatal("SetupComm(...) failed, win32 error %d", GetLastError());
//	}
	if (!SetupComm(victim_handle, 1000, 4000)) {
	    Fatal("SetupComm(...) failed, win32 error %d", GetLastError());
	}

	if (!GetCommState(victim_handle, &dcb)) {
	    Fatal("GetCommState(...) failed, win32 error %d",
						    GetLastError());
	}
	printf("DCBlength %d\n", dcb.DCBlength);
        printf("BaudRate %d\n", dcb.BaudRate);
	printf("fBinary %d\n", dcb.fBinary);
	printf("fParity %d\n", dcb.fParity);
	printf("fOutxCtsFlow %d\n", dcb.fOutxCtsFlow);
	printf("fOutxDsrFlow %d\n", dcb.fOutxDsrFlow);
	printf("fDtrControl %d\n", dcb.fDtrControl);
	printf("fDsrSensitivity %d\n", dcb.fDsrSensitivity);
	printf("fTXContinueOnXoff %d\n", dcb.fTXContinueOnXoff);
	printf("fOutX %d\n", dcb.fOutX);
	printf("fInX %d\n", dcb.fInX);
	printf("ErrorChar %d\n", dcb.ErrorChar);
	printf("fNull %d\n", dcb.fNull);
	printf("fRtsControl %d\n", dcb.fRtsControl);
	printf("fAbortOnError %d\n", dcb.fAbortOnError);
	printf("fDummy2 %d\n", dcb.fDummy2);
	printf("wReserved %d\n", dcb.wReserved);
	printf("XonLim %d\n", dcb.XonLim);
	printf("XoffLim %d\n", dcb.XoffLim);
        printf("ByteSize %d\n", dcb.ByteSize);
	printf("Parity %d\n", dcb.Parity);
	printf("StopBits %d\n", dcb.StopBits);
	printf("XonChar %d\n", dcb.XonChar);
	printf("XoffChar %d\n", dcb.XoffChar);
	printf("fErrorChar %d\n", dcb.fErrorChar);
	printf("EofChar %d\n", dcb.EofChar);
	printf("EvtChar %d\n", dcb.EvtChar);
	printf("wReserved1 %d\n", dcb.wReserved1);

	dcb.DCBlength = sizeof(dcb);
        dcb.BaudRate = CBR_38400;
	dcb.fBinary = TRUE;     /* Binary transfer */
	dcb.fParity = FALSE;  /* ignore parity errors */
	dcb.fOutxCtsFlow = FALSE;   /* disable */
	dcb.fOutxDsrFlow = FALSE;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fDsrSensitivity = FALSE;
	dcb.fTXContinueOnXoff = TRUE;
	dcb.fOutX = FALSE;  /* disable */
	dcb.fInX = FALSE;   /* disable */
	dcb.ErrorChar = 0;
	dcb.fNull = FALSE;      /* Don't discard nulls in binary mode */
	dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;      /* enable */
	dcb.fAbortOnError = TRUE;
	dcb.fDummy2 = 0;
	dcb.wReserved = 0;
	dcb.XonLim = 0;
	dcb.XoffLim = 0;
        dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.XonChar = 0x11;
	dcb.XoffChar = 0x13;
	dcb.fErrorChar = FALSE;
	dcb.EofChar = 0;        /* No end-of-data in binary mode */
	dcb.EvtChar = 0;        /* No end-of-data in binary mode */
	dcb.wReserved1 = 0;
	if (!SetCommState(victim_handle, &dcb)) {
	    Fatal("SetCommState(...) failed, win32 error %d",
						    GetLastError());
	}

	if (!GetCommTimeouts(victim_handle, &cto)) {
	    Fatal("GetCommTimeouts(...) failed, win32 error %d",
						    GetLastError());
	}
	printf("ReadIntervalTimeout %d\n", cto.ReadIntervalTimeout);
	printf("ReadTotalTimeoutMultiplier %d\n", cto.ReadTotalTimeoutMultiplier);
	printf("ReadTotalTimeoutConstant %d\n", cto.ReadTotalTimeoutConstant);
	printf("WriteTotalTimeoutMultiplier %d\n", cto.WriteTotalTimeoutMultiplier);
	printf("WriteTotalTimeoutConstant %d\n", cto.WriteTotalTimeoutConstant);

	//cto.ReadIntervalTimeout = MAXDWORD;
	//cto.ReadTotalTimeoutMultiplier = MAXDWORD;
	//cto.ReadTotalTimeoutConstant = (MAXDWORD - 1);
	//cto.WriteTotalTimeoutMultiplier = 0;
	//cto.WriteTotalTimeoutConstant = 0;
	cto.ReadIntervalTimeout = 10;
	cto.ReadTotalTimeoutMultiplier = 0;
	cto.ReadTotalTimeoutConstant = 0;
	cto.WriteTotalTimeoutMultiplier = 0;
	cto.WriteTotalTimeoutConstant = 5000;
	if (!SetCommTimeouts(victim_handle, &cto)) {
	    Fatal("SetCommTimeouts(...) failed, win32 error %d",
						    GetLastError());
	}
    }
}

int VictimRead(char *buf, int len)
{
    int cnt, i, j;
    char c;

#if 0
    AcceptConnections(1);
#endif

    if (victim_fd >= 0) {
	cnt = read(victim_fd, buf, len);
	if (cnt < 0) {
	    Fatal("read() failed for victim");
	}
    } else {
	if (!ReadFile(victim_handle, buf, len, (unsigned int *)&cnt, 0)) {
	    DWORD errors;
	    COMSTAT comstat;
	    printf("ReadFile(...) failed, win32 error %d\n", GetLastError());
	    if (!ClearCommError(victim_handle, &errors, &comstat)) {
		Fatal("ClearCommError(...) failed, win32 error %d",
							    GetLastError());
	    }
	    printf("errors 0x%x\n", errors);
	    printf("comstat.fCtsHold %d\n", comstat.fCtsHold);
	    printf("comstat.fDsrHold %d\n", comstat.fDsrHold);
	    printf("comstat.fRlsdHold %d\n", comstat.fRlsdHold);
	    printf("comstat.fXoffHold %d\n", comstat.fXoffHold);
	    printf("comstat.fXoffSent %d\n", comstat.fXoffSent);
	    printf("comstat.fEof %d\n", comstat.fEof);
	    printf("comstat.fTxim %d\n", comstat.fTxim);
	    printf("comstat.cbInQue %d\n", comstat.cbInQue);
	    printf("comstat.cbOutQue %d\n", comstat.cbOutQue);
	    Fatal("");
	}
    }

    if (victim_encoded) {
	i = 0;
	j = 0;
	while (i < cnt) {
	    c = buf[i++];
	    if (c == '\033') {
		if (i < cnt) {
		    c = buf[i++];
		} else {
		    // We need one more character.
		    if (read(victim_fd, &c, 1) != 1) {
			Fatal("read() failed for victim");
		    }
		}
		c &= 0x7f;
	    }
	    buf[j++] = c;
	}
	cnt = j;
    }

    return cnt;
}

void VictimWrite(char *buf, int len)
{
    int i, j;
    char c;
    char newbuf[4 + (2*MAX_PACKET_LEN)];

    if (victim_encoded) {
	j = 0;
	for (i = 0; i < len; i++) {
	    c = buf[i];
	    if ((c == '\033') || (c == '\023') || (c == '\021')) {
		newbuf[j++] = '\033';
		c |= 0x80;
	    }
	    newbuf[j++] = c;
	}
	buf = newbuf;
	len = j;
    }

    if (victim_fd >= 0) {
	if (write(victim_fd, buf, len) != len) {
	    Fatal("write() failed for victim socket");
	}
    } else {
	int cnt;
	if (!WriteFile(victim_handle, buf, len, (unsigned int *)&cnt, 0)) {
	    Fatal("WriteFile(...) failed, win32 error %d", GetLastError());
	}
	if (cnt != len) {
	    Fatal("WriteFile(...), %d bytes requested, %d bytes written",
								    len, cnt);
	}
    }
}

void ChannelListen(int chan)
{
    struct sockaddr_in sockaddr;

    channel[chan].listen_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (channel[chan].listen_socket < 0) {
	Fatal("socket() failed for channel %d", chan);
    }

    sockaddr.sin_family = PF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(channel[chan].port);
    if (bind(channel[chan].listen_socket,
		(struct sockaddr *) &sockaddr, sizeof(sockaddr)) != 0) {
	Fatal("bind() failed for channel %d", chan);
    }

    /* Allow rapid reuse of this port. */
    SetSocketFlag(channel[chan].listen_socket, SOL_SOCKET, SO_REUSEADDR);

    if (listen(channel[chan].listen_socket, 4) < 0) {
	Fatal("listen() failed for channel %d", chan);
    }

    channel[chan].data_socket = -1;
}

void ChannelWrite(int chan, int len, char *buf)
{
    int n;

    while (len > 0) {
	if (channel[chan].data_socket < 0) {
	    Message("awaiting connection on channel %d (port %d)",
						chan, channel[chan].port);
	    do {
		AcceptConnections(0);
	    } while (channel[chan].data_socket < 0);
	}
	if (verbose) {
	    Display(chan, '>', buf, len);
	}
	n = write(channel[chan].data_socket, buf, len);
	char dummy[1];
	(void) read(channel[chan].data_socket, dummy, 1);
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
    fd_set fds;
    struct timeval timeout;

    n = 0;
    if (channel[chan].data_socket >= 0) {
	FD_ZERO(&fds);
	FD_SET(channel[chan].data_socket, &fds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;
	if (select(channel[chan].data_socket + 1,
				    &fds, 0, 0, &timeout) == 1) {
	    n = read(channel[chan].data_socket, buf, len);
	    if (n < 0) {
		ConnectionLost(chan);
		n = 0;
	    }
	}
    }

    if (verbose) {
	Display(chan, '<', buf, n);
    }

    return n;
}

int ProcessPackets(char *buf, int len)
{
    int pktlen, chan;
    char inbuf[4 + MAX_PACKET_LEN];

    for (;;) {
	if (len < 4) break;
	pktlen = ((((unsigned char) buf[1]) - ' ') << 12) |
		    ((((unsigned char) buf[2]) - ' ') << 6) |
			((((unsigned char) buf[3]) - ' ') << 0);
	if (pktlen > MAX_PACKET_LEN) {
	    Fatal("packet length (%d) exceeds maximum (%d)",
						pktlen, MAX_PACKET_LEN);
	}
	if (('0' <= buf[0]) && (buf[0] < ('0' + MAX_CHANNELS))) {
	    chan = buf[0] - '0';
	    if (len < (4 + pktlen)) break;
	    ChannelWrite(chan, pktlen, buf + 4);
	    //usleep(10000);
	    buf += (4 + pktlen);
	    len -= (4 + pktlen);
	    inbuf[0] = 'A' + chan;
	    inbuf[1] = ' ' + ((pktlen >> 12) & 0x3f);
	    inbuf[2] = ' ' + ((pktlen >>  6) & 0x3f);
	    inbuf[3] = ' ' + ((pktlen >>  0) & 0x3f);
	    VictimWrite(inbuf, 4);
	} else if (('A' <= buf[0]) && (buf[0] < ('A' + MAX_CHANNELS))) {
	    chan = buf[0] - 'A';
	    pktlen = ChannelRead(chan, pktlen, inbuf + 4);
	    inbuf[0] = '0' + chan;
	    inbuf[1] = ' ' + ((pktlen >> 12) & 0x3f);
	    inbuf[2] = ' ' + ((pktlen >>  6) & 0x3f);
	    inbuf[3] = ' ' + ((pktlen >>  0) & 0x3f);
	    VictimWrite(inbuf, pktlen + 4);
	    buf += 4;
	    len -= 4;
	} else {
	    Fatal("unknown channel indicator '%c'", buf[0]);
	}
    }

    return len;
}

int main(int argc, char **argv)
{
    int chan, leftover, len;
    char buf[4 + (2*MAX_PACKET_LEN)];

    ParseCommandLine(argc, argv);

    VictimConnect();

    for (chan = 0; chan < nchannels; chan++) {
	ChannelListen(chan);
    }

    leftover = 0;
    for (;;) {
	len = leftover + VictimRead(buf + leftover, sizeof(buf) - leftover);

	leftover = ProcessPackets(buf, len);

	if ((leftover > 0) && (leftover < len)) {
	    memmove(buf, buf + len - leftover, leftover);
	}
    }
}
