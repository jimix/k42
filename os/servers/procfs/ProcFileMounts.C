/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementing /proc/mounts
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include "ProcFileMounts.H"

/* virtual */ SysStatusUval
ProcFileMounts::_getMaxReadSize(uval &max, uval token/*=0*/) {
    max = PPCPAGE_LENGTH_MAX - 2*sizeof(uval) - sizeof(__XHANDLE);
    return 0;
}

// synchronous read interface where whole file is passed back
/* virtual */ SysStatusUval
ProcFileMounts::_read (char *buf, uval buflength, uval userData,
		       uval token/*=0*/)
{
    /* FIXME: for now we're returning fakeinformaation, just trying to make sure
     * that we don't get tons of warnings about this file not being available when
     * we run dbench. I proper implementation is not hard to do, because the
     * MountPointManager has all the info available. The only issue there is to deal
     * with covered mount points */


    // returning fake info, but enough for statvfs find what it needs here
    char content[128];
    sprintf(content, "rootfs /knfs rootfs rw 0 0\n"
	    "rootfs /     rootfs r 0 0\n"
	    "/dev/root /kkfs rw 0 0\n");
    uval len = strlen(content);
    uval length = (buflength < len)  ? buflength : len;

    memcpy(buf, content, length);
    
    /* FIXME: should contact mount point manager and get the whole thing!
     *        for now returning empty */ 
    return length;
}
