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
 * Module Description: simple echo server over UDP
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc, char **argv)
{
  int fd;
  uint16_t port;
  struct sockaddr_in addr, them;
  socklen_t themlen = sizeof(them);
  char buff[128];

  if (argc != 2)
    fprintf(stderr, "usage: udp-echo-server PORT\n"), exit(1);

  port = (uint16_t)atoi(argv[1]);

  if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    fprintf(stderr, "socket error: %m\n"), exit(1);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    fprintf(stderr, "bind error: %m\n"), exit(1);

  for (;;) {
      if (recvfrom(fd, buff, sizeof(buff), 
		   0, (struct sockaddr *)&them, &themlen) == -1)
	fprintf(stderr, "recvfrom error: %m\n"), exit(1);

      printf("got packet from %s:%d: %c\n",
	     inet_ntoa(them.sin_addr), ntohs(them.sin_port), buff[0]);
  }

  return 0;
}
