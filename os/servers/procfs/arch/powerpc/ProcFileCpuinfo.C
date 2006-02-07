/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
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
#include "ProcFileCpuinfo.H"
#include <misc/hardware.H>

#define CPU_NAME_LEN 	32

/* virtual */ SysStatusUval
ProcFileCpuinfo::_getMaxReadSize(uval &max, uval token/*=0*/) {
    max = PPCPAGE_LENGTH_MAX - 2*sizeof(uval) - sizeof(__XHANDLE);
    return 0;
}

/* virtual */ SysStatusUval
ProcFileCpuinfo::getCpuNameString(uval pvr, char * cpuNameString,
			    uval cpuNameStringLength)
{
    /*
     * Code names are already available in the linux source.
     */
    switch (PVR_VER(pvr)) {
    case 0x0001:
	strncpy(cpuNameString, "601", cpuNameStringLength);
	break;
    case 0x0003:
	strncpy(cpuNameString, "603", cpuNameStringLength);
	break;
    case 0x0004:
	strncpy(cpuNameString, "604", cpuNameStringLength);
	break;
    case 0x0006:
	strncpy(cpuNameString, "603e", cpuNameStringLength);
	break;
    case 0x0007:
	strncpy(cpuNameString, "603ev", cpuNameStringLength);
	break;
    case 0x0009:
	strncpy(cpuNameString, "604e", cpuNameStringLength);
	break;
    case 0x000A:
	strncpy(cpuNameString, "604ev", cpuNameStringLength);
	break;
    case 0x0020:
	strncpy(cpuNameString, "403GC", cpuNameStringLength);
	break;
    case PV_NORTHSTAR:
	strncpy(cpuNameString, "RS64-II (northstar)", cpuNameStringLength);
	break;
    case 0x0032:	/* Apache (see main.c) */
	strncpy(cpuNameString, "??", cpuNameStringLength);
	break;
    case PV_PULSAR:
	snprintf(cpuNameString, cpuNameStringLength, "%s", "RS64-III (pulsar)");
	break;
    case PV_POWER4:	/* GP */
	strncpy(cpuNameString, "POWER4 (gp)", cpuNameStringLength);
	break;
    case PV_ICESTAR:
	strncpy(cpuNameString, "RS64-III (icestar)", cpuNameStringLength);
	break;
    case PV_SSTAR:
	strncpy(cpuNameString, "RS64-IV (sstar)", cpuNameStringLength);
	break;
    case PV_POWER4p:	/* GQ */
	strncpy(cpuNameString, "POWER4+ (gq)", cpuNameStringLength);
	break;
    case PV_970:
	strncpy(cpuNameString, "PPC970", cpuNameStringLength);
	break;
    case PV_970FX:
	strncpy(cpuNameString, "PPC970FX", cpuNameStringLength);
	break;
    case PV_POWER5:	/* GR */
	strncpy(cpuNameString, "POWER5 (gr)", cpuNameStringLength);
	break;
    case PV_630:
    case PV_630p:
	snprintf(cpuNameString, cpuNameStringLength, "%s", "POWER3 (630+)");
	break;
    default:	/* defaul string as used by the linux kernel */
	snprintf(cpuNameString, cpuNameStringLength, "%s", "POWER4 (compatible)");
	break;
    }

    return 0;
}

/* virtual */ SysStatusUval
ProcFileCpuinfo::getCpuRevisionString(uval pvr, char * cpuRevisionString,
			    uval cpuRevisionStringLength)
{
    switch (PVR_VER(pvr)) {
    case 0x0020:	// 403
    default:
	snprintf(cpuRevisionString, cpuRevisionStringLength, "%lu.%lu",
			(PVR_REV(pvr) >> 8) & 0xFF,
			PVR_REV(pvr) & 0xFF);
	break;
    }

    return 0;
}

// synchronous read interface where whole file is passed back
/* virtual */ SysStatusUval
ProcFileCpuinfo::_read (char *buf, uval buflength, uval userData,
			uval token/*=0*/)
{
    SysStatus rc;
    long length;
    long written_length;
    VPNum i, numberCpus;
    uval cVer, cpuClockFreq;

    numberCpus = KernelInfo::CurPhysProcs();

    written_length = 0;
    for ( i = 0; i < numberCpus; i++ ) {
	char cpuNameString[CPU_NAME_LEN];
        char cpuRevisionString[CPU_NAME_LEN];

	rc = DREFGOBJ(TheSystemMiscRef)->getCpuInfo(i, cVer, cpuClockFreq);
	_IF_FAILURE_RET_VERBOSE(rc);
	rc = getCpuNameString(cVer, cpuNameString, CPU_NAME_LEN);
	_IF_FAILURE_RET_VERBOSE(rc);
	rc = getCpuRevisionString(cVer, cpuRevisionString, CPU_NAME_LEN);
	_IF_FAILURE_RET_VERBOSE(rc);

	length =  snprintf(buf, buflength,
		"processor\t: %ld\n"
		"cpu\t\t: %s\n"
		"clock\t\t: %ldMHz\n"
		"revision\t: %s\n\n",
		i, cpuNameString, cpuClockFreq/1000000, cpuRevisionString );

	if (length < 0) {
	    tassertWrn(0, "snprint failed\n");
	    length = 0;
	    break;
	} else if ((uval)length >= buflength) {  // truncated
	    tassertWrn(0, "snprint buffer too small\n");
	    written_length += buflength;
	    buf[written_length-1] = '\0';
	    break;
	}
	buf += length;
	buflength -= length;
	written_length += length;
    }

    return _SRETUVAL((uval)written_length);
}
