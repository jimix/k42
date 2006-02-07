/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: copy.c,v 1.9 2002/11/05 22:25:01 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test copy and cat
 * **************************************************************************/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-s chunk_size] <FILE> [FILE]...\n"
	    "  copy files to last file\n"
	    "    optional argument -s chunk_size specifies the number of bytes\n"
	    "        transfered at each read/write operation (default is 128)\n"
	    "    if only one file is given or last file is `-'\n"
	    "    then ouput to stdout\n"
	    "\n", prog);
}

/* This program originally took as input a list of files.
 * The routine bellow deals with an optional argument "-s chunk_size"
 * leaving the argument list as expected by the original version
 * of this program, so that the changes for the original main routine
 * could be minimal
 */
static void
process_chunk_size(int *argc, char* argv[], int * chunk_size)
{
    int sz, i;
    char *endptr;
    
    if (*argc < 2) return;

    if (strcmp(argv[1], "-s") == 0) {
	if (*argc == 2) { // missing value for -s
	    fprintf(stderr, "%s: argument for -s missing; option ignored\n",
		    argv[0]);
	    (*argc)--;
	} else {
	    sz = strtol(argv[2], &endptr, 10);
	    if (*endptr != '\0' || errno == ERANGE ) {
		fprintf(stderr, "%s: length %s provided for argument -s is "
			"invalid, so it's ignored.\n", argv[0], argv[2]);
		for (i=1; i < *argc-1; i++) argv[i] = argv[i+1];
		(*argc)--;
	    } else {
		if (sz == 0) {
		    fprintf(stderr, "%s: length %s provided for argument -s is "
			    "invalid, so it's ignored.\n", argv[0], argv[2]);
		} {
		    *chunk_size = sz;
		}
		for (i=1; i < *argc-2; i++) argv[i] = argv[i+2];
		(*argc) -= 2;
	    }
	}
    }
    return;
}

int
main(int argc, char* argv[])
{
    int infd;
    int outfd;
    int infiles;
    int i;
    int sz;
    char *buf;
    int chunk_size = 128; // default is 128 bytes
    
    /* This program originally took as input a list of files.
     * The routine bellow deals with an optional argument "-s chunk_size"
     * leaving the argument list as expected by the original version
     * of this program, so that the code below didn't have to be changed
     */
    process_chunk_size(&argc, argv, &chunk_size);
    buf = (char*)malloc(chunk_size);

    switch (argc) {
    case 0: /* impossible */
    case 1:
	usage(argv[0]);
	return 1;
	break;
    case 2:
	outfd = 1;
	infiles = 1;
	break;
    default:
	if (strcmp(argv[argc - 1], "-") == 0) {
	    outfd = 1;
	} else {
	    outfd = open(argv[argc - 1], O_CREAT|O_TRUNC|O_WRONLY, 0640);
	    if (outfd == -1) {
		fprintf(stderr, "%s: %s: %s\n",
			argv[0], argv[argc - 1], strerror(errno));
		return 1;
	    }
	}
	infiles = argc - 2;
	break;
    }

    for (i = 1; i <= infiles; i++) {
	infd = open(argv[i], O_RDONLY);
	if (infd == -1) {
	    fprintf(stderr, "%s: %s: %s\n",
		    argv[0], argv[i], strerror(errno));
	    continue;
	}
	do {
	    sz = read(infd, buf, chunk_size);
	    if (sz == -1) {
		fprintf(stderr, "%s: %s: %s\n", 
                    argv[0], argv[i], strerror(errno));
		return 1;
	    } else if (sz > 0) {
		sz = write(outfd, buf, sz);
		if (sz == -1) { 
		    fprintf(stderr, "%s: write(): %s\n",  
			    argv[0], strerror(errno)); 
		    return 1; 
		} 
	    }
	} while (sz > 0);
	close(infd);
    }
    if (outfd != 1) {
	close(outfd);
    }

    free(buf);
    return 0;
}
