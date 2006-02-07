#ifndef __BUFFER_H_
#define __BUFFER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Buffer.H,v 1.4 2005/08/29 14:13:55 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <io/FileLinux.H>
#include <io/IOBufferList.H>
#include <fslib/fs_defines.H>
#include <trace/traceFS.h>
#include <stub/StubFR.H>
#include <stub/StubFileLinuxServer.H>
#include <sys/ppccore.H>
#include <sys/ShMemClnt.H>
#include <fslib/fs_defines.H>

class Buffer {
    // THIS IS TEMPORARY. This class will be broken in subclasses for memory
    // mapped and small file support

protected:
    StubFR stubFr;
    StubFileLinuxServer stub;

    IOBufListMap  bufList;		// buffers with outstanding refs

    uval          isFlengthOffsetKnown; // if we did get from the file system
                                        // server information about file length
                                        // and position
    uval	  flength;		// current file length know
    uval          flengthFR;            // last file length value passed
                                        // to FR

    uval          initialOffset;        // information retrieved from file
                                        // server before setting up buffer
                                        // (for NON_SHARED only)

    typedef enum {NONE, MMAP, SMALL_BUFFER} BufferType;
    uval bufferType;

    /* cbuf invariants for FileLinux mapped:
     * fileOffset is 0
     * len is length of region - larger than file
     * off is current read/write position
     * off+cnt is current (local) file length IF length >= offset (truncate
     *         doesnt' change offset)
     */
    MapBufInfo    cbuf;
    // data structures for small buffers (small file support)
    /* FIXME: this sucks: no encapsulation at all! But I'm trying to minimize
     * structuring changes in case things break while we have these deadlines*/
    ShMemClntRef shmemRef;
    char *smallBuf;
    uval smallOffset;
    /* FIXME: smallBufSize is redundant ... flength and openFlags give
     * us this info.
     *
     * DONTFIXME: I don't buy it.  It may be useful to consider that
     * the amount of data being stored for a file does not match the
     * file length.  This is likely to occur with small files where
     * the transport mechanism transfers an amount of data that need
     * not be equal to file length.  This discrepancy may exist
     * because data transfer (through _syncSharedBuf, is accomplished
     * independently of _setLengthOffset).
     */
    uval smallBufSize;
    uval smallBufRefs;
    uval smallBufModified;   // Number of bytes needing sync'ing
    uval smallBufModStart;   // Offset of first changed byte

    enum {SEND, RCV};

    uval useType;
    uval openFlags;

    SysStatus init(uval flags, ObjectHandle oh, uval ut, uval currentOpLen);

    SysStatus attachNewCbuf(uval offset, uval size, uval noDetachPrev=0);
    SysStatus initCbuf();
    SysStatus bufferSetup();
    SysStatus initSmallBuf();
    SysStatus smallBufToMmap();
    SysStatusUval readSmallBuffer(uval len, char * &buf, ThreadWait **tw);
    SysStatusUval writeSmallBuffer(uval len, char * &buf, ThreadWait **tw);

    SysStatusUval transportSmallBuf(uval operation);

public:
    DEFINE_GLOBAL_NEW(Buffer);
    Buffer(uval flags, ObjectHandle oh, uval ut, uval currentOpLen);
    Buffer(uval flen, uval initOff, uval flags, ObjectHandle oh,
	   uval ut, uval currentOpLen);
    virtual ~Buffer();

    virtual SysStatus getLengthOffset(uval &flen, uval &offset);

    virtual SysStatus isMMap() { return bufferType == MMAP; }
    virtual SysStatus isSmallBuffer() { return bufferType == SMALL_BUFFER; }

    virtual SysStatusUval readAlloc(uval len, char * &buf, ThreadWait **tw);
    virtual SysStatus readFree(char *ptr);
    virtual SysStatusUval writeAlloc(uval len, char * &buf, ThreadWait **tw);
    virtual SysStatus writeFree(char *ptr);
    virtual SysStatus flush(uval release=0);
    virtual SysStatus ftruncate(uval length);
    virtual SysStatusUval setFilePosition(sval position, FileLinux::At at);

//    virtual SysStatus setOffset(uval position);
    virtual SysStatus afterServerUpdate();
    virtual SysStatus switchToShared(uval &flen, uval &off);
};

#endif // #ifndef __BUFFER_H_
