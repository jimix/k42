#ifndef PROC_FILE_MOUNTS_H
#define PROC_FILE_MOUNTS_H
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
 * Module Description: implements /proc/mounts
 ***************************************************************************/

#include <io/FileLinux.H>
#include <io/FileLinuxVirtFile.H>
#include <io/VirtFile.H>
#include <meta/MetaVirtFile.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include "ProcFileTemplate.H"

class ProcFileMounts : public ProcFSFile_DynamicRO {
public:
    DEFINE_GLOBAL_NEW(ProcFileMounts);

    ProcFileMounts() : ProcFSFile_DynamicRO("mounts") { /* empty body */ }

    virtual SysStatus _getMaxReadSize(uval &max, uval token=0);

    // synchronous read interface where whole file is passed back
    virtual SysStatusUval _read (char *buf, uval buflength, uval userData,
				 uval token=0);
};

#endif // #ifndef PROC_FILE_MOUNTS_H
