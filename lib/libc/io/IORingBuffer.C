/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IORingBuffer.C,v 1.8 2005/04/15 17:39:35 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface for simple byte ring buffer.
 ****************************************************************************/

#include <sys/sysIncs.H>
#include "IORingBuffer.H"
void
IORingBuffer::init(uval bSize, char* buffer)
{
    if (buffer) {
	buf = buffer;
    } else {
	buf = (char*)allocGlobal(bSize);
    }

    tassertMsg(buf!=NULL, "allocation of ring buffer failed\n");

    len = bSize;
    readNext = 0;
    writeNext = 0;
};

char*
IORingBuffer::unPutData(sval &length)
{
    if (length<0 || (sval)bytesAvail()<length) {
	length = bytesAvail();
    }

    uval bytes_to_end = len - readNext%len;

    if (bytesAvail() - bytes_to_end < (uval)length) {
	length = bytesAvail() - bytes_to_end;
    }

    char* p = &buf[(writeNext - length) % len];
    writeNext -= length;

    if (writeNext-readNext==0) {
	readNext= 0;
	writeNext= 0;
    }

    return p;
}


uval
IORingBuffer::putData(const char* const data, uval size)
{
    uval copy = 0;
    while (size>0 && (bytesAvail() < len)) {
	uval end = writeNext + len - writeNext%len;
	if (readNext+len < end) {
	    end = readNext + len;
	}

	if (writeNext+size<end) {
	    end = writeNext+size;
	}

	memcpy(&buf[writeNext % len],&data[copy], end-writeNext);
	size-= end-writeNext;
	copy+= end-writeNext;
	writeNext = end;
    }
    if (writeNext-readNext==len) {
	readNext= readNext%len;
	writeNext= (writeNext%len)+len;
    }
    return copy;
}

char*
IORingBuffer::reserveSpace(sval &size)
{
    char* p = &buf[writeNext%len];

    if (size>(sval)spaceAvail() || size==-1) {
	size = spaceAvail();
    }
    if (writeNext+size>=len) {
	size = len - writeNext;
    }
    tassert(!(spaceAvail() && size==0),
	    err_printf("Reserved no space, space is available\n"));
    return p;
}

void
IORingBuffer::commitSpace(sval size)
{
    if (size>(sval)spaceAvail() || size==-1) {
	size = spaceAvail();
    }

    writeNext += size;

    if (writeNext-readNext==len) {
	readNext= readNext%len;
	writeNext= (writeNext%len)+len;
    }
}

void
IORingBuffer::consumeBytes(sval size)
{
    if (size>(sval)bytesAvail() || size==-1) {
	size = bytesAvail();
    }

    readNext += size;
    if (writeNext-readNext==0) {
	readNext= 0;
	writeNext= 0;
    }

}


uval
IORingBuffer::getData(char* const data, uval size)
{
    uval copy = 0;
    while (size>0 && (bytesAvail() > 0)) {
	uval end = readNext + len - readNext%len;

	if (writeNext<end) {
	    end = writeNext;
	}

	if (readNext+size<end) {
	    end = readNext+size;
	}

	memcpy(&data[copy],&buf[readNext % len], end - readNext );
	size-= end - readNext;
	copy+= end - readNext;
	readNext= end;
    }
    if ( writeNext - readNext == 0 ) {
	readNext= 0;
	writeNext= 0;
    }
    return copy;
}

uval
IORingBuffer::getDelimData(char* const data, uval size, char delim)
{
    uval copy = 0;
    while (size>0 && (bytesAvail() > 0)) {
	uval end = readNext + len - readNext%len;

	if (writeNext<end) {
	    end = writeNext;
	}

	if (readNext+size<end) {
	    end = readNext+size;
	}

	for (uval i=0; i<end-readNext; ++i) {
	    data[copy] = buf[readNext%len];
	    ++copy;
	    ++readNext;
	    --size;

	    // Easiest way to break out of 2 loops.
	    if (data[copy]==delim)
		goto abort;
	}
    }
 abort:
    if (writeNext-readNext==0) {
	readNext = 0;
	writeNext = 0;
    }
    return copy;
}

char*
IORingBuffer::peekBytes(sval &bytes)
{
    uval end = readNext + len - readNext%len;
    char* p=&buf[readNext%len];
    if ((sval)bytesAvail()<bytes || bytes==-1) {
	bytes = bytesAvail();
    }

    if (writeNext<end) {
	end = writeNext;
    }

    if (end - readNext < (uval)bytes) {
	bytes = end - readNext;
    }
    return p;
}
