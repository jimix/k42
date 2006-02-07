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
 * Module Description: test basic UNIX daemon pattern
 * **************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
  int i, pid, ret, fd;

  if (argc != 1) {
      printf("`daemon' exercises the basic UNIX daemon pattern\n\n"
	     "Usage: daemon\n\n"
	     "Example: daemon\n\n"
             "Note: the child exits after verifying daemon pattern works\n");
      return 1;
  }

  /* Step 1: fork a child and exit, so child will be adopted by init.  */
  if ((pid = fork()) == 0) {

      /* Step 2: become the session leader of this new session, and
	 the process group leader of a new process group, and have no
	 controlling terminal.  Our process group ID shall be set
	 equal to our process ID.  We shall be the only process in the
	 new process group and the only process in the new
	 session.  */
      if ((ret = setsid()) < 0) {
	  fprintf(stderr, "FAIL: setsid error: %m\n");
	  exit(1);
      }
      
      /* Step 3: close all file descriptors.  */
      for (i = getdtablesize(); i >= 0; --i)
	close(i);

      /* Step 4: set stdin/stdout to be /dev/null.  */
      if ((fd = open("/dev/null", O_RDWR)) < 0)
	exit(1);
      dup(fd);
      dup(fd);

      /* Step 5: set umask to something safe for a daemon.  */
      umask(027);

      /* Step 6: change operating directory to something safe.  */
      if ((ret = chdir("/")) < 0)
	exit(1);

      /* Most daemons go on to ignore certain signals, but we do not
	 care about that yet.  */

      exit(0);
  } else if (pid < 0) {
      fprintf(stderr, "FAIL: fork failed: %m\n");
      exit(1);
  } else {
      printf("parent: orphaning child whose pid is %i (0x%X)\n", pid, pid);
      exit(0);
  }
}
