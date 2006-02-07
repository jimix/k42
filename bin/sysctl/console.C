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
 * Module Description: Force input onto system console: FIXME: this
 * should really be called forcing input into the SystemControl file,
 * not necessarily associated with console at all.
 **************************************************************************/

#include <sys/sysIncs.H>
#include "FileTemplate.H"

class VirtConsoleFile:public VirtNamedFile{
public:
    DEFINE_GLOBAL_NEW(VirtConsoleFile);
    VirtConsoleFile(char *fname) {
	init((mode_t)0222);
	name = fname;
    }

    virtual SysStatus deleteFile() {
	return 0;
    }
    virtual SysStatus getServerFileType() {
	return VirtFSInfo;
    }
    virtual SysStatus _getMaxReadSize(uval &max, uval token=0) {
	return 0;
    }
    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _readOff (char *buf, uval length, uval offset,
				    uval userData, uval token=0) {
	return _SRETUVAL(0);
    }

    // synchronous read interface where whole file is passed back
    virtual SysStatusUval _read (char *buf, uval buflength,
				 uval userData, uval token=0)
    {
	return _SRETUVAL(0);
    }

    virtual SysStatusUval _write(const char *buf, uval length,
				 __in uval userData, uval token=0) {
	DREFGOBJ(TheSystemMiscRef)->systemControlInsert((char*)buf,length);
	return length;
    }
    virtual SysStatus _open(uval oflags, uval userData, uval &token) {
	return 0;
    }
    virtual SysStatus _close(uval userData, uval token=0) {
	// nothing to do on close
	return 0;
    }
};


void console_init(FileInfoVirtFSDir* sysFS) {
    addFile(new VirtConsoleFile("console"), sysFS);
}
