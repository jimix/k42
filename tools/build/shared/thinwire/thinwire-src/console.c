/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: console.c,v 1.14 2004/08/04 14:47:06 jimix Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: console program used with thinwire connection
 * **************************************************************************/

/*
 * This code is a standalone console program for use with
 * thinwire.  The main control flow (in main() below), is to get
 * a socket connection to thinwire, adjust the tty/display
 * settings, and then simply copy bytes to/from the terminal and
 * thinwire.  Most of our time is spent in copyBytes.  When we
 * quit (when the thinwire socket goes away), we reset the TTY
 * back to what it was before we changed it.

 * copyBytes copies bytes from the thinwire server to the
 * display, and from the display/keyboard to the thinwire server.
 * So it waits until there is input available on either of its
 * two inputs (fd 0 is stdin, or the thinwire socket), and then
 * reads from one and writes that data to the other.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>     // strrchr()
#include <strings.h>    // bzero()
#include <termios.h>    // tcgetattr()
#include <sys/time.h>   // FD_ZERO, ...

#include <sys/socket.h> // socket(), setsockopt(), ...
#include <netdb.h>	// gethostbyname(), getprotobyname()
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#ifndef TCP_NODELAY
#include <tiuser.h>	// needed on AIX to get TCP_NODELAY defined
#endif /* #ifndef TCP_NODELAY */


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* Variables and options which control this execution */

char *program_name;
int noraw = 0;  /* option: -noraw .. do not put display in raw mode */

#define DEFAULT_CONSOLE_HOST "localhost"
#define DEFAULT_CONSOLE_PORT 2102

char *hostname = NULL;
int port = DEFAULT_CONSOLE_PORT;

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

int scanArgs(int argc, char **argv)
{
    char *p;

    program_name = argv[0];
    argv++; argc--;

    noraw = 0;
    if ((argc > 0) && (strcmp(argv[0], "-noraw") == 0)) {
	noraw = 1;
	argv++; argc--;
    }

    hostname = DEFAULT_CONSOLE_HOST;
    port = DEFAULT_CONSOLE_PORT;

    /* if we have a parameter, it can be "host:port", "host", or ":port" */
    if (argc > 0) {
	if ((p = strrchr(argv[0], ':')) != NULL) {
	    *p = '\0';
	    if (*(argv[0]) != '\0')
		hostname = argv[0];
	    port = atoi(p+1);
	    argv++; argc--;
	} else {
	    hostname = argv[0];
	}
    }

#ifdef DEBUG
    fprintf(stderr, "scanArgs:  host = \"%s\", port = %d, noraw = %d\n",
	    hostname, port, noraw);
#endif /* #ifdef DEBUG */

    if (argc != 0) {
	fprintf(stderr, "Usage: %s [-noraw] [<host>:<port>]\n", program_name);
	return (-1);
    }
    return (0);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

int getSocket(char *hostname)
{
    struct hostent *hostent;
    int socket_fd;
    int tmp;
    struct sockaddr_in sockaddr;
    struct protoent *protoent;

    hostent = gethostbyname(hostname);

    if (!hostent) {
	fprintf(stderr, "%s: host \"%s\" unknown\n", program_name, hostname);
	return (-1);
    }

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
	fprintf(stderr, "%s: socket() failed\n", program_name);
	return (-1);
    }

    /* Allow rapid reuse of this port. */
    tmp = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp, sizeof(tmp));

    /* Enable TCP keep alive process. */
    tmp = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&tmp, sizeof(tmp));

    sockaddr.sin_family = PF_INET;
    sockaddr.sin_port = htons(port);
    memcpy(&sockaddr.sin_addr.s_addr, hostent->h_addr,
	   sizeof (struct in_addr));

#ifdef DEBUG
    fprintf(stderr, "%s: connecting to host \"%s\", port %d.\n",
	    program_name, hostname, port);
#endif /* #ifdef DEBUG */

    while (connect(socket_fd, (struct sockaddr *) &sockaddr,
		   sizeof(sockaddr)) != 0) {
	sleep(1);
    }
    fprintf(stderr, "%s: connected to host \"%s\", port %d.\n",
	    program_name, hostname, port);

    protoent = getprotobyname("tcp");
    if (!protoent) {
	fprintf(stderr, "%s: getprotobyname() failed\n", program_name);
	return (-1);
    }

    tmp = 1;
    if (setsockopt(socket_fd, protoent->p_proto, TCP_NODELAY,
		   (char *)&tmp, sizeof(tmp)) != 0) {
	fprintf(stderr, "%s: setsockopt() failed\n", program_name);
	return (-1);
    }

    /* success -- return the fd */
    return (socket_fd);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* save the original terminal settings, so we can put them
   back when we are done. */
static  struct termios ttystate0;

int setTTY(int noraw)
{
    struct termios ttystate;

    if (noraw) return (0);

#ifdef DEBUG
    fprintf(stderr, "%s: set TTY attributes\n", program_name);
#endif /* #ifdef DEBUG */

    /* get initial tty state */
    if (tcgetattr(0, &ttystate0) < 0) {
	fprintf(stderr, "%s: tcgetattr() failed\n", program_name);
	return (-1);
    }

    /* modify initial tty state. To understand these changes,
       check the flags definition for the tcsetattr system call. */
    ttystate = ttystate0;
    ttystate.c_iflag = 0;
    ttystate.c_oflag = 0;
    ttystate.c_lflag = 0;
    ttystate.c_cflag &= ~(CSIZE|PARENB);
    ttystate.c_cflag |= CLOCAL | CS8;
    ttystate.c_lflag |= ISIG;
    ttystate.c_cc[VMIN] = 1;
    ttystate.c_cc[VTIME] = 0;

    if (tcsetattr(0, TCSANOW, &ttystate) < 0) {
	fprintf(stderr, "%s: tcsetattr() failed\n", program_name);
	return (-1);
    }
    return (0);
}


/* put back the original terminal settings */
void resetTTY(int noraw)
{
    if (noraw) return;

#ifdef DEBUG
    fprintf(stderr, "%s: reset TTY attributes\n", program_name);
#endif /* #ifdef DEBUG */

    tcsetattr(0, TCSANOW, &ttystate0);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void copyBytes(int socket_fd)
{
    int numfds;
    fd_set readfds;
    char buf[BUFSIZ];
    int cnt;

    while (1) {
	FD_ZERO (&readfds);
	FD_SET(0, &readfds);
	FD_SET(socket_fd, &readfds);
	numfds = select(socket_fd + 1, &readfds, 0, 0, 0);
	if (numfds < 0) {
	    fprintf(stderr, "\r\n%s: select() failed\r\n", program_name);
	    return;
	}

	if (FD_ISSET(0, &readfds)) {
	    cnt = read(0, buf, sizeof(buf));
#ifdef DEBUG
	    fprintf(stderr, "%s: write %d bytes to thinwire\n", program_name, cnt);
#endif /* #ifdef DEBUG */
	    /* XXX write should keep allow partial writes */
	    if (write(socket_fd, buf, cnt) < cnt) {
		fprintf(stderr, "\r\n%s: write() failed\r\n", program_name);
		return;
	    }
	}

	if (FD_ISSET(socket_fd, &readfds)) {
	    cnt = read(socket_fd, buf, sizeof(buf));
#ifdef DEBUG
	    fprintf(stderr, "%s: read %d bytes from thinwire\n", program_name, cnt);
#endif /* #ifdef DEBUG */
	    if (cnt <= 0) {
		fprintf(stderr, "\r\n%s: read() failed\r\n", program_name);
		return;
	    }
#ifdef NL_NEEDS_CR
	    {
	      int i;
	      for (i = 0; i < cnt; i++) {
		if (buf[i] == '\n') {
		  fputc('\r', stdout);
		}
		fputc(buf[i], stdout);
	      }
	    }
#else
	    /* we use fputs(3) instead of write(2) so out of band
	     * printf()s for debug messages are in sync with regular
	     * output */
	    buf[cnt] = '\0';
	    fputs(buf, stdout);
#endif
	    fflush(stdout);
	}
    }
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */



int main(int argc, char **argv)
{
    int socket_fd;

    /* get the options from the command line arguments */
    if (scanArgs(argc, argv) < 0) exit(-1);

    /* get a socket connection */
    socket_fd = getSocket(hostname);
    if (socket_fd < 0)  exit(-1);

    /* set the output terminal display characteristics */
    if (setTTY(noraw) < 0)  exit(-1);

    /* copy bytes until done */
    copyBytes(socket_fd);

    /* reset the output terminal display back */
    resetTTY(noraw);

    return (0);
}
