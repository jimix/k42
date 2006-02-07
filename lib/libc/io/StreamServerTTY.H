#ifndef __STREAM_SERVER_TTY_H_
#define __STREAM_SERVER_TTY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServerTTY.H,v 1.7 2003/01/05 02:41:40 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Stream interface exported by TTY Servers
 * **************************************************************************/
#include <io/StreamServer.H>
#include <meta/MetaStreamServerTTY.H>
#include <stub/StubStreamServerTTY.H>
#include <alloc/AllocPool.H>
#ifndef EXPORT_STREAMSERVERTTY
#define EXPORT_STREAMSERVERTTY
#include <io/GenState.H>
#include <cobj/BaseObj.H>
#include <sys/types.h>
#include <sys/ProcessLinux.H>
#endif /* #ifndef EXPORT_STREAMSERVERTTY */

class StreamServerTTY : public StreamServer __xadef(attach) {
public:
#ifndef _IN_STUBGEN
    DEFINE_GLOBAL_NEW(StreamServerTTY);
    DEFINE_ACCESS_SERVER(StreamServerTTY,
			 MetaObj::write|MetaObj::read|
			 MetaObj::controlAccess|MetaStreamServerTTY::attach,
			 MetaStreamServerTTY::attach);
#endif /* #ifndef _IN_STUBGEN */
__xprivate:
    // these both return the amount of data read or written
    virtual SysStatusUval _write(__inbuf(len) const char *buf,
				 __in uval len, __out GenState &moreAvail,
				 __XHANDLE xhandle)
	__xa(write)=0;

    virtual SysStatusUval _read(__outbuf(__rc:len) char *buf, __in uval len,
				__out GenState &moreAvail, __XHANDLE xhandle)
	__xa(read)=0;

};

#endif /* #ifndef __STREAM_SERVER_TTY_H_ */
