/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: chario.C,v 1.43 2005/04/27 03:57:26 okrieg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: User-level versions of getchar() and putchar().
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <misc/baseStdio.H>
#include <io/FileLinux.H>
#include <stub/StubStreamServer.H>
#include <cobj/XHandleTrans.H>

extern "C" ssize_t __k42_linux_read(int fd, void *buf, size_t count);

void
consoleWrite(const char *str, uval len)
{
    GenState moreAvail;
    StubStreamServer cons(StubObj::UNINITIALIZED);

    cons.initOHWithPID(0,XHANDLE_MAKE_NOSEQNO(CObjGlobals::ReservedForConsole));
    // should always succeed to console
    while (len) {
	if (len>=512) {
	    cons._sendto((char*)str, 512, 0, NULL, 0, 512, moreAvail,
			 NULL /* controlData */, 0 /* controlDataLen */);
	    str+=512;
	    len-=512;
	} else {
	    cons._sendto((char*)str, len, 0, NULL, 0, len, moreAvail,
			 NULL /* controlData */, 0 /* controlDataLen */);
	    len = 0;
	}
    }
}
