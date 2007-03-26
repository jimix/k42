/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceNetClient.C,v 1.10 2005/06/10 20:00:56 apw Exp $
 *****************************************************************************/

#define _USE_IRS /* needed on AIX to get herror prototype, don't ask me why! */
#define __STDC_CONSTANT_MACROS /* needed on linux to get UINT64_C */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/hostSysTypes.H>

# include <endian.h>
# define BYTE_ORDER __BYTE_ORDER

#if BYTE_ORDER == BIG_ENDIAN
# define ntoh64(x) (x)
#else
# include <sys/bswap.h>
# define ntoh64(x) bswap_64(x)
#endif

#define MAX_CPUS	32
#define DEFAULT_PORT	4242
#define DEFAULT_MPBASE	"trace-out"

#define MAX_PAYLOAD	1400
#define FLAG_SEQNUM     UINT64_C(-1)

/* format of packets sent - data plus a sequence number */
struct packet {
    uint64_t seqnum;		/* sequence number, in network byte order */
    char data[MAX_PAYLOAD];	/* payload, in host byte order */
};

static struct {
    int sockfd;
    FILE *file;
    uint64_t seqnum;
} clients[MAX_CPUS];
static int cpus = 1;

static struct in_addr peer_addr;
static int check_peer_addr = 0;

static void
print_usage(void)
{
    printf("traceNetClient [--help] [--src host] [--port port] [--mp N]\n"
	   "This program receives trace data from the network and dumps it"
	   " into files\non the local machine. Options are:\n"
	   " --help\t\tprints out this usage information\n"
	   " --src host\tspecifies the host to listen for data from (default"
		" anyone)\n"
	   " --port port\tspecifies the base port number to receive on"
		" (default %hu)\n"
	   " --mp N\t\tspecifies to receive into N files from N consecutive"
		" ports,\n\t\tused for multiprocessor tracing\n",
	   DEFAULT_PORT);
}

/* (re)open and initialise the output file */
static int
reopen_file(int cpu, const char *basename, size_t baselen)
{
    char namebuf[PATH_MAX];
    unsigned long i;
    int rc;

    /* make sure the base name string contains only "proper" characters */
    for (i = 0; i < baselen; i++) {
        if (!isprint(basename[i])) {
            baselen = i;
        }
    }

    /* make sure the base name string contains no '/' characters */
    for (i = baselen - 1; i > 0; i--) {
        if (basename[i] == '/') {
            basename = &basename[i + 1];
            baselen -= i + 1;
        }
    }

    /* they supplied junk or nothing, use the default basename */
    if (baselen == 0) {
        basename = DEFAULT_MPBASE;
    }

    rc = snprintf(namebuf, sizeof(namebuf), "%.*s.%d.trc", (int)baselen,
    	    	  basename, cpu);
    if (rc == -1) {
	return 1;
    }

    printf("%d: opening %s\n", cpu, namebuf);

    if (clients[cpu].file != NULL) {
        fclose(clients[cpu].file);
    }
    clients[cpu].file = fopen(namebuf, "w");
    if (clients[cpu].file == NULL) {
	printf("Error: failed to open %s for writing\n", namebuf);
	return 1;
    }

    return 0;
}

/* open and initialise the UDP sockets we'll be receiving data from */
static int
open_sockets(uint16_t port)
{
    struct sockaddr_in sockaddr;
    int i, fd, rc;

    for (i = 0; i < cpus; i++) {
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
	    perror("Error: socket");
	    return -1;
	}

	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(port);
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&sockaddr.sin_zero, 0, sizeof(sockaddr.sin_zero));

	rc = bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	if (rc != 0) {
	    fprintf(stderr, "Error: bind (UDP port %hu): %s\n", port,
                    strerror(errno));
            if (errno == EADDRINUSE) {
                fprintf(stderr, "Try again with the --port flag, remembering"
                        " to use the same port on the victim.\n");
            }
	    return -1;
	}

	clients[i].sockfd = fd;
	port++;
    }

    return 0;
}

/* receive data which is ready for a given CPU's socket */
static int
receive_data(int cpu)
{
    struct packet buf;
    uint64_t seqnum, lost;
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t packet_len;
    size_t data_len;
    int rc;

    packet_len = recvfrom(clients[cpu].sockfd, &buf, sizeof(buf), 0,
			  (struct sockaddr *)&from, &fromlen);
    if (packet_len < 0) {
	perror("Error: recvfrom");
	return -1;
    } else if (fromlen != sizeof(from)) {
	fprintf(stderr, "Error: unexpected source address len %d in recvfrom\n",
		fromlen);
	return -1;
    }

    /* check that it came from the host we're listening to */
    if (check_peer_addr && from.sin_addr.s_addr != peer_addr.s_addr) {
	printf("Warning: ignored bogus packet from %s\n",
	       inet_ntoa(from.sin_addr));
	return 0;
    }

    /* work out sequence number and length of data */
    seqnum = ntoh64(buf.seqnum);
    data_len = packet_len - sizeof(seqnum);

    /* check for the flag that occurs at the start of a new trace */
    if (seqnum == FLAG_SEQNUM) {
        rc = reopen_file(cpu, buf.data, data_len);
        if (rc != 0) {
            return rc;
        }
        clients[cpu].seqnum = 0;
        return 0;
    } else if (clients[cpu].file == NULL) {
        printf("Warning: ignoring data before start-of-trace marker from "
               "CPU %d\n", cpu);
        return 0;
    }

    /* check that the sequence number is what we expect */
    if (clients[cpu].seqnum < seqnum) {
	lost = seqnum - clients[cpu].seqnum;
    	printf("Warning: %llu bytes lost from CPU %d\n", 
	       (unsigned long long)lost, cpu);
	rc = fseek(clients[cpu].file, lost, SEEK_CUR);
	if (rc != 0) {
	    perror("Error: fseek");
	    return -1;
	}
    } else if (clients[cpu].seqnum > seqnum) {
	printf("Warning: out of order packet from CPU %d, dropped\n", cpu);
	return 0;
    }

    /* update our idea of the sequence */
    clients[cpu].seqnum = seqnum + data_len;

    rc = fwrite(&buf.data, data_len, 1, clients[cpu].file);
    if (rc != 1) {
	perror("Error: fwrite");
	return -1;
    }

    fflush(clients[cpu].file);

    return 0;
}

/* main select() loop, calls receive_data when required */
static int
main_loop(void)
{
    fd_set fds;
    int i, rc, active_fds, maxfd = -1;

    for (i = 0; i < cpus; i++) {
    	if (clients[i].sockfd > maxfd) {
	    maxfd = clients[i].sockfd;
	}
    }

    while (1) {
	FD_ZERO(&fds);
	for (i = 0; i < cpus; i++) {
	    FD_SET(clients[i].sockfd, &fds);
	}

	active_fds = select(maxfd + 1, &fds, NULL, NULL, NULL);
	if (active_fds <= 0) {
	    perror("Error: select");
	    return -1;
	}

	for (i = 0; active_fds > 0 && i < cpus; i++) {
	    if (FD_ISSET(clients[i].sockfd, &fds)) {
		active_fds--;
		rc = receive_data(i);
		if (rc != 0) {
		    return rc;
		}
	    }
	}
    }
}

int
main(int argc, char **argv)
{
    char *srcHost = NULL;
    uint16_t basePort = DEFAULT_PORT;
    struct hostent *hostent;
    int rc, i;

    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "--help") == 0) {
	    print_usage();
	    return 0;
	} else if (strcmp(argv[i], "--src") == 0) {
	    srcHost = argv[++i];
	} else if (strcmp(argv[i], "--port") == 0) {
	    sscanf(argv[++i], "%hu", &basePort);
	} else if (strcmp(argv[i], "--mp") == 0) {
	    sscanf(argv[++i], "%d", &cpus);
	} else {
	    printf("Error: unknown option %s\n", argv[i]);
	    print_usage();
	    return 1;
	}
    }

    if (cpus <= 0 || cpus > MAX_CPUS) {
	printf("Error: can't support %d CPUs\n", cpus);
	return 1;
    }

    if (srcHost != NULL) {
	hostent = gethostbyname(srcHost);
	if (hostent == NULL) {
	    herror("Error: gethostbyname");
	    return -1;
	}
	peer_addr = *((struct in_addr *)hostent->h_addr);
	check_peer_addr = 1;
    }

    rc = open_sockets(basePort);
    if (rc != 0) {
	return rc;
    }

    /* initialise file poiners and sequence numbers - we expect 0 first off */
    for (i = 0; i < cpus; i++) {
	clients[i].seqnum = 0;
        clients[i].file = NULL;
    }

    return main_loop();
}
