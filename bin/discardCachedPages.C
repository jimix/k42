/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: discardCachedPages.C,v 1.3 2005/06/28 19:42:45 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A real simple implementation of cat.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <io/FileLinux.H>
#include <io/FileLinuxFile.H>
#include <sys/systemAccess.H>

#include <fcntl.h>

static void usage(char *prog)
{
    printf("Usage: %s <file name>\n e.g. %s /nfs/tmp\n", prog, prog);
}

int
main(int argc, char *argv[])
{
    NativeProcess();

    SysStatus rc;
    FileLinuxRef fileRef;

    if (argc != 2) {
	usage(argv[0]);
	return 1;
    }

    printf("discardingCached pages on %s\n", argv[1]);

    rc = FileLinux::Create(fileRef, argv[1], O_RDWR, 0644);
    if (_FAILURE(rc)) {
	err_printf("open of file %s failed\n", argv[1]);
	return (rc);
    }

    DREF((FileLinuxFile **)fileRef)->discardCachedPages();

    DREF(fileRef)->destroy();
    return 0;
}
