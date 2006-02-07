#ifndef _TEXTFILE_H_
#define _TEXTFILE_H_
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
 * Module Description: Virt file for storing small pieces of text
 *		       (stores shortest, alpha-numeric + punctuation
 *			string written to file)
 ***************************************************************************/

#include "FileTemplate.H"
#include <ctype.h>

class VirtFileText:public VirtNamedFile{
public:
#define TEXT_SIZE 256
    char text[TEXT_SIZE+1];

    DEFINE_GLOBAL_NEW(VirtFileText);
    VirtFileText(char *fname) {
	init((mode_t)0755);
	memset(text, 0, TEXT_SIZE);
	name = fname;
    }

    virtual SysStatus deleteFile() {
	breakpoint();
	return 0;
    }
    virtual SysStatus getServerFileType() {
	return VirtFSInfo;
    }
    virtual SysStatus _getMaxReadSize(uval &max, uval token = 0) {
	max = strnlen(text, TEXT_SIZE);
	return 0;
    }

    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _readOff (char *buf, uval length, uval offset,
				    uval userData, uval token = 0) {
	// no synchronization necessary (file system library provides sync)
	uval len = 0;
	_getMaxReadSize(len, token);

	if (offset > len) {
	    return _SERROR(2706, FileLinux::EndOfFile, 0);
	}

	if (offset+length > len) {
	    length = len - offset;
	}
	memcpy(buf, text+offset, length);
	return length;
    }

    // synchronous read interface where whole file is passed back
    virtual SysStatusUval _read (char *buf, uval buflength,
				 uval userData, uval token = 0)
    {
	// no synchronization necessary (file system library provides sync)
	uval len = 0;
	_getMaxReadSize(len, token);

	if (buflength > len) {
	    buflength = len;
	}
	memcpy(buf, text, buflength);
	return _SRETUVAL(buflength);
    }

    virtual SysStatusUval _write(const char *buf, uval length,
				 __in uval userData, uval token = 0) {
	if (length > TEXT_SIZE) {
	    length = TEXT_SIZE;
	}
	uval i = 0;
	while (i<length && (isalnum(buf[i]) || ispunct(buf[i])) ) {
	    ++i;
	}
	memcpy(text,buf,i);
	text[i]=0;
	return _SRETUVAL(length);
    }
    virtual SysStatus _open(uval oflags, uval userData, uval &token) {
	return 0;
    }
    virtual SysStatus _close(uval userData, uval token = 0) {
	// nothing to do on close
	return 0;
    }
};


#endif // #ifndef _TEXTFILE_H_
