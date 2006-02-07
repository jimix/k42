/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: select.c,v 1.14 2004/10/01 00:51:42 butrico Exp $
 *****************************************************************************/
/* Creates two threads - one server, one client.  Server blocks in select
 * waiting for client to connect and send a message.  Server reads message and
 * sends message back in reply.  Server could handle multiple clients...
 */

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>           // for exit();
#include "pthread.h"
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#define SERVER_PORT 8000
#define MESG "hello all"
#define MESG_SIZE (sizeof(MESG))

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK         (u_long)0x7f000001 /* in host order */
#endif /* #ifndef INADDR_LOOPBACK */
static volatile int sync_thread = 0;

static void *
server(void *arg)
{
  struct sockaddr_in name;
  fd_set fds, orig_fds;
  int fd_listen;
  int i, rc, max_fd;
  int one = 1;

  fd_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd_listen < 0) {
    fprintf(stderr, "Error: server socket create failed (%d)\n", errno);
    return NULL;
  }

  rc = setsockopt(fd_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (rc != 0) {
    fprintf(stderr, "setsockopt(SO_REUSEADDR): %s\n", strerror(errno));
    return NULL;
  }

  name.sin_family = AF_INET;
  name.sin_addr.s_addr = htonl(INADDR_ANY);
  name.sin_port = htons(SERVER_PORT);

  rc = bind(fd_listen, (struct sockaddr *)&name, sizeof(struct sockaddr));
  if (rc < 0) {
    fprintf(stderr, "Error: server bind failed (%d)\n", errno);
    close (fd_listen);
    exit(1);
  }
  rc = listen(fd_listen, 8);
  if (rc < 0) {
    fprintf(stderr, "Error: server listen failed (%d)\n", errno);
    exit(1);
  }

  sync_thread = 1;

  FD_ZERO(&orig_fds);
  FD_SET(fd_listen, &orig_fds);

  max_fd = fd_listen;

  while (1) {

    fds = orig_fds;

    rc = select(max_fd+1, &fds, NULL, NULL, NULL);

    printf("Select returned: %d\n", rc);

    if (rc < 0) {
      fprintf(stderr, "Error: server select failed (%d)\n", errno);
      exit(1);
    }

    if (rc == 0) {
      fprintf(stderr, "Select timed out\n");
    } else {

      if (FD_ISSET(fd_listen, &fds)) {
        int new_fd;
        new_fd = accept(fd_listen, NULL, NULL);
        if (new_fd < 0) {
          fprintf(stderr, "Error: server accept failed (%d)\n", errno);
        } else {
          printf("Found new connection\n");
          if (new_fd > max_fd) max_fd = new_fd;
          FD_SET(new_fd, &orig_fds);
        }
        rc--;
      }
      i = 0;
      while (rc > 0 && i <= max_fd) {
        if (FD_ISSET(i, &fds)) {
          int bread;
          char buffer[MESG_SIZE];

          bread = read(i, buffer, MESG_SIZE);
          if (bread < 0) {
            fprintf(stderr, "Error: server read failed (%d)\n", errno);
            FD_CLR(i, &orig_fds);
            close(i);
          } else if (bread == 0) {
            FD_CLR(i, &orig_fds);
            close(i);
          } else {
            printf("Server: received message [%s]\n", buffer);
            rc = write(i, buffer, MESG_SIZE);
            if (rc < 0 || rc != MESG_SIZE) {
              fprintf(stderr, "Server: write failed (%d)\n", errno);
            }
            FD_CLR(i, &orig_fds);
            close(i);
          }
          rc--;
        }
        i++;
      }

    }

  }
#if 0 /* No select */
  {
    int new_fd;
    int bread;
    char buffer[MESG_SIZE];


    new_fd = accept(fd_listen, NULL, NULL);
    if (new_fd < 0) {
      fprintf(stderr, "Error: server accept failed (%d)\n", errno);
    } else {
      printf("Found new connection\n");
    }

    bread = read(new_fd, buffer, MESG_SIZE);
    if (bread < 0) {
      fprintf(stderr, "Error: server read failed (%d)\n", errno);
      close(new_fd);
    } else if (bread == 0) {
      close(new_fd);
    } else {
      printf("Server: received message [%s]\n", buffer);
      rc = write(new_fd, buffer, MESG_SIZE);
      if (rc < 0 || rc != MESG_SIZE) {
        fprintf(stderr, "Server: write failed (%d)\n", errno);
      }
      close(new_fd);
    }
  }
#endif /* #if 0 */

  return NULL;
}

static void *
client(void *arg)
{
  struct sockaddr_in saddr;
  int fd;
  int rc;
  char *msg = MESG;
  char in[MESG_SIZE];

  fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    fprintf(stderr, "Error: client socket create failed (%d)\n", errno);
    return NULL;
  }

  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  saddr.sin_port = htons(SERVER_PORT);

  rc = connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));
  if (rc < 0) {
    fprintf(stderr, "Error: client connect failed (%d)\n", errno);
    close (fd);
    return NULL;
  }

  printf("Client: sending message [%s]\n", msg);

  rc = write(fd, msg, MESG_SIZE);
  if (rc < 0 || rc != MESG_SIZE) {
    fprintf(stderr, "Error: client write failed rc=%d (%d)\n", rc, errno);
    close (fd);
    return NULL;
  }

  rc = read(fd, in, MESG_SIZE);
  if (rc < 0 || rc != MESG_SIZE) {
    fprintf(stderr, "Error: client read failed rc=%d (%d)\n", rc, errno);
    close (fd);
    return NULL;
  }

  printf("Client read: [%s]\n", in);

  close (fd);

  return NULL;
}


int
main(void) {
  pthread_t th_a, th_b;
  int rc;
  void *retval;

  rc = pthread_create(&th_a, NULL, server, NULL);
  if (rc != 0) {
    fprintf(stderr, "Create a failed %d (%d)\n", rc, errno);
    return 1;
  }
  printf("Created server thread\n");

  while (sync_thread==0);

  rc = pthread_create(&th_b, NULL, client, NULL);
  if (rc != 0) {
    fprintf(stderr, "Create b failed %d (%d)\n", rc, errno);
    return 1;
  }

  printf("Created client thread\n");

  rc = pthread_join(th_b, &retval);
  if (rc < 0) {
    fprintf(stderr, "Pthread_join failed %d (%d)\n", rc, errno);
    return 1;
  }

  printf("Client exited\n");

  return 0;
}

