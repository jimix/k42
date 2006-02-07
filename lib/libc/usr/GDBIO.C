/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: GDBIO.C,v 1.38 2005/04/27 03:57:26 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A class that defines the IO interface from
 * application level for debugger
 * **************************************************************************/

#include "sys/sysIncs.H"
#include <io/FileLinux.H>
#include <io/Socket.H>
#include <sys/KernelInfo.H>
#include <stub/StubIPSock.H>
#include "GDBIO.H"
#include <sys/socket.h>
#include <netinet/tcp.h>

/*static*/ uval GDBIO::Initialized = 0;
/*static*/ uval GDBIO::DebugMeValue = 0;
/*static*/ StubSocketServer *GDBIO::Sock = NULL;

// FIXME: change all places where use this
#define MAX_READ_WRITE 3072

/* static */ uval
GDBIO::GDBRead(char *buf, uval len)
{
    SysStatusUval rc;
    GenState moreAvail;
    if (len > MAX_READ_WRITE) len = MAX_READ_WRITE;
    do {
	uval len1=0;
	uval len2=0;
	uval controlDataLen = 0;
	rc = Sock->_recvfrom(buf, len, 0, NULL, len1, len2, moreAvail,
			     NULL /* controlData */, controlDataLen);
    } while (_SUCCESS(rc) && _SGETUVAL(rc) == 0);

    return rc;
}

/* static */ void
GDBIO::GDBWrite(char *buf, uval nbytes)
{
    SysStatus tot = 0;
    while (nbytes > 0) {
	SysStatus rc ;
	uval amt;
	GenState moreAvail;
	amt = MIN(nbytes,MAX_READ_WRITE);
	rc = Sock->_sendto((char*)buf+tot, amt, 0, NULL, 0, amt, moreAvail,
			   NULL /* controlData */, 0 /* controlDataLen */);
	if (_SUCCESS(rc)) {
	    tot = tot + _SGETUVAL(rc);
	    nbytes -= _SGETUVAL(rc);
	} else {
	    // this is not strictly kosher, but it seems to be the most
	    // reliable thing to do
	    if (_SGENCD(rc) == EAGAIN) {
		continue;
	    } else {
		err_printf("ASF:loopWrite: error <%ld %ld %ld> dw %ld n %ld\n",
		       _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc), tot, amt);
		// this probably should do something to say
		// some valid data written
		return;
	    }
	}
    }
    return;
}

#include <misc/baseStdio.H>

#if __BYTE_ORDER == __BIG_ENDIAN
#define htonl(x)	(x)
#else
#define htonl(x) (__extension__			\
	 ({ uval32 __nl = (x);			\
	 ((((__nl) & 0xff000000) >> 24) |	\
	  (((__nl) & 0x00ff0000) >>  8) |	\
	  (((__nl) & 0x0000ff00) <<  8) |	\
	  (((__nl) & 0x000000ff) << 24)); }))
#endif

static void
StartConnection(StubSocketServer *Sock, uval startport)
{
    SysStatus rc;
    uval port;
    ObjectHandle stubOH;
    uval clientType;
    rc = StubIPSock::_Create(stubOH, clientType, AF_INET, SOCK_STREAM,0);
    tassertMsg((_SUCCESS(rc)),"socket() failed for port: %lx\n",rc);
    Sock->setOH(stubOH);

    port = startport;
    while (port < startport+100) {
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htonl(port);
	uval len = sizeof(sin);
	rc = Sock->_bind((char*)&sin, len);
	if (_SUCCESS(rc)) break;
	port ++;
    }

    uval mypid =(uval)DREFGOBJ(TheProcessRef)->getPID();

    err_printf("User program pid=%lu(0x%lx) connecting to GDB via port %lu\n",
	       mypid, mypid, port);
    rc = Sock->_listen(4);

    tassert((_SUCCESS(rc)), err_printf("listen() failed\n"));

    GenState moreAvail;
    do {
	char buf[64];
	uval bufLen = 64;
	rc = Sock->_accept(stubOH, buf, bufLen, moreAvail);
    } while (_FAILURE(rc) || stubOH.invalid());
    tassert((_SUCCESS(rc)), err_printf("accept() failed: %lX\n",rc));

    rc = Sock->_releaseAccess();
    tassert((_SUCCESS(rc)), err_printf("release() failed\n"));

    Sock->setOH(stubOH);

    int val=1;
    rc = Sock->_setsockopt(IPPROTO_TCP, TCP_NODELAY,
			   (char *)&val, sizeof(int));
    if (!_SUCCESS(rc)) {
	err_printf("setsockopt failed with error <%ld %ld %ld>\n",
		   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
    }
}

/* static */ void
GDBIO::ClassInit()
{
    tassertMsg(!Initialized, "GDBIO already initialized\n");
    Initialized = 1;
    if (Sock == NULL) {
	Sock = new StubSocketServer(StubObj::UNINITIALIZED);
    }
    return StartConnection(Sock, 2223);
}

/* static */ void
GDBIO::PostFork()
{
    Initialized = 0;
}

/* static */ void
GDBIO::GDBClose()
{
    SysStatus rc;

    rc = Sock->_releaseAccess();
    tassert((_SUCCESS(rc)), err_printf("release() failed\n"));
    Initialized = 0;
}

/* static */ uval
GDBIO::IsConnected()
{
    return Initialized;
}
