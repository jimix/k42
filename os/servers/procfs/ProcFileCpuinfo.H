#ifndef PROCFILECPUINFO_H
#define PROCFILECPUINFO_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implements /proc/cpuinfo
 ***************************************************************************/

#include <io/FileLinux.H>
#include <io/FileLinuxVirtFile.H>
#include <io/VirtFile.H>
#include <meta/MetaVirtFile.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>
#include <sys/SystemMiscWrapper.H>
#include "ProcFileTemplate.H"

class ProcFileCpuinfo : public ProcFSFile_DynamicRO {
private:
    virtual SysStatusUval getCpuNameString(uval CpuVersion,
					char *cpuNameString,
					uval cpuNameStringLength);
    virtual SysStatusUval getCpuRevisionString(uval CpuVersion,
		char *cpuRevisionString, uval cpuRevisionStringLength);

public:
    DEFINE_GLOBAL_NEW(ProcFileCpuinfo);

    ProcFileCpuinfo() : ProcFSFile_DynamicRO("cpuinfo") { /* empty body */ }

    virtual SysStatus _getMaxReadSize(uval &max, uval token=0);

    // synchronous read interface where whole file is passed back
    virtual SysStatusUval _read (char *buf, uval buflength, uval userData,
				 uval token=0);
};

#endif // #ifndef PROCFILECPUINFO_H
