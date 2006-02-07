/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mkfs.C,v 1.8 2004/08/20 17:30:47 mostrows Exp $
 *****************************************************************************/
#include "fs.H"
#include "FileDisk.H"
#include "SuperBlock.H"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "KFSDebug.H"

static void
usage(char *prog)
{
    printf("Usage: %s -d <diskname> [-b <number of blocks>] "
	   "[-m <debug mask>]\n"
	   "\t default for number of blocks is %ld\n"
	   "\t default for debug mask is 0 (no debug printfs)\n"
	   "\t\t(mask can be decimal or hexadecimal number)\n",
	   prog, (uval) FileDisk::DEFAULT_NUMBER_BLOCKS);
}

int
main(int argc, char *argv[])
{
    int c;
    extern int optind;
    const char *optlet = "dnbm";
    char *endptr;

    Disk *disk;
    char *diskname = NULL;
    uval numberBlocks = FileDisk::DEFAULT_NUMBER_BLOCKS;
    uval32 debugMask = DebugMask::GetDebugMask();

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'd':
	    if (optind == argc) {
		fprintf(stderr, "%s: missing disk name\n", argv[0]);
		usage(argv[0]);
		return 1;
	    }
	    diskname = argv[optind];
	    optind++;
	    break;
	case 'b':
	    if (optind == argc) {
		fprintf(stderr, "%s: missing number of blocks\n", argv[0]);
		usage(argv[0]);
		return 1;
	    }
	    numberBlocks = (int) strtoul(argv[optind], &endptr, 10);
	    if (*endptr != '\0' || errno == ERANGE || numberBlocks < 0) {
		fprintf(stderr, "%s: disk number provided (%s) is invalid\n",
			argv[0], argv[optind]);
		usage(argv[0]);
		return 1;
	    }
	    optind++;
	    break;
	case 'm':
	    if (optind == argc) {
		fprintf(stderr, "%s: missing debug mask\n", argv[0]);
		usage(argv[0]);
		return 1;
	    }
	    debugMask = (int) strtoul(argv[optind], &endptr, 0);
	    if (*endptr != '\0') {
		fprintf(stderr, "%s: debug mask provided (%s) is invalid\n",
			argv[0], argv[optind]);
		usage(argv[0]);
		return 1;
	    } else if (errno == ERANGE) {
		fprintf(stderr, "%s: debug mask provided (%s) is too large, will"
			"\nuse DebugMask::ALL (0x%lx)\n",
			argv[0], argv[optind], (uval)DebugMask::ALL);
		debugMask = DebugMask::ALL;
		errno = 0; /* it seems next successful invocation of strtol is
			    * not taking care of this! */
	    } else if (debugMask < 0 || debugMask > DebugMask::ALL) {
		fprintf(stderr, "%s: debug mask provided (%s) is invalid\n",
			argv[0], argv[optind]);
		usage(argv[0]);
		return 1;
	    }

	    optind++;
	    break;
	case '?':
	default:
	    usage(argv[0]);
	    return (1);
	}
    }
    if (optind != argc) {
	usage(argv[0]);
	return (1);
    }
    if (diskname == NULL) {
	fprintf(stderr, "%s: disk name not provided\n", argv[0]);
	usage(argv[0]);
	return (1);
    }

    // open file to serve as FileDisk
#if defined (KFS_TOOLS) && defined(PLATFORM_OS_Darwin)
    int fd = open(diskname, O_CREAT | O_RDWR | O_TRUNC,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#else
    int fd = open64(diskname, O_CREAT | O_RDWR | O_TRUNC,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
#endif

    if (fd == -1) {
	printf("%s: open64(%s) failed: %s\n", argv[0], diskname, strerror(errno));
	return 1;
    }

    // set debug mask
    DebugMask::Mask = debugMask;

    // create a disk to format
    disk = new FileDisk(fd, numberBlocks, OS_BLOCK_SIZE);

    // format the disk
    SysStatus rc = formatKFS(disk);
    if (rc != 0) {
	printf("formatKFS failed\n");
    }

    // free the disk
    delete disk;
}
