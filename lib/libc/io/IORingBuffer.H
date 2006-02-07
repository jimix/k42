/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IORingBuffer.H,v 1.7 2005/04/16 20:33:37 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface for simple byte ring buffer.
 ****************************************************************************/
#ifndef __IORING_BUFFER_H_
#define __IORING_BUFFER_H_
#include <sys/TAssert.H>
#include <alloc/alloc.H>

/**
 * A simple byte ring buffer
 */
class IORingBuffer{
protected:
    uval len;
    uval readNext;
    uval writeNext;
    char *buf;
public:
    virtual void init(uval bufSize, char* buf=NULL);
    virtual void destroy() {
	freeGlobal(buf,len);
    };

    virtual uval putData(const char* data, uval size);
    virtual uval getData(char* data, uval size);
    virtual uval getDelimData(char* data, uval size, char delim);

    virtual char* reserveSpace(sval &size);
    virtual void commitSpace(sval bytes);	// commit part of reservation

    virtual char* peekBytes(sval &bytes);	// Preview data
    virtual void consumeBytes(sval bytes);	// Make data disappear


    virtual char* unPutData(sval &length);  //Erase last written bytes

    uval bytesAvail() { return writeNext - readNext; };
    uval bufSize() { return len; };
    uval spaceAvail() { return bufSize() - bytesAvail(); };
};
#endif /* #ifndef __IORING_BUFFER_H_ */
