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
 * Module Description: simple daytime server
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
  int fd, fd2;
  uint16_t port;
  struct sockaddr_in addr;
  time_t ticks;
  char buff[128];

  if (argc != 2)
    fprintf(stderr, "usage: daytime-server PORT\n"), exit(1);

  port = (uint16_t)atoi(argv[1]);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    fprintf(stderr, "socket error: %m\n"), exit(1);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    fprintf(stderr, "bind error: %m\n"), exit(1);

  if (listen(fd, 16) < 0)
    fprintf(stderr, "listen error: %m\n"), exit(1);

  for (;;) {
      if ((fd2 = accept(fd, (struct sockaddr *)NULL, NULL)) < 0)
	fprintf(stderr, "listen error: %m\n"), exit(1);

      ticks = time(NULL);
      snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
      printf("%s", buff);
      if (write(fd2, buff, strlen(buff)) != strlen(buff))
	fprintf(stderr, "write error: %m\n"), exit(1);

      close(fd2);
  }

  return 0;
}
