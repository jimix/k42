#ifndef FILETEMPLATE_H_
#define FILETEMPLATE_H_
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
 * Module Description: Templates(base classes) for sysctl file implementations
 ***************************************************************************/

#include <io/FileLinux.H>
#include <io/FileLinuxVirtFile.H>
#include <io/VirtFile.H>
#include <meta/MetaVirtFile.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>
#include <sys/SystemMiscWrapper.H>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fslib/virtfs/FileInfoVirtFS.H>

class VirtNamedFile:public VirtFSFile{
public:
    char* name;
};

class VirtFileUval:public VirtNamedFile{
public:
    const char* header;
    uval decimal;
    DEFINE_GLOBAL_NEW(VirtFileUval);
    // decimal == 1 --> use decimal output, not hex
    VirtFileUval(char *fname, const char *preMsg, uval dec=0) {
	init((mode_t)0666);
	decimal = dec;
	name = fname;
	header = preMsg;
    }

    virtual SysStatus deleteFile() {
	breakpoint();
	return 0;
    }
    virtual SysStatus getServerFileType() {
	return VirtFSInfo;
    }
    virtual SysStatus _getMaxReadSize(uval &max, uval token = 0) {
	max = 20;
	if (header) {
	    max+= strlen(header);
	}
	return 0;
    }

    virtual SysStatusUval getUval()=0;
    virtual SysStatus setUval(uval val)=0;

    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _readOff(char *buf, uval length, uval offset,
				   uval userData, uval token = 0) {
	// no synchronization necessary (file system library provides sync)
	char msg[64];
	uval max = 0;
	_getMaxReadSize(max, token);

	uval val = getUval();
	uval len;
	if (header) {
	    if (decimal) {
		len = snprintf(msg,length,"%s%ld\n", header, val);
	    } else {
		len = snprintf(msg,length,"%s0x%016lx\n", header, val);
	    }
	} else {
	    if (decimal) {
		len = snprintf(msg,length,"%ld\n", val);
	    } else {
		len = snprintf(msg,length,"0x%016lx\n",val);
	    }
	}
	if (offset > max) {
	    return _SERROR(2156, FileLinux::EndOfFile, 0);
	}
	memcpy(buf, msg+offset, len);

	return len;
    }

    // synchronous read interface where whole file is passed back
    virtual SysStatusUval _read (char *msg, uval length, uval userData,
				 uval token = 0)
    {
	uval val = getUval();
	uval len;
	if (header) {
	    if (decimal) {
		len = snprintf(msg,length,"%s%ld\n", header, val);
	    } else {
		len = snprintf(msg,length,"%s0x%016lx\n", header, val);
	    }
	} else {
	    if (decimal) {
		len = snprintf(msg,length,"%ld\n", val);
	    } else {
		len = snprintf(msg,length,"0x%016lx\n",val);
	    }
	}
	return _SRETUVAL(len);
    }

    virtual SysStatusUval _write(const char *buf, uval length,
				 __in uval userDataa, uval token = 0) {
	char msg[length+1];
	memcpy(msg,buf,length);
	msg[length]=0;
	uval val = strtoul(msg,NULL,0);
	SysStatusUval rc = setUval(val);
	if (_SUCCESS(rc)) {
	    return length;
	}
	return rc;
    }
    virtual SysStatus _open(uval oflags, uval userDataa, uval &token) {
	return 0;
    }
    virtual SysStatus _close(uval userDataa, uval token = 0) {
	// nothing to do on close
	return 0;
    }
};

extern SysStatus addFile(VirtNamedFile* vf, FileInfoVirtFSDir* sysFS);


#endif // #ifndef FILETEMPLATE_H_
