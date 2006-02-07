/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SocketCreate.C,v 1.25 2004/09/30 11:21:54 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 ****************************************************************************/

#include "kernIncs.H"
#include <bilge/SocketWire.H>
#include <linux/LinuxSock.H>
#include <stub/StubKBootParms.H>
#include <io/SocketServer.H>
#include <stub/StubSocketServer.H>
#include <meta/MetaSocketServer.H>
#include <io/PacketServer.H>
#include <stub/StubPacketServer.H>
#include <meta/MetaPacketServer.H>
#include <io/StreamServer.H>
#include <stub/StubStreamServer.H>
#include <meta/MetaStreamServer.H>
#include <bilge/IPSock.H>
#include <meta/MetaIPSock.H>
#include <io/FileLinuxStream.H>

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
		__in uval domain, __in uval type,
		__in uval protocol, __CALLER_PID processID)
{
    switch (transport) {
	case WIRE:
	{
	    return SocketWire::Create(oh, clientType, domain, type,
				      protocol, processID);
	    break;
	}
	case LINUX:
	{
	    return LinuxSock::Create(oh, clientType, domain, type,
				     protocol, processID);
	    break;
	}
	default:
	    tassertWrn(0, "Unknown transport %ld\n", transport);
	    return -1;
    }
    return 0;
}

void
IPSock::ClassInit()
{
    char buf[32];

    StubKBootParms::_GetParameterValue("K42_IOSOCKET_TRANSPORT", buf, 32);

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
	LinuxSock::ClassInit();
	break;
    }
    default:
	tassertWrn(0, "Unknown transport %ld\n", transport);
    }
}
