#ifndef __FILE_LINUX_SERVER_H_
#define __FILE_LINUX_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxServer.H,v 1.42 2005/07/14 21:46:13 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: This is a really trivial interface for a server
 * object, FIXME lots of ways, put in lots of implementation that should
 * really be specific to certain files.
 * **************************************************************************/

#ifndef EXPORT_FILELINUXSERVER
#define EXPORT_FILELINUXSERVER
#include <sys/stat.h>
#include <sys/fcntl.h>
#endif /* #ifndef EXPORT_FILELINUXSERVER */

#include <fslib/fs_defines.H>

class FileLinuxServer : public Obj __xbase(Obj) {
public:
__xprivate:
    virtual SysStatus _getStatus(__out struct stat& status,
				uval isPathBasedRequest = 0)=0;

    virtual SysStatus _fchown(__in uval uid, __in uval gid,
			      __CALLER_PID pid)=0;
    virtual SysStatus _fchmod(__in uval mode, __CALLER_PID pid)=0;
    virtual SysStatus _ftruncate(__in uval length, __CALLER_PID pid)=0;

    virtual SysStatus _getLengthOffset(__out uval &flength,
				       __out uval &offset,
				       __in uval isWrite,
				       __in uval opLen,
				       __XHANDLE xhandle)=0;

    virtual SysStatus _setLengthOffset(__in uval flength,
				       __in uval offset,
				       __XHANDLE xhandle)=0;

    // dup for files
    virtual SysStatus _dup(__in ProcessID pid,
			   __in uval originalUseType,
			   __out uval &newUseType,
			   __in uval flength,
			   __in uval offset,
			   __out ObjectHandle &oh,
			   __XHANDLE origXhandle)=0;

    // dup for directories
    virtual SysStatus _dup(__in ProcessID pid,
			   __out ObjectHandle &oh,
			   __XHANDLE origXhandle) {
	return _SERROR(2585, 0, EINVAL);
    }

    // System V discretionary locks (see fcntl(2))
    virtual SysStatus _getLock(__inout struct flock &lock) {
	return _SERROR(2060, 0, EINVAL);
    }
    virtual SysStatus _setLock(__inout struct flock &lock,
			       __CALLER_PID pid,
			       __XHANDLE xhandle) {
	return _SERROR(2059, 0, EINVAL);
    }
    virtual SysStatusUval _setLockWait(__inout struct flock &lock,
				       __out uval &key,
				       __CALLER_PID pid,
				       __XHANDLE xhandle) {
	return _SERROR(2460, 0, EINVAL);
    }

    virtual SysStatus _registerCallback(ObjectHandle callbackOH,
					__XHANDLE xhandle) {
	return _SERROR(2461, 0, EINVAL);
    }

    virtual SysStatus _registerCallback(__in ObjectHandle callbackOH,
					__in uval callBackType,
					__inout uval &useType,
					__out uval &flength,
					__out uval &offset,
					__XHANDLE xhandle) {
	return _SERROR(2106, 0, EINVAL);
    }

    virtual SysStatus _ackUseTypeCallBack(__in uval responseType,
					  __in uval fileLength,
					  __in uval fileOffset,
					  __XHANDLE xhandle) {
	return _SERROR(2191, 0, EINVAL);
    }

    // ------------------------------------------------------------
    // FIXME: delete all the ones below, make specific to some type
    // of file if at all
    // ------------------------------------------------------------
    virtual SysStatus _flush()=0;

    // these  return the amount of data read or written
    virtual SysStatusUval _write(__inbuf(len) const char *buf, __in uval len,
				 __XHANDLE xhandle)
	__xa(write) {
	return _SERROR(1587, 0, EINVAL);
    };

    virtual SysStatusUval _write(__inbuf(len) const char *buf, __in uval len,
				 __in uval offset,
				 __XHANDLE xhandle)
	__xa(write) {
	return _SERROR(2426, 0, EINVAL);
    };

    virtual SysStatusUval _read(__outbuf(__rc:len) char *buf, __in uval len,
				__XHANDLE xhandle)
	__xa(read) {
	return _SERROR(1588, 0, EINVAL);
    };

    virtual SysStatusUval _read(__outbuf(__rc:len) char *buf, __in uval len,
				__in uval offset,
				__XHANDLE xhandle)
	__xa(read) {
	return _SERROR(2427, 0, EINVAL);
    };

    virtual SysStatusUval _getDents(__outbuf(__rc:len) char *buf,
				    __in uval len, __XHANDLE xhandle,
				    __CALLER_PID pid)
	__xa(read) {
	return _SERROR(1589, 0, EINVAL);
    };

    virtual SysStatusUval _setFilePosition(__in sval position, __in uval at,
					   __XHANDLE xhandle) {
	return _SERROR(1590, 0, EINVAL);
    };

    virtual SysStatus _getFROH(__out ObjectHandle &FRoh, __CALLER_PID pd) {
	return _SERROR(1592, 0, EINVAL);
    };

    virtual SysStatus _ioctl(__in uval req,
			     __inoutbuf(size:size:size) char* buf,
			     __inout uval &size) {
	return _SERROR(1967, 0, EINVAL);
    };

    // blow away cached pages of a file
    virtual SysStatus _discardCachedPages() {
	return _SERROR(2215, 0, EINVAL);
    };

    virtual SysStatus _lazyGiveAccess(__XHANDLE xhandle,
				      __in sval file, __in uval type,
				      __in sval closeChain,
				      __inbuf(dataLen) char *data,
				      __in uval dataLen) {
#ifndef _IN_STUBGEN
	passertMsg(0, "NIY\n");
#endif /* #ifndef _IN_STUBGEN */
	return 0;
    }
    virtual SysStatus _getSharedBuf(__out ObjectHandle &oh, __out uval &offset,
				    __inout uval &length, __XHANDLE xhandle,
				    __CALLER_PID pid) {
	return _SERROR(2599, 0, EINVAL);
    }

    virtual SysStatus _syncSharedBuf(__in uval newLength, //Total len of file
				     __in uval start,     //Start mod data
				     __in uval modLen,    //Length mod data
				     __in uval release,
				     __XHANDLE xh,
				     __CALLER_PID pid) {
	return _SERROR(2598, 0, EINVAL);
    }

//#ifdef HACK_FOR_FR_FILENAMES
    virtual SysStatusUval _getFileName(__outbuf(__rc:bufsize) char *buf,
				       __in uval bufsize) {
	return _SERROR(2714, 0, EINVAL);
    }
//#endif // #ifdef HACK_FOR_FR_FILENAMES
};

#endif /* #ifndef __FILE_LINUX_SERVER_H_ */
