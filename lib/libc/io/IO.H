#ifndef __IO_H_
#define __IO_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IO.H,v 1.67 2003/05/01 15:05:48 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the fundamental interface all IO
 * objects support
 * **************************************************************************/

#include <cobj/Obj.H>
#include <sys/ProcessLinux.H>
#include <io/FileLinux.H>

class IO : public Obj {
public:
    /*
     * allocate a buffer of date for reading.  The buffer
     * will actually contain the values "read"
     * returns the actual length if at end of file
     */
    virtual SysStatusUval readAlloc(uval len, char * &buf);
    virtual SysStatusUval readAllocAt(uval len, uval off, FileLinux::At at,
				      char * &bf);

    // indicate that the buffer is no longer needed
    virtual SysStatus readFree(char *ptr);

    // allocate an output buffer to be filled
    virtual SysStatusUval writeAlloc(uval len, char * &buf);
    virtual SysStatusUval writeAllocAt(uval len, uval off, FileLinux::At at,
				       char * &bf);
    // indicate that the buffer has been written
    virtual SysStatus writeFree(char *ptr);

    // force end to end completion of writes
    virtual SysStatus flush();

    /*
     * read and write - by default implemented using alloc interface
     * but some IO objects may override to go directly to server
     */
    virtual SysStatusUval read(void* buf, uval length, GenState &moreAvail);
    virtual SysStatusUval write(const void* buf, uval length, GenState &moreAvail);
    virtual SysStatusUval getDents(void* buf, uval length) {
	return _SERROR(1340, 0, ENOTDIR);
    }

    /*
     * Operations same as above alloc ones, but lock is acquired and
     * freed explicitly before calling them. They typically assert lock
     * acquisition.
     */
    virtual SysStatus lock();
    virtual SysStatus unLock();

    virtual SysStatusUval locked_readAlloc(uval len, char * &buf);
    virtual SysStatusUval locked_readAllocAt(uval len, uval off,
					     FileLinux::At at, char *&bf);
    virtual SysStatusUval locked_readRealloc(char *prev, uval oldlen,
					     uval newlen, char * &buf);
    virtual SysStatus locked_readFree(char *ptr);
    virtual SysStatusUval locked_writeAllocAt(uval len, uval off,
					      FileLinux::At at, char *&bf);
    virtual SysStatusUval locked_writeAlloc(uval len, char * &buf);
    virtual SysStatusUval locked_writeRealloc(char *prev, uval oldlen,
					      uval newlen, char * &buf);
    virtual SysStatus locked_writeFree(char *ptr);
    virtual SysStatus locked_flush(uval release = 0);

    // the way we spell close
    virtual SysStatus detach()=0;

    // duplicate the IO object - client only
    virtual SysStatus dup(IORef& newIORef)=0;

    // returns success or failure, function "fct" called (under
    // protection of object lock if "locked" is true) to
    // indicate state, fct is also recorded by object and called on
    // subsequent changes in availability
    virtual SysStatus ioctl(uval request, va_list args);
};


#endif /* #ifndef __IO_H_ */
