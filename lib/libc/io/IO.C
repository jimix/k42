/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IO.C,v 1.39 2005/07/15 17:14:25 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: base io class implementation
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "io/IO.H"

SysStatus
IO::lock()
{
    return _SERROR(1021, 0, ENOSYS);
}

SysStatus
IO::flush()
{
    return _SERROR(1036, 0, ENOSYS);
}

SysStatus
IO::locked_flush(uval release)
{
    return _SERROR(1037, 0, ENOSYS);
}

SysStatus
IO::unLock()
{
    return _SERROR(1022, 0, ENOSYS);
}


SysStatusUval
IO::locked_readAlloc(uval /*len*/, char * &/*buf*/)
{
    return _SERROR(1023, 0, ENOSYS);
}

SysStatusUval
IO::locked_writeAlloc(uval /*len*/, char * &/*buf*/)
{
    return _SERROR(1020, 0, ENOSYS);
}

SysStatusUval
IO::locked_readRealloc(char */*prev*/, uval /*oldlen*/, uval /*nlen*/,
		       char * &/*buf*/)
{
    return _SERROR(1024, 0, ENOSYS);
}

SysStatusUval
IO::locked_writeRealloc(char */*prev*/, uval /*oldlen*/, uval /*nlen*/,
			char * &/*buf*/)
{
    return _SERROR(1147, 0, ENOSYS);
}

SysStatus
IO::locked_readFree(char */*ptr*/)
{
    return _SERROR(1025, 0, ENOSYS);
}

SysStatus
IO::locked_writeFree(char */*ptr*/)
{
    return _SERROR(1148, 0, ENOSYS);
}

SysStatusUval
IO::locked_readAllocAt(uval /*len*/, uval /*off*/, FileLinux::At /*at*/,
		       char * &/*buf*/)
{
    return _SERROR(1499, 0, ENOSYS);
}

SysStatusUval
IO::locked_writeAllocAt(uval /*len*/, uval /*off*/, FileLinux::At /*at*/,
			char * &/*buf*/)
{
    return _SERROR(1149, 0, ENOSYS);
}

SysStatusUval
IO::readAlloc(uval /*len*/, char * &/*buf*/)
{
    return _SERROR(1026, 0, ENOSYS);
}

SysStatusUval
IO::writeAlloc(uval /*len*/, char * &/*buf*/)
{
    return _SERROR(1150, 0, ENOSYS);
}

SysStatus
IO::readFree(char */*ptr*/)
{
    return _SERROR(1018, 0, ENOSYS);
}

SysStatus
IO::writeFree(char */*ptr*/)
{
    return _SERROR(1151, 0, ENOSYS);
}

SysStatusUval
IO::readAllocAt(uval /*len*/, uval /*off*/, FileLinux::At /*at*/,
	     char * &/*buf*/)
{
    return _SERROR(1019, 0, ENOSYS);
}

SysStatusUval
IO::writeAllocAt(uval /*len*/, uval /*off*/, FileLinux::At /*at*/,
	     char * &/*buf*/)
{
    return _SERROR(1152, 0, ENOSYS);
}

SysStatusUval
IO::read(void* buf, uval length, GenState &moreAvail)
{
    SysStatusUval rc;
    uval length_read;
    char* buf_read;
    lock();
    moreAvail.state = FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL;
    rc = locked_readAlloc(length, buf_read);
    if (_FAILURE(rc)) {
	unLock();
	//semantics shift from EOF error to 0 length on EOF
	if (_SCLSCD(rc) == FileLinux::EndOfFile) return 0;
	return rc;
    }
    length_read = _SGETUVAL(rc);
    memcpy(buf,buf_read,length_read);
    locked_readFree(buf_read);
    unLock();
    return rc;
}

SysStatusUval
IO::write(const void* buf, uval length, GenState &moreAvail)
{
    SysStatusUval rc;
    uval length_write;
    char* buf_write;
    lock();
    moreAvail.state = FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL;
    rc = locked_writeAlloc(length, buf_write);
    if (_FAILURE(rc)) {
	unLock();
	return rc;
    }
    length_write = _SGETUVAL(rc);
    memcpy(buf_write,buf,length_write);
    locked_writeFree(buf_write);
    unLock();
    return rc;
}

SysStatus
IO::ioctl(uval request, va_list args)
{
    return _SERROR(1786, 0, ENOTTY);
}
