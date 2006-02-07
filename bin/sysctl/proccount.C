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


class ProcCount: public VirtFileUval {
public:
    ProcCount(char* fileName):
	VirtFileUval(fileName, NULL, 1) {};
    virtual SysStatusUval getUval() {
	return DREFGOBJ(TheProcessRef)->ppCount();
    }
    virtual SysStatus setUval(uval val) {
	return 0;
    }
    DEFINE_GLOBAL_NEW(ProcCount);
};


void proccount_init(FileInfoVirtFSDir* sysFS) {
    addFile(new ProcCount("proccount"), sysFS);
}
