/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: printStat.C,v 1.12 2005/06/28 19:48:46 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <unistd.h>
#include <usr/ProgExec.H>
#include <stub/StubFileSystemK42RamFS.H>
#include <stub/StubFileSystemNFS.H>
#ifdef KFS_ENABLED
#include <stub/StubFileSystemKFS.H>
#endif // #ifdef KFS_ENABLED
#include <sys/systemAccess.H>

static void usage(char *prog)
{
    printf("Usage: %s -t type\n"
	   "\ttype: kfs or nfs or ramfs\n", prog);
}

int
main(int argc, char **argv)
{
    NativeProcess();

    SysStatus rc;
    enum {ramfs, kfs, nfs, invalid} fileSystem;

    fileSystem = invalid;

    int c;
    extern int optind;
    const char *optlet = "t";

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 't':
	    if (optind == argc) {
		printf("%s: missing type\n", argv[0]);
		usage(argv[0]);
		return 1;
	    }
	    if (strncmp(argv[optind], "ramfs", 5) == 0) {
		fileSystem = ramfs;
	    } else if (strncmp(argv[optind], "kfs", 3) == 0) {
		fileSystem = kfs;
	    } else if (strncmp(argv[optind], "nfs", 3) == 0) {
		fileSystem = nfs;
	    } else {
		printf("%s: invalid type\n", argv[0]);
		usage(argv[0]);
		return 1;
	    }
	    break;
	case '?':
	default:
	    usage(argv[0]);
	    return 1;
	}
    }

    switch (fileSystem) {
    case ramfs:
	rc = StubFileSystemK42RamFS::_PrintStats();
	if (_FAILURE(rc)) {
	    printf("%s: invocation of StubFileSystemK42RamFS::_printStat()"
		       " failed rc = (%ld, %ld, %ld)\n", argv[0],
		   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	}
	break;
#ifdef KFS_ENABLED
    case kfs:
	rc = StubFileSystemKFS::_PrintStats();
	if (_FAILURE(rc)) {
	    printf("%s: invocation of StubFileSystemKFS::_printStat()"
		       " failed rc = (%ld, %ld, %ld)\n", argv[0],
		   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	}
	break;
#endif // #ifdef KFS_ENABLED
    case nfs:
	rc = StubFileSystemNFS::_PrintStats();
	if (_FAILURE(rc)) {
	    printf("%s: invocation of StubFileSystemNFS::_printStat()"
		       " failed rc = (%ld, %ld, %ld)\n", argv[0],
		   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	}
	break;
    case invalid:
	printf("%s: type of FS not specified\n", argv[0]);
	usage(argv[0]);
	return 1;
    default:
	passertMsg(0, "Something wrong\n");
    }

    return 0;
}


