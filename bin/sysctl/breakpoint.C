/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Force input onto system console
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "FileTemplate.H"


class BreakProc: public VirtFileUval {
public:
    BreakProc(char* fileName):
	VirtFileUval(fileName,"NFS Revalidation Toggle (not available): ") {};
    virtual SysStatusUval getUval() {
	return 0;
    }
    virtual SysStatus setUval(uval val) {
	return DREFGOBJ(TheSystemMiscRef)->breakpointProc(val);
    }
    DEFINE_GLOBAL_NEW(BreakProc);
};


void breakpoint_init(FileInfoVirtFSDir* sysFS) {
    addFile(new BreakProc("breakpoint"), sysFS);
}
