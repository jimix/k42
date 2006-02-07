/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: build_kern_parms.c,v 1.2 2004/09/23 18:23:38 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Create the binary kernel parameter file.
 * Currently only supports boot parameters to replace thinwire
 * environment variables 
 * **************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

extern char **environ;

void print_help()
{
    fprintf(stderr, "Usage: build_kern_parms -o output_file\n");
}

int build_env_data_block(char **data_block)
{
    unsigned int data_block_size = 10, data_block_used = 0;

    *data_block = (char *)malloc(data_block_size);

    sprintf(*data_block, "KBootParms");
    data_block_used += strlen("KBootParms") + 1;
    
    while (*environ != NULL) {
	if (strstr(*environ, "K42")==*environ) {
	    
	    /* Allocate more memory if necessary */
	    if (strlen(*environ)+1+data_block_used > data_block_size) {
		data_block_size += strlen(*environ)*10;
		*data_block = (char *)realloc(*data_block, data_block_size);
		if (data_block == NULL) {
		    fprintf(stderr, "realloc for %i bytes failed\n",
			    data_block_size);
		    exit(1);
		}
	    }

	    sprintf((*data_block)+data_block_used, "%s", *environ);
	    data_block_used += strlen(*environ) + 1;
	}
	**environ++;
    }
    
    return data_block_used;
}


void build_parameter_file(char *filename)
{
    char *environment_block;
    int environment_block_size, tmp;
    int output_fd;
    

    environment_block_size = build_env_data_block(&environment_block);

    output_fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 
		     S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if (output_fd < 0) {
	perror("Failed to open output file");
	exit(1);
    }

    /* Currently we only have 1 block of data. The environment block */
    tmp = htonl(1);
    if (write(output_fd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
	perror("Write failed\n");
	exit(1);
    }

    /* Write environment block, with size of block as header. Size
       includes the header */
    tmp = htonl(environment_block_size+sizeof(tmp));
    if (write(output_fd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
	perror("Write failed\n");
	exit(1);
    }
    if (write(output_fd, environment_block, environment_block_size) !=
	environment_block_size) {
	perror("Write failed\n");
	exit(1);
    }

    close(output_fd);
}


int main(int argc, char *argv[])
{
    int option;
    char *output_file = NULL;

    while (1) {
	option = getopt(argc, argv, "ho:");

	if (option == -1) break;

	switch (option) {
	case 'h':
	    print_help();
	    break;

	case 'o':
	    //asprintf(&output_file, "%s", optarg);
	    output_file = (char *) malloc(strlen(optarg) + 1);
	    strcpy(output_file, optarg);
	    break;
	}

    }

    if (output_file == NULL) {
	fprintf(stderr, "Must specify output file\n");
	exit(1);
    }

    build_parameter_file(output_file);

    exit(0);
}
