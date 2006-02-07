/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SocketCreate.C,v 1.16 2004/09/30 11:21:54 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 ****************************************************************************/

#include "kernIncs.H"
#include <bilge/SocketWire.H>
#include <linux/LinuxUDP.H>
#include <linux/LinuxTCP.H>
#include <stub/StubKBootParms.H>
#include <io/SocketServerServer.H>
#include <stub/StubSocketServer.H>
#include <meta/MetaSocketServer.H>
#include <io/StreamServer.H>
#include <stub/StubStreamServer.H>
#include <meta/MetaStreamServer.H>
#include <bilge/WireUDP.H>
#include <bilge/WireTCP.H>

/*
 * These are the two functions to replace if you want to change sockets to
 * go to something other than thinwire (actually, last stays same, but for
 * consistency should be moved to new socket class).
 */

// default transport value

//FIXME: Presently, we have two transports for IOSockets:
//       SocketWire and SocketLinux.
//       The type that is used depends on the execution
//       environment. Usually one uses IOSocketWire in a
//       simulation environment and SocketLinux in a
//       hardware environment.
//       A static variable is defined (transport) and it is
//       set by an enviroment variable at initialization time.
//       This allows one to easily change the transport used
//       for a "run".

// defined transport values
enum {
    NONE = 0,
    WIRE = 1,
    LINUX = 2
};

static uval transport = WIRE;

/* static */ SysStatus
IPSock::_Create(__out ObjectHandle &oh, __out uval &clientType,
		__in uval domain, __in uval type, __in uval protocol,
		__CALLER_PID processID)
{
    /* FIXME -- X86-64 */
    SysStatus rc =0 ;
    ObjRef sref;
    clientType = TransStreamServer::TRANS_VIRT;
    switch (transport) {
	case WIRE:
	{
	    SocketWire *nSck;
	    rc = SocketWire::Create(sref, nSck, domain, type, protocol);
	    if (_FAILURE(rc)) return rc;
	    break;
	}
	case LINUX:
	{
	    // LinuxSock *nSck;
	    passertWrn(0, "FIXME XXX");
	    // FIXME: XXX rc = LinuxSock::Create(sref, nSck, type);
	    if (_FAILURE(rc)) return rc;
	    break;
	}
	default:
	    tassertWrn(0, "Unknown transport %ld\n", transport);
    }

    rc = DREF(sref)->giveAccessByServer(oh, processID);
    if (_FAILURE(rc)) return rc;

    return 0;
}

void
IPSock::ClassInit()
{
#if defined(TARGET_amd64) && defined(CONFIG_SIMICS)
    err_printf("Invalid K42_IOSOCKET_TRANSPORT value\n");
    err_printf("Valid values are: wire, linux\n");
    transport = NONE;
#else /* #if defined(TARGET_amd64) && ... */
    char buf[32];

    StubKBootParms::_GetParameterValue("K42_IOSOCKET_TRANSPORT", buf);

    if (strcmp(buf, "wire") == 0 || strcmp(buf, "WIRE") == 0) {
	cprintf("IOSocket::transport = IOSocket::WIRE\n");
	    transport = WIRE;
    } else if (strcmp(buf, "linux") == 0 || strcmp(buf, "LINUX") == 0) {
	cprintf("IOSocket::transport = IOSocket::LINUX\n");
	transport = LINUX;
    } else {
	cprintf("Invalid K42_IOSOCKET_TRANSPORT value: %s\n", buf);
	cprintf("Valid values are: wire, linux\n");
	transport = NONE;
    }
#endif /* #if defined(TARGET_amd64) && ... */
    MetaPacketServer::init();
    MetaSocketServer::init();
    MetaIPSock::init();

    switch (transport) {
    case WIRE:
    {
	SocketWire::ClassInit();
	break;
    }
    case LINUX:
    {
      passertWrn(0, "FIXME XXX");
      // FIXME: XXX LinuxSock::ClassInit(vp);
	break;
    }
    default:
	tassertWrn( 0,"Unknown transport %ld\n", transport);
    }

}
