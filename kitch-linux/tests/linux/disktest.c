/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000,2001
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
   char *fname = "/dev/sdc"; 
   char buf[1024];
   int fd;

   fd = open (fname, O_RDWR);

   read(fd, buf, 1024);
   write(fd, buf, 1024);
   lseek(fd, 0, SEEK_SET);
   return 0;
}
