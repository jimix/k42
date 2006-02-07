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
 * Module Description: simple daytime client
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
    fprintf(stderr, "usage: daytime-client IP PORT\n"), exit(1);

  port = (uint16_t)atoi(argv[2]);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fprintf(stderr, "socket error: %m\n"), exit(1);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0)
    fprintf(stderr, "inet_pton error: %m\n"), exit(1);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    fprintf(stderr, "connect error: %m\n"), exit(1);

  while ((n = read(fd, line, MAX_LEN)) > 0)
    line[n] = 0, fputs(line, stdout);

  if (n < 0)
    fprintf(stderr, "read error: %m\n"), exit(1);

  return 0;
}
