/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcFileMeminfo.C,v 1.4 2004/12/14 19:04:37 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: File system specific interface to /proc
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <fslib/virtfs/FIVFAccessOH.H>
#include <fslib/DirLinuxFSVolatile.H>
#include <fslib/NameTreeLinuxFSVirtFile.H>
#include <stdio.h>
#include <sys/SystemMiscWrapper.H>
#include "ProcFileTemplate.H"
#include "ProcFileMeminfo.H"

const char * format_string =
		"        total:    used:    free:  shared: buffers:  cached:\n"
                "Mem:  %8Lu %8Lu %8Lu %8Lu %8Lu %8Lu\n"
                "Swap: %8Lu %8Lu %8Lu\n"
		"MemTotal:     %8lu kB\n"
		"MemFree:      %8lu kB\n"
		"MemShared:    %8lu kB\n"
		"Buffers:      %8lu kB\n"
		"SwapTotal:    %8lu kB\n"
		"SwapFree:     %8lu kB\n"
		"HugePages_Total: %5d\n"
		"HugePages_Free:  %5d\n"
		"Hugepagesize:    %5lu kB\n";

/* virtual */ SysStatusUval
ProcFileMeminfo::_getMaxReadSize(uval &max, uval token/*=0*/) {
    max = strlen(format_string) + 100;
    return 0;
}

// synchronous read interface where whole file is passed back
/* virtual */ SysStatusUval
ProcFileMeminfo::_read (char *buf, uval buflength, uval userData,
			uval token/*=0*/)
{
    SysStatus rc;
    long length;

    uval totalram, freeram, bufferram, sharedram;
    uval totalswap, freeswap;
    uval largePageTotal, largePageFree, largePageSize;
   
    bufferram = sharedram = 0;  
    totalswap = freeswap = 0;
    largePageTotal = largePageFree = largePageSize = 0;

    rc = DREFGOBJ(TheSystemMiscRef)->getBasicMemoryInfo(totalram, freeram,
			largePageSize, largePageTotal, largePageFree);
    _IF_FAILURE_RET_VERBOSE(rc);
    /*
    largePageTotal = 2;
    largePageFree = 2;
    largePageSize = 0x1000000;
    */

#define K(x) (x/1024)
    length = snprintf(buf, buflength, format_string,
		totalram, (totalram-freeram),freeram, sharedram, bufferram,
		(unsigned long) 0,  // no idea what 'cached' is!
		totalswap, (totalswap-freeswap), freeswap,
		K(totalram), K(freeram), K(sharedram), K(bufferram),
		K(totalswap), K(freeswap),
		largePageTotal, largePageFree,
		K(largePageSize)
		);
#undef K
    if (length < 0)
    {
	tassertWrn(1, "snprint failed");
	length = 0;
    }

    if ((uval)length > buflength) length = buflength;

    return _SRETUVAL((uval)length);
}
