#ifndef PROCFILETEMPLATE_H_
#define PROCFILETEMPLATE_H_
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
 * Module Description: Templates(base classes) for proc file implementations
 ***************************************************************************/

#include <io/FileLinux.H>
#include <io/FileLinuxVirtFile.H>
#include <io/VirtFile.H>
#include <meta/MetaVirtFile.H>
#include <fslib/virtfs/FileInfoVirtFS.H>

class ProcFSFile : public VirtFSFile {
public:
    const char* filename;

    ProcFSFile(char *fname) : filename(fname) { /*empty body*/ }

    virtual SysStatus _open(uval oflags, uval userData, uval &token)
	 { return 0; }
    virtual SysStatus deleteFile() { return 0; }   
    virtual SysStatus _close(uval userData, uval token) { return 0; }
};

class ProcFSFileRO : public ProcFSFile {
public:
    ProcFSFileRO(char *fname) : ProcFSFile(fname) {
	init((mode_t) (S_IREAD|S_IRGRP|S_IROTH) ); /* mode is read-only */
    }

    virtual uval isReadOnly() { return 1; };

    virtual SysStatusUval _write(const char *buf, uval length,
                               __in uval userData) {
        return _SERROR(2803, 0, EPERM);
    };
};

/*
 * a class that allows read-only access to static text information
 */
class ProcFSFile_StaticTextRO : public ProcFSFileRO {
protected:
#define TEXT_SIZE 256
    char text[TEXT_SIZE];
    uval textlen;

    SysStatus init(const char *string, uval slen)
    {
	SysStatus rc;

	textlen = slen;

	if (textlen>TEXT_SIZE) {
       	    tassertWrn(slen<=TEXT_SIZE, "woops, too long string\n");
	    textlen = TEXT_SIZE;
	    rc = -1;
	} else {
	    rc = 0;
	}

        memcpy(text,string,textlen);

	return rc;
    }

public:
    DEFINE_GLOBAL_NEW(ProcFSFile_StaticTextRO);

    ProcFSFile_StaticTextRO(char *fname) : ProcFSFileRO(fname) {
	memset(text, 0, TEXT_SIZE);
	textlen = 0;
    }

    // use this constructor when the string contains nulls, e. g,
    // /proc/<pid>/cmdline
    ProcFSFile_StaticTextRO(char *fname, const char *string, uval slen) :
	 ProcFSFileRO(fname)
    {
	this->init(string, slen);
    }

    ProcFSFile_StaticTextRO(char *fname, const char *string)
	: ProcFSFileRO(fname)
    {
	this->init(string, strlen(string)+1);
    }

    virtual SysStatus _getMaxReadSize(uval &max, uval token=0) {
	max = textlen;
	return 0;
    }

    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _readOff(char *buf, uval length, uval offset,
                                   uval userData, uval token=0) {
	uval len = 0;
	_getMaxReadSize(len, token);

	if (offset > len) {
	    return _SERROR(2706, FileLinux::EndOfFile, 0);
	}

	if (offset+length > len) {
	    length = len - offset;
	}
	memcpy(buf, text+offset, length);
	return _SRETUVAL(length);
    }

    // synchronous read interface where whole file is passed back
    virtual SysStatusUval _read (char *buf, uval buflength, uval userData,
				 uval token=0)
    {
	uval len = 0;
	_getMaxReadSize(len, token);

	if (buflength > len) {
	    buflength = len;
	}
	memcpy(buf, text, buflength);
	return _SRETUVAL(buflength);
    }

};

class ProcFSFile_DynamicRO : public ProcFSFileRO {
public:
    DEFINE_GLOBAL_NEW(ProcFSFile_DynamicRO);

    ProcFSFile_DynamicRO(char *fname) : ProcFSFileRO(fname) { /* empty body */ }
};

#endif // #ifndef PROCFILETEMPLATE_H_
