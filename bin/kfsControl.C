/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kfsControl.C,v 1.2 2005/08/03 18:10:28 neamtiu Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: place holder for miscellaneous kfs settings
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <stdlib.h>
#include <sys/systemAccess.H>

#ifdef KFS_ENABLED
#include <stub/StubFileSystemKFS.H>
#endif // #ifdef KFS_ENABLED

 static void
 usage(char *prog)
 {
     printf("Usage: %s \n"
	    "\t path --syncOn:       turn sync of meta-data on for path\n"
	    "\t path --syncOff:      turn sync of meta-data off for path\n"
	    "\t --getDebugMask: prints the current mask and the possible\n"
	    "\t                 debug class values\n"
	    "\t --setDebugMask mask: sets debug mask\n", prog);
 }

 static void
 setSync(char *path, uval value)
 {
    SysStatus rc;
    rc = StubFileSystemKFS::_SetSyncMetaData(path, strlen(path), value);
    if (_FAILURE(rc)) {
	printf("Invocation of _SetSyncMetaData failed with rc 0x%lx\n",
	       rc);
    }
}

static void getDebugMask(char *prog)
{
    SysStatus rc;
    uval mask;
    printf("Available debug classes will be printed on the console\n");
    rc = StubFileSystemKFS::_PrintDebugClasses(mask);
    if (_SUCCESS(rc)) {
	printf("Current mask is %ld\n", mask);
    } else {
	printf("%s: invocation of StubFileSystemKFS::_printStat()"
	       " failed rc = (%ld, %ld, %ld)\n", prog,
	       _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
    }
}

// returns 1 on sucess, 0 otherwise
static int setDebugMask(char *cmask, char *prog)
{
    char *endptr;
    int base = 10;
    if (strncmp(cmask, "0x", 2) == 0) {
	base = 16;
    }
    uval mask = strtol(cmask, &endptr, base);
    if (*endptr != '\0' || errno == ERANGE ) {
	printf("%s: error on mask %s provided\n", prog, cmask);
	usage(prog);
	return 0; // error
    }
    SysStatus rc = StubFileSystemKFS::_SetDebugMask(mask);
    if (_FAILURE(rc)) {
	printf("%s: invocation of StubFileSystemKFS::_SetDebugMask(%s)"
	       " failed rc = (%ld, %ld, %ld)\n", prog, cmask,
	       _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	return 0;
    }
    return 1;
}

int
main(int argc, char *argv[])
{
    NativeProcess();

#ifdef KFS_ENABLED
    if (argc < 2) {
	usage(argv[0]);
	return 1;
    }

    char *path = argv[1];
    SysStatus rc;
    rc = StubFileSystemKFS::_TestAlive(path, strlen(path));
    if (_FAILURE(rc)) {
	printf("Path %s doesn't correspond to a KFS file system\n", path);
	return 0;
    }

    // FIXME: instead of all this strcmp we should use getopt() stuff
    for (int i = 2; i < argc; i++) {
	if (strcmp(argv[i], "--syncOn") == 0) {
	    setSync(path, 1);
	} else if (strcmp(argv[i], "--syncOff") == 0) {
	    setSync(path, 0);
	} else if (strcmp(argv[i], "--getDebugMask") == 0) {
	    getDebugMask(argv[0]);
	} else if (strcmp(argv[i], "--setDebugMask") == 0) {
	    if (i + 1 < argc) {
		if (setDebugMask(argv[++i], argv[0]) == 0) {
		    // error
		    return 1;
		}
	    } else {
		printf("%s: Mask missing for --setDebugMask\n", argv[0]);
		usage(argv[0]);
		return 1;
	    }
	} else {
	    printf("Option %s not recognized\n", argv[i]);
	    usage(argv[0]);
	    return 1;
	}
    }

#else
    printf("KFS is not enabled\n");
#endif // #ifdef KFS_ENABLED

    return 0;
}
