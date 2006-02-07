#ifndef __FILE_LINUX_STREAM_TTY_H_
#define __FILE_LINUX_STREAM_TTY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxStreamTTY.H,v 1.11 2005/03/04 00:43:58 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality
 * **************************************************************************/

#include <io/FileLinuxStream.H>
class FileLinuxStreamTTY : public FileLinuxStream {
protected:
    DEFINE_GLOBAL_NEW(FileLinuxStreamTTY);
    virtual SysStatus init(FileLinuxRef &ref, ObjectHandle oh,
			   uval oflags, FileLinuxRef useR);
public:
    // instantiate a stream based on passed in oh
    static SysStatus Create(FileLinuxRef &r, ObjectHandle oh,
			    uval oflags, FileLinuxRef useR = NULL);

    virtual SysStatus dup(FileLinuxRef& newStream);
    virtual SysStatus ioctl(uval request, va_list args);

    virtual SysStatus lazyGiveAccess(sval fd);
    static  SysStatus LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh,
				 char *buf, uval bufLen);
    virtual SysStatus getStatus(FileLinux::Stat *status);

};

#endif /* #ifndef __FILE_LINUX_STREAM_TTY_H_ */
