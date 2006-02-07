/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: cat.c,v 1.6 2004/03/12 04:22:43 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for linux personality
 * **************************************************************************/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int
main(int argc, char* argv[])
{
    int in,out;
    int i,j;
    char buf[128];
    if (argc != 3) {
	fprintf(stderr, "don't know why I am called cat, I am cp, give me two"
		" file names\n");
	return 1;
    }
    in = open(argv[1],O_RDONLY,0);
    if (in < 0)
	return 2;
    out = open(argv[2],O_CREAT+O_TRUNC+O_WRONLY,0640);
    if (out < 0)
	return 3;
    do {
	i = read(in,buf,sizeof(buf));
	if(i) {
	    j = write(out,buf,i);
	    if (j != i)
		return 4;
	}
    } while (i);
    close(in);
    close(out);
    return 0;
}
