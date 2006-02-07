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
 * Module Description: simple echo client
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_LEN 1024

int main(int argc, char **argv)
{
  int fd, n;
  uint16_t port;
  char line[MAX_LEN];
  struct sockaddr_in addr;

  if (argc != 3)
    fprintf(stderr, "usage: echo-client IP PORT\n"), exit(1);

  port = (uint16_t)atoi(argv[2]);

#ifdef STOP_HERE_FOR_BREAK_INTO
  write(1, "> ", 1);
  read(0, &line, MAX_LEN);
#endif

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fprintf(stderr, "socket error: %m\n"), exit(1);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0)
    fprintf(stderr, "inet_pton error: %m\n"), exit(1);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    fprintf(stderr, "connect error: %m\n"), exit(1);

  if ((n = write(fd, "!", 1)) != 1)
    fprintf(stderr, "write error: %m\n"), exit(1);

  if ((n = read(fd, line, 1)) != 1)
    fprintf(stderr, "read error: %m\n"), exit(1);

  if (line[0] != '!') {
    fprintf(stderr, "error: we sent `%c', we got back `%c'\n", '!', line[0]);
    exit(1);
  }

  exit(0);
}
