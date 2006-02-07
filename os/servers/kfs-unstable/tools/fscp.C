/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fscp.C,v 1.2 2004/02/24 19:07:45 lbsoares Exp $
 *****************************************************************************/
#include "fs.H"
#include "FileDisk.H"
#include "SuperBlock.H"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "KFSDebug.H"

static uval numberBlocks = FileDisk::DEFAULT_NUMBER_BLOCKS;

void usage(char *prog) 
{
    printf("Usage: %s [-b number_blocks] [-m debug mask]\n"
	   "\t\tdisk oldfile newfile\n"
	   "   or: %s [-b number_blocks] [-m debug mask]\n"
	   "\t\t-d disk newdir\n",
	   prog, prog);
    printf("Default value for debug mask is 0 (i.e., no debug printfs)\n");
    printf("About first use:\n");
    printf("\tUse kfs: to indicate a file/dir on a KFS disk\n\n");
    printf("\tdefault number of blocks is %ld\n",
	   FileDisk::DEFAULT_NUMBER_BLOCKS);
    printf("About second use:\n\tWith argument -d, fscp creates newdir (IT'S"
	   " NOT A RECURSIVE COPY)\n");
    printf("In both cases, intermmediate directories are created as needed\n");
    printf("Examples:\n");
    printf("To copy a file or symlink:\n");
    printf("\t%s DISK /bin/ls kfs:/bin/ls\n", prog);
    printf("To link a file:\n");
    printf("\t%s DISK kfs:/bin/ls kfs:/bin/dir\n\n", prog);
    printf("To create A DIRECTORY:\n"
	   "\t%s -d DISK kfs:root/dira/mydir\n", prog);
}

int
main(int argc, char *argv[])
{
    int c;
    extern int optind;
    const char *optlet = "nbdm";
    char *endptr;
    int create_dir = 0;
    uval debugMask = 0;

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'b': // number of blocks in the disk
	    if (optind == argc) {
		fprintf(stderr, "%s: missing number of blocks\n", argv[0]);
		usage(argv[0]);
		return (1);
	    }
	    numberBlocks = (int) strtol(argv[optind], &endptr, 10);
	    if (*endptr != '\0' || errno == ERANGE) {
		fprintf(stderr, "%s: number of blocks provided (%s) is "
			"invalid\n", argv[0], argv[optind]);
		usage(argv[0]);
		return 1;
	    }
	    optind++;
	    break;
	case 'd': // specifying directory creation
	    create_dir = 1;
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

    DebugMask::Mask = debugMask;

    if (create_dir) { // usage with -d, requires only disk and newdir as args
	if (optind + 2 != argc) {
	    usage(argv[0]);
	    return (1);
	}
    } else {	// we require more 3 arguments
	if (optind + 3 != argc) {
	    usage(argv[0]);
	    return (1);
	}
    }

    char *diskname, *oldfile = NULL, *newfile;
    diskname = argv[optind++];
    if (!create_dir) {
	oldfile = argv[optind++];
    }
    newfile = argv[optind];

    int fd;
    // check that we have a disk already initialized through something
    // like mkfs
#if defined (KFS_TOOLS) && defined(PLATFORM_Darwin)
    fd = open(diskname, O_RDWR);
#else
    fd = open64(diskname, O_RDWR);
#endif
    if (fd == -1) {
	printf("%s: open(%s) failed with %s\n", argv[0], diskname,
	       strerror(errno));
	return 1;
    }

    // create a disk to use
    Disk *disk;
    disk = new FileDisk(fd, numberBlocks, OS_BLOCK_SIZE);

    char oldPath[1024], newPath[1024];
    if (!create_dir) {
	tassertMsg(strlen(oldfile) < 1024,
		   "not enough space for oldfile name\n");
    }
    tassertMsg(strlen(newfile) < 1024, "not enough space for newfile name\n");
    struct stat stat_buf;

    if (strncmp(newfile, "kfs", 3) != 0) {
	printf("%s: new file or directory should start with kfs:\n", argv[0]);
	usage(argv[0]);
	return 1;
    }

    if (newfile[4] != '/') {
	newPath[0] = '/';
	strcpy(newPath + 1, newfile + 4);
    } else {
	strcpy(newPath, newfile + 4);
    }

    int ret = 0;
    if (create_dir) {
	SysStatus rc = createDirKFS(argv[0], disk, newPath, 0, 0);
	if (rc != 0) {
	    printf("%s: createDirKFS returned rc 0x%lx for dir %s\n",
		   argv[0], rc, newfile);
	    return 1;
	}
    } else {
        if (strlen(oldfile) > 3 && !strncmp(oldfile, "kfs", 3)) {
            // link a file
            if (oldfile[4] != '/') {
                oldPath[0] = '/';
                strcpy(oldPath + 1, oldfile + 4);
            } else {
                strcpy(oldPath, oldfile + 4);
            }

            printf("linking %s to %s\n", oldPath, newPath);
            SysStatus rc = linkFileKFS(argv[0], disk, oldPath, newPath);
	    if (_FAILURE(rc)) {
		if (_SGENCD(rc) == ENOENT) {
		    printf("%s: directory or file in %s doesn't exist\n",
			   argv[0], oldfile);
		} else if (_SGENCD(rc) == EPERM) {
		    printf("%s: %s is a directory\n", argv[0], oldfile);
		}
		return 1;
	    }
        } else {
            printf("copying %s to %s\n", oldfile , newPath);
            if (lstat(oldfile, &stat_buf) == -1) {
		printf("%s: open(%s) failed with error %s\n", argv[0],
		       oldfile, strerror(errno));
		return 1;
	    } else if (!S_ISLNK(stat_buf.st_mode)
		       && !S_ISREG(stat_buf.st_mode)) {
		printf("%s: oldfile %s is not a regular file or symbolic "
		       "link\n", argv[0], oldfile);
		return 1;
	    }

	    SysStatus rc;
	    if (S_ISLNK(stat_buf.st_mode)) {
		rc = createSymLinkKFS(argv[0], disk, oldfile, newPath, 0, 0);
		if (rc != 0) {
		    printf("%s: createFileKFS returned rc 0x%lx for file %s\n",
			   argv[0], rc, newfile);
		    ret = 1;
		}

	    } else {
		fd = open(oldfile, O_RDONLY);
		if (fd == -1) {
		    printf("%s: open(%s) failed with error %s\n", argv[0],
			   oldfile, strerror(errno));
		    return 1;
		}
		// FIXME: should check for maximum file size available for this
		// file
		rc = createFileKFS(argv[0], disk, fd, newPath, 0x1FF, 0, 0);
		if (rc != 0) {
		    printf("%s: createFileKFS returned rc 0x%lx for file %s\n",
			   argv[0], rc, newfile);
		    ret = 1;
		}
	    }
            close(fd);
        }
    }

    // free the disk
    delete disk;
    return ret;
}
