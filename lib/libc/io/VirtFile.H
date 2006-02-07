#ifndef __VIRT_FILE_H_
#define __VIRT_FILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VirtFile.H,v 1.6 2004/02/27 17:14:35 mostrows Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

class VirtFile;
typedef VirtFile** VirtFileRef;


// This interface is called by the fs-server, so having __in XHANDLE is
// kind of pointless --- what we really want is the fs-server to pass
// us the real xhandle of the client.  We make this generic and get a
// uval/void* for end-client-specific data instead.

class VirtFile : public Obj __xbase(Obj) {
  public:
    DEFINE_REFS(VirtFile);
__xpublic:
    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _readOff(__outbuf(__rc:length) char  *buf,
				   __in uval length, __in uval offset,
				   __in uval userData, __in uval token = 0)
	__xa(none) {
	return _SERROR(2157, 0, EINVAL);
    }
    // synchronous read interface without offset
    virtual SysStatusUval _read(__outbuf(__rc:buflength) char  *buf,
				__in uval buflength, __in uval userData,
				__in uval token = 0)
	__xa(none) {
	return _SERROR(2159, 0, EINVAL);
    }
    virtual SysStatusUval _write(__inbuf(length) const char *buf,
				 __in uval length, __in uval userData,
				__in uval token = 0)
	__xa(none) {
	return _SERROR(2158, 0, EINVAL);
    }
    virtual SysStatus _getMaxReadSize(__out uval &max_read_size,
				      __in uval token = 0)

	__xa(none) {
	return _SERROR(2161, 0, EINVAL);
    }
    virtual SysStatus _open(__in uval flags, __in uval userData,
			    __inout uval &token)
	__xa(none) {
	return _SERROR(2163, 0, EINVAL);
    }
    virtual SysStatus _close(__in uval userData, __in uval token = 0)
	__xa(none) {
	return _SERROR(2164, 0, EINVAL);
    }
    virtual SysStatus _setFilePosition(__in sval position, __in uval at,
				       __in uval userData, __in uval token = 0)
	__xa(none) {
	return _SERROR(2466, 0, ENOSYS);
    }

};
#endif /* #ifndef __VIRT_FILE_H_ */

