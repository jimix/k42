/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simip.c,v 1.85 2005/04/28 17:40:14 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: simulates basic TCP/UDP/IP services by
 * cut-through to posix interface: this is the cut-through side
 * **************************************************************************/

/*
 * Get ThinIP File System version of openargs  - must be first
 * there's got to be a better way?
 * this gobldygood includes the file whose name is the value
 * of the preprocessor variable LINUXARGS - which is set on the compile line
 */

#define COMPAT_43 // compatibility for 4.3 in AIX to get common sockaddr

#ifdef __APPLE__ /* OS X.. Maybe even BSD? */
#define _BSD_SOCKLEN_T_ int
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory.h>
#include <ctype.h>
#include <math.h>
#include <fcntl.h>
#include <setjmp.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>

#include <sys/times.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#ifndef TCP_NODELAY
#include <tiuser.h>	// needed on AIX to get TCP_NODELAY defined
#endif /* #ifndef TCP_NODELAY */

#include "simip.h"

static inline void mem_htonl(void *p, unsigned sz)
{
    uval32 *v = (uval32 *)p;
    uval32 i;

    /* If your machine is naturally network byte order then this whole
     * thing should get optimized away.
     * If it is not then (shame on you) and hey this can;t be too bad. */
    if (0x1234 != htons(0x1234)) {
	for (i = 0; i < (sz / sizeof (*v)); i++) {
	    v[i] = htonl(v[i]);
	}
    }
}

int commSocket;				// socket used for normal communication
int pollSocket;				// socket used for polling on ready soc
char *program_name;			// name of this program

// FIXME: this never gets reduced
int maxFDSet;
fd_set fullFDSet;		// sockets we currently have to block on

enum {ISBUF, ISWORDS};

void
blockedRead(int sock, void *ptr, int length, int type)
{
    char *p = (char *)ptr;
    int l = length;
    int i;
    // fprintf(stderr, "blockedRead: %d bytes\n", length);
    while (l > 0) {
	if ((i = read(sock, p, l)) == -1) {
	    fprintf(stderr, "read failed\n");
	    shutdown(sock,2);
	    close(sock);
	    exit(-1);
	}
	l -= i;
	p += i;
    }
    if (type == ISWORDS) {
	mem_htonl(ptr, length);
    }
}

void
blockedWrite(int sock, void *ptr, int length, int type)
{
    void *p = ptr;
    int l = length;
    
    int i;
    // fprintf(stderr, "blockedWrite: %d bytes\n", length);
    if (type == ISWORDS) {
	mem_htonl(ptr, length);
    }
    if ((i = write(sock, (void *)p, l)) != l) {
	fprintf(stderr, "write failed\n");
	shutdown(sock,2);
	close(sock);
	exit(-1);
    }
    /* need to change it back if values are reused */
    if (type == ISWORDS) {
	mem_htonl(ptr, length);
    }
}

void
addSocketToGlobalSelect(int sock)
{
    FD_SET(sock, &fullFDSet);
    if (maxFDSet < sock + 1) {
	maxFDSet = sock + 1;
    }
}

void
tellVictimDataAvailable(char sock)
{
    // write to other port a single character
    blockedWrite(pollSocket, &sock, 1, ISBUF);
    
    // already told about it, so don't bother selecting anymore
    FD_CLR(sock, &fullFDSet);
}

char dataBuf[4096 * 4];

//K42 IOSocket canonical "type" values
#define IOSOCKET_STREAM    1
#define IOSOCKET_DATAGRAM  2

void
emulSocket()
{
    struct simipSocketRequest in;
    struct simipSocketResponse out;
    int type;
    
    blockedRead(commSocket, &in, sizeof(in), ISWORDS);
    
    //NOTE - All socket "type" values should be passed in the K42
    //       canonical form.
    
    switch(in.type) {
    case IOSOCKET_STREAM:
	type = SOCK_STREAM;
	break;
    case IOSOCKET_DATAGRAM:
	type = SOCK_DGRAM;
	break;
    default:
	type = in.type;
	fprintf(stderr, "unknown socket type %d\n", type);
    }
    
    out.sock = socket(AF_INET, type, 0);
    if (out.sock == -1) {
	fprintf(stderr, "got emulSocket: failed.\n");
#if 0
	out.rc = -1;
	out.errnum = errno;
#endif /* #if 0 */
    } else {
	int tmp;

	if (type == SOCK_DGRAM) {
	    addSocketToGlobalSelect(out.sock);
	}
	
	// FIXME - remove this kludge when the client can set socket options
	//         in a first class way.
	// Allow rapid reuse of the port for this socket.
	tmp = 1;
	setsockopt(out.sock, SOL_SOCKET, SO_REUSEADDR,
		   (char*)&tmp, sizeof(tmp));
#ifdef SO_REUSEPORT
	setsockopt(out.sock, SOL_SOCKET, SO_REUSEPORT,
		   (char*)&tmp, sizeof(tmp));
#endif /* #ifdef SO_REUSEPORT */
	fprintf(stderr, "got emulSocket %d\n", out.sock);
    }
    
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
}

void
emulClose()
{
    struct simipCloseRequest in;
    struct simipCloseResponse out;
    
    blockedRead(commSocket, &in, sizeof(in), ISWORDS);
    
    out.rc = close(in.sock);
    if (out.rc == -1) {
	out.errnum = errno;
	fprintf(stderr, "simip: close(%d) failed [%d]: %s\n",
		in.sock, errno, strerror(errno));
    } else {
	out.errnum = 0;
	FD_CLR(in.sock, &fullFDSet);
	fprintf(stderr, "simip: close fd %d\n", in.sock);
    }
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
}

void
emulBind()
{
    struct sockaddr_in sockaddr;
    struct simipBindRequest in;
    struct simipBindResponse out;
    
    blockedRead(commSocket, &in, sizeof(in), ISWORDS);
    
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(in.port);
    
    // FIXME: when in.addr is always assigned properly
    if (in.addr) {
	sockaddr.sin_addr.s_addr = htonl(in.addr);
    } else {
	inet_aton("0", &sockaddr.sin_addr);	/* INADDR_ANY */
    }
    
    out.rc = bind(in.sock, (struct sockaddr *) &sockaddr,
		  sizeof(sockaddr));
    if (out.rc == -1) {
	out.errnum = errno;
	fprintf(stderr, "simip: bind(%d) to port %d failed [%d]: %s\n",
		in.sock, in.port, errno, strerror(errno));
    }
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
}

void
emulListen()
{
    struct simipListenRequest in;
    struct simipListenResponse out;
    
    blockedRead(commSocket, &in, sizeof(in), ISWORDS);
    
    out.rc = listen(in.sock, in.backlog);
    
    if (out.rc == -1) {
	out.errnum = errno;
    } else {
	addSocketToGlobalSelect(in.sock);
    }
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
}

void
emulAccept()
{
    struct simipAcceptRequest in;
    struct simipAcceptResponse out;
    fd_set fds;
    int res;
    struct timeval timeout;
    
    FD_ZERO(&fds);
    
   blockedRead(commSocket, &in, sizeof(in), ISWORDS);
    
    FD_SET(in.sock, &fds);
    
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    do {
	res = select(in.sock + 1, &fds, 0, 0, &timeout);
    } while (res == -1 && errno == EINTR);
    
    if (res == -1) {
	fprintf(stderr, "simip: emulAccept(): select() failed: %s\n",
		strerror(errno));
	out.rc = -1;
	out.block = 0;
	out.errnum = errno;
    } else if (res == 0) {
	out.rc = -1;
	out.block = 1;
	out.errnum = EWOULDBLOCK;
    } else {
	out.block = 0;
	out.rc = accept(in.sock, 0, 0);
	if (out.rc == -1) {
	    out.errnum = errno;
	} else {
	    struct protoent *protoent;
	    addSocketToGlobalSelect(out.rc);
	    // FIXME - remove this kludge when the client can set
	    // socket options in a first class way.
	    protoent = getprotobyname("tcp");
	    if (protoent != NULL) {
		int tmp = 1;
		(void) setsockopt(out.rc, protoent->p_proto, TCP_NODELAY,
				  (char *) &tmp, sizeof(tmp));
	    }
	}
	
	FD_SET(in.sock, &fds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	
	do {
	    res = select(in.sock + 1, &fds, 0, 0, &timeout);
	} while (res == -1 && errno == EINTR);
	
	if (res == -1) {
	    fprintf(stderr, "simip: emulRead(): select() failed: %s\n",
		    strerror(errno));
	    out.rc = -1;
	    out.block = 0;
	    out.errnum = errno;
	} else if (res > 0) {
	    out.available = 1;
	} else {
	    out.available = 0;
	    addSocketToGlobalSelect(in.sock);
	}
    }
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
}

void
emulRead()
{
    struct simipReadRequest in;
    struct simipReadResponse out;
    fd_set fds;
    int res;
    struct timeval timeout;
    FD_ZERO(&fds);
    
    blockedRead(commSocket, &in,  sizeof(in), ISWORDS);
    
    FD_SET(in.sock, &fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    do {
	res = select(in.sock+1, &fds, 0, 0, &timeout);
    } while (res == -1 && errno == EINTR);
    
    if (res == -1) {
	fprintf(stderr, "simip: emulRead(): select() failed: %s\n",
		strerror(errno));
	out.block = 0;
	out.errnum = errno;
	blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
	return;
    } else if (res == 0) {
	out.nbytes = 0;
	out.block = 1;
	out.errnum = EWOULDBLOCK;
	fprintf(stderr, "nothing available for read\n");
	blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
	return;
    }
    
    out.nbytes = read(in.sock, dataBuf, in.nbytes);
    if(out.nbytes == -1) {
	out.errnum = errno;
    } else {
#if 0
	fprintf(stderr, "simip: read fd %d succeeded, got nbytes %d\n",
		in.fd, out.nbytes);
#endif /* #if 0 */
	FD_SET(in.sock, &fds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	
	// check for more...
	do {
	    res = select(in.sock+1, &fds, 0, 0, &timeout);
	} while (res == -1 && errno == EINTR);
	
	if (res == -1) {
	    fprintf(stderr, "simip: emulRead(): select() failed: %s\n",
		    strerror(errno));
	    out.errnum = errno;
	} else if (res > 0) {
	    out.available = 1;
	} else {
	    // at this time there is no more
	    addSocketToGlobalSelect(in.sock);
	    out.available = 0;
	}
    }
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
    if (out.nbytes > 0) {
	blockedWrite(commSocket, dataBuf, out.nbytes, ISBUF);
    }
}

void
emulWrite()
{
    struct simipWriteRequest in;
    struct simipWriteResponse out;
    
    blockedRead(commSocket, &in, sizeof(in), ISWORDS);
    blockedRead(commSocket, dataBuf, in.nbytes, ISBUF);
    
    out.nbytes = write(in.sock, dataBuf, in.nbytes);
    if(out.nbytes == -1) {
	out.errnum = errno;
	fprintf(stderr, "simip: write fd %d failed errno %d\n",
		in.sock, errno);
    }
#if 0
    else {
	fprintf(stderr, "simip: write fd %d len %d\n", in.fd, out.nbytes);
    }
#endif /* #if 0 */
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
}

void
emulSendto()
{
    struct sockaddr_in to;
    struct simipSendtoRequest in;
    struct simipSendtoResponse out;
    
    blockedRead(commSocket, &in, sizeof(in), ISWORDS);
    blockedRead(commSocket, dataBuf, in.nbytes, ISBUF);
    
    to.sin_family = AF_INET;
    to.sin_port = htons(in.port);
    
    // FIXME: when in.addr is always assigned properly
    if (in.addr) {
	to.sin_addr.s_addr = htonl(in.addr);
    } else {
	inet_aton("127.1", &to.sin_addr);	/* localhost */
    }
    
    out.nbytes = sendto(in.sock, dataBuf, in.nbytes, 0,
			(struct sockaddr*)&to, sizeof(to));
    if(out.nbytes == -1) {
	out.errnum = errno;
	fprintf(stderr, "simip: sendto fd %d failed errno %d\n",
		in.sock, errno);
    }
#if 0
    else {
	fprintf(stderr, "simip: write fd %d len %d\n", in.fd, out.nbytes);
    }
#endif /* #if 0 */
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
    
#ifdef NOTIFY_ON_SENDTO
    // dd a notification here
    int res;
    struct timeval timeout;
    fd_set fds;
    FD_ZERO(&fds);
    
    FD_SET(in.sock, &fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    do {
	res = select(in.sock + 1, &fds, 0, 0, &timeout);
    } while (res == -1 && errno == EINTR);
    if (res == -1) {
	fprintf(stderr, "simip: emulSendto(): select() failed: %s\n",
		strerror(errno));
    } else if (res > 0) {
	tellVictimDataAvailable(in.sock);
    } else {
	addSocketToGlobalSelect(in.sock);
    }
#endif /* #ifdef NOTIFY_ON_SENDTO */
}

void
emulRecvfrom()
{
    struct simipRecvfromRequest in;
    struct simipRecvfromResponse out;
    fd_set fds;
    int res;
    struct timeval timeout;
    struct sockaddr_in from;
    socklen_t fromlen;
    
    FD_ZERO(&fds);
    
    blockedRead(commSocket, &in,  sizeof(in), ISWORDS);
    
    FD_SET(in.sock, &fds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    do {
	res = select(in.sock + 1, &fds, 0, 0, &timeout);
    } while (res == -1 && errno == EINTR);
    
    if (res == -1) {
	fprintf(stderr, "simip: emulRecvfrom(): select() failed: %s\n",
		strerror(errno));
	out.block = 0;
	out.errnum = errno;
	blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
	return;
    } else if (res == 0) {
	out.nbytes = 0;
	out.block = 1;
	out.errnum = EWOULDBLOCK;
	fprintf(stderr, "nothing available for recvfrom\n");
	blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
	return;
    }
    
    fromlen = sizeof(from);
    memset(&from, 0, sizeof(from));
    
    out.nbytes = recvfrom(in.sock, dataBuf, in.nbytes, 0,
			  (struct sockaddr*)&from, &fromlen);
    if(out.nbytes == -1) {
	out.errnum = errno;
    } else {
#if 0
	fprintf(stderr, "simip: recvfrom fd %d succeeded, got nbytes %d\n",
		in.sock, out.nbytes);
#endif /* #if 0 */
	out.port = ntohs(from.sin_port);
	out.addr = ntohl(from.sin_addr.s_addr);
	
	// check for more
	FD_SET(in.sock, &fds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	do {
	    res = select(in.sock+1, &fds, 0, 0, &timeout);
	} while (res == -1 && errno == EINTR);
	if (res == -1) {
	    fprintf(stderr, "simip: emulRecvfrom(): select() failed: %s\n",
		    strerror(errno));
	    out.block = 0;
	    out.errnum = errno;
	} else if (res > 0) {
	    out.available = 1;
	} else {
	    addSocketToGlobalSelect(in.sock);
	    out.available = 0;
	}
    }
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
    if (out.nbytes > 0) {
	blockedWrite(commSocket, dataBuf, out.nbytes, ISBUF);
    }
}

void
emulConnect()
{
    struct sockaddr_in sockaddr;
    struct simipConnectRequest in;
    struct simipConnectResponse out;
    
    blockedRead(commSocket, &in, sizeof(in), ISWORDS);
    
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(in.port);
    sockaddr.sin_addr.s_addr = htonl(in.addr);
    
    out.rc = connect(in.sock, (struct sockaddr *) &sockaddr,
		     sizeof(sockaddr));
    if (out.rc == -1) {
	out.errnum = errno;
    } else {
	addSocketToGlobalSelect(in.sock);
    }
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
}

void
emulGetEnvVar()
{
    struct simipGetEnvVarRequest in;
    struct simipGetEnvVarResponse out;
    char *envVarName;
    char *envVarValue;
    
    blockedRead(commSocket, &in, sizeof(in), ISBUF);
    
    envVarName = in.envVarName;
    envVarValue = getenv(envVarName);
    if (envVarValue == NULL) {
	envVarValue = "";
    }
    if (strlen(envVarValue) >= sizeof(out.envVarValue)) {
	fprintf(stderr,
		"\n%s: getenv(\"%s\") is to large for transport (%lu chars)\n"
		"\t exiting!!\n",
		program_name, envVarName,
		(unsigned long)sizeof(out.envVarValue));
	kill(getppid(), SIGHUP);
	exit (-1);
    }
    strcpy(out.envVarValue, envVarValue);
    
    blockedWrite(commSocket, &out, sizeof(out), ISBUF);
}

void
emulGetTimeOfDay()
{
    struct simipGetTimeOfDayResponse out;
    struct timeval tv;
    
    gettimeofday(&tv, 0);
    out.tv_sec = tv.tv_sec;
    out.tv_usec = tv.tv_usec;
    
    blockedWrite(commSocket, &out, sizeof(out), ISWORDS);
}

void emulGetKParmBlock()
{
    int kparmFd;
    struct stat kparmStat;
    char *data_buffer;
    uval32 data_buffer_size;
    char *kparm_filename = getenv("K42_KPARMS_FILE");

    if (kparm_filename) {
	printf("Using kparm file %s\n", kparm_filename);
    } else {
	printf("K42_KPARMS_FILE is not set (should have been set by "
	       "k42console\n");
    }

    kparmFd = open(kparm_filename, O_RDONLY);
    if (kparmFd < 0) {
	perror("Failed to open kparm.data\nExiting.\n");
	kill(getppid(), SIGHUP);
	exit(-1);
    }

    if (stat(kparm_filename, &kparmStat) < 0) {
	perror("Failed to stat kparm.data\nExiting.\n");
	kill(getppid(), SIGHUP);
	exit(-1);
    }
    
    data_buffer_size = kparmStat.st_size;
    data_buffer = malloc(data_buffer_size);
    if (data_buffer == NULL) {
	fprintf(stderr, "Failed to malloc %i bytes .data\nExiting.\n",
		data_buffer_size);
	kill(getppid(), SIGHUP);
	exit(-1);
    }

    if (read(kparmFd, data_buffer, data_buffer_size) != data_buffer_size)
    {
	perror("Failed to read kparm data file\n");
	kill(getppid(), SIGHUP);
	exit(-1);
    }

    blockedWrite(commSocket, &data_buffer_size, sizeof(data_buffer_size),
		 ISWORDS);
    blockedWrite(commSocket, data_buffer, data_buffer_size, ISBUF);
}

void
usage()
{
    fprintf(stderr, "Usage: %s <host>:<port> <host>:<port>\n",
	    program_name);
    fprintf(stderr, "- first port is for standard communication\n");
    fprintf(stderr, "- second port is for poll requests\n");
    exit(-1);
}

// returns back fd to open socket
int
establishConnection(char *con)
{
    char *p, *hostname = NULL;
    int port = -1;
    struct hostent *hostent;
    struct sockaddr_in sockaddr;
    int tmp, newSocket;
    struct protoent *protoent;
    
    if ((p = strrchr(con, ':')) != NULL) {
	*p = '\0';
	hostname = con;
	port = atoi(p+1);
    }
    hostent = gethostbyname(hostname);
    
    if (!hostent) {
	fprintf(stderr, "%s: host \"%s\" unknown\n", program_name, hostname);
	usage();
    }
    
    fprintf(stderr, "%s: connecting to host \"%s\", port %d.\n",
	    program_name, hostname, port);
    
    while (1) {
	newSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (newSocket < 0) {
	    fprintf(stderr, "%s: socket() failed\n", program_name);
	    exit(-1);
	}
	
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	memcpy(&sockaddr.sin_addr.s_addr, hostent->h_addr,
	       sizeof (struct in_addr));
	
	if (connect(newSocket, (struct sockaddr *) &sockaddr,
		    sizeof(sockaddr)) != 0) {
	    if( errno == ECONNREFUSED ) {
		/* close and retry */
		close(newSocket);
		sleep(1);
	    } else {
		/* fatal error */
		fprintf(stderr, "%s: connecting to victim failed",
			program_name);
		exit(1);
	    }
	} else {
	    // connected
	    break;
	}
    }
    fprintf(stderr, "%s: connected.\n", program_name);
    
    /* Allow rapid reuse of this port. */
    tmp = 1;
    setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp, sizeof(tmp));
#ifdef SO_REUSEPORT
    tmp = 1;
    setsockopt(newSocket, SOL_SOCKET, SO_REUSEPORT, (char *)&tmp, sizeof(tmp));
#endif /* #ifdef SO_REUSEPORT */
    
    /* Enable TCP keep alive process. */
    tmp = 1;
    setsockopt(newSocket, SOL_SOCKET, SO_KEEPALIVE, (char *)&tmp, sizeof(tmp));
    
    protoent = getprotobyname("tcp");
    if (!protoent) {
	fprintf(stderr, "%s: getprotobyname() failed\n", program_name);
	exit(-1);
    }
    
    tmp = 1;
    if (setsockopt(newSocket, protoent->p_proto, TCP_NODELAY,
		   (char *)&tmp, sizeof(tmp)) != 0) {
	fprintf(stderr, "%s: setsockopt() failed\n", program_name);
	exit(-1);
    }
    return newSocket;
}

void
handleCommReq()
{
    char req;
    int cnt = read(commSocket, &req, 1);
    if(cnt <= 0) {
	fprintf(stderr, "read on socket failed, exiting\n");
	shutdown(commSocket,2);
	close(commSocket);
	exit(-1);
    }
    switch(req) {
    case SIMIP_SOCKET:
	emulSocket();
	break;
    case SIMIP_CLOSE:
	emulClose();
	break;
    case SIMIP_BIND:
	emulBind();
	break;
    case SIMIP_LISTEN:
	emulListen();
	break;
    case SIMIP_ACCEPT:
	emulAccept();
	break;
    case SIMIP_READ:
	emulRead();
	break;
    case SIMIP_WRITE:
	emulWrite();
	break;
    case SIMIP_SENDTO:
	emulSendto();
	break;
    case SIMIP_RECVFROM:
	emulRecvfrom();
	break;
    case SIMIP_CONNECT:
	emulConnect();
	break;
    case SIMIP_GETENVVAR:
	emulGetEnvVar();
	break;
    case SIMIP_GETTIMEOFDAY:
	emulGetTimeOfDay();
	break;
    case SIMIP_GETKPARM_BLOCK:
	emulGetKParmBlock();
	break;

    case SIMIP_RECV:
	fprintf(stderr, "NYI function SIMIP_RECV\n");
	exit(-1);
    case SIMIP_SEND:
	fprintf(stderr, "NYI function SIMIP_SEND\n");
	exit(-1);
    case SIMIP_IOCTL_FIONREAD:
	fprintf(stderr, "NYI function SIMIP_IOCTL_FIONREAD\n");
	exit(-1);
    case SIMIP_IOCTL_FIONBIO:
	fprintf(stderr, "NYI function SIMIP_IOCTL_FIONBIO\n");
	exit(-1);
    case SIMIP_IOCTL_FL:
	fprintf(stderr, "NYI function SIMIP_IOCTL_FL\n");
	exit(-1);
    case SIMIP_SETSOCKOPT:
	fprintf(stderr, "NYI function SIMIP_SETSOCKOPT\n");
	exit(-1);
    case SIMIP_FCNTL_NONBLOCK:
	fprintf(stderr, "NYI function SIMIP_FCNTL_NONBLOCK\n");
	exit(-1);
    case SIMIP_GET_SOCKNAME:
	fprintf(stderr, "NYI function SIMIP_GET_SOCKNAME\n");
	exit(-1);
    case SIMIP_GET_PEERNAME:
	fprintf(stderr, "NYI function SIMIP_GET_PEERNAME\n");
	exit(-1);
    default:
	fprintf(stderr, "NYI unknown function %d\n", req);
	exit(-1);
    }
}

int
main(int argc, char **argv)
{
    char *con1;
    char *con2;
    int testSock;
    
    FD_ZERO(&fullFDSet);
    
    program_name = argv[0];
    argv++; argc--;
    
    // first guy we connect to
    if (argc == 0) usage();
    con1 = argv[0];
    argv++; argc--;
    
    // second port used for polling
    if (argc == 0) usage();
    con2 = argv[0];
    argv++; argc--;
    
    if (argc != 0) usage();
    
    commSocket = establishConnection(con1);
    FD_SET(commSocket, &fullFDSet);
    maxFDSet = commSocket+1;
    
    pollSocket = establishConnection(con2);
    
    (void) signal(SIGPIPE, SIG_IGN);
    
    while (1) {
	fd_set tmpFDSet;	// sockets we currently have to block on
	int numSock;
	
	memcpy(&tmpFDSet, &fullFDSet, sizeof(fullFDSet));
	numSock = select(maxFDSet, &tmpFDSet, 0, 0, 0);
	
	if (numSock < 0) {
	    if (errno == EINTR) {
		continue;
	    }
	    fprintf(stderr,
		    "simip: select failed (rc %d, errno %d), exiting.\n",
		    numSock, errno);
	    exit(-1);
	}
	if (FD_ISSET(commSocket, &tmpFDSet) ) {
	    handleCommReq();
	    numSock--;
	}
	
	testSock = 0;
	while (numSock > 0) {
	    if ((testSock != commSocket) && FD_ISSET(testSock, &tmpFDSet)) {
		numSock--;
		tellVictimDataAvailable(testSock);
	    }
	    testSock++;
	}
    }
}
