/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: invokes MountPointMgr::_Bind, which provides
 * Plan9-like bind call (duplicates some piece of existing name space
 * at another point in the name space)
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <io/FileLinux.H>
#include <sys/MountPointMgrClient.H>
#include <sys/systemAccess.H>

static void usage(char *prog)
{
    printf("Usage: %s <oldname> <newname>\n"
	       "e.g. %s /nfs/tmp /tmp\n", prog, prog);
}

int
main(int argc, char *argv[])
{
    NativeProcess();

    if (argc != 3) {
	usage(argv[0]);
	return 1;
    }

    SysStatus rc;

    PathNameDynamic<AllocGlobal> *oldPath, *newPath;
    uval oldlen, newlen, maxpthlen;

    rc = FileLinux::GetAbsPath(argv[1], oldPath, oldlen, maxpthlen);
    if (_FAILURE(rc)) {
	printf("%s:GetAbsPath for %s failed\n", argv[0], argv[1]);
	return 1;
    }
    rc = FileLinux::GetAbsPath(argv[2], newPath, newlen, maxpthlen);
    if (_FAILURE(rc)) {
	printf("%s:GetAbsPath for %s failed\n", argv[0], argv[2]);
	return 1;
    }

    // FIXME: assuming we want isCoverable 0
    rc = DREFGOBJ(TheMountPointMgrRef)->bind(oldPath, oldlen, newPath, newlen,
					     0);

    if (_SUCCESS(rc)) {
	printf("%s: Success\n", argv[0]);
    } else {
	printf("%s: bind of %s to %s failed\n", argv[0], argv[2], argv[1]);
	return 1;
    }

    return 0;
}
