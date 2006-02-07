/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: simple echo client over UDP
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc, char **argv)
{
  int fd;
  uint16_t port;
  struct sockaddr_in addr;

  if (argc != 3)
    fprintf(stderr, "usage: echo-client-udp IP PORT\n"), exit(1);

  port = (uint16_t)atoi(argv[2]);

  if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    fprintf(stderr, "socket error: %m\n"), exit(1);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0)
    fprintf(stderr, "inet_pton error: %m\n"), exit(1);

  for (;;) {
      if (sendto(fd, "!", 1, 0, (struct sockaddr *)&addr, sizeof(addr))==-1)
	fprintf(stderr, "sendto error: %m\n"), exit(1);
  }

  exit(0);
}
