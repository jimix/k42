/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Wire.C,v 1.37 2005/02/23 16:20:54 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: object for reading/writing a "wire" channel
 * The wire is the bringup external i/o interface supported by simulators
 * or by a serial connection to the thinwire program on another computer
 * **************************************************************************/

#include "kernIncs.H"
#include "Wire.H"
#include "ThinWireMgr.H"
#include <sys/thinwire.H>
#include <misc/hardware.H>
#include <sys/ProcessSet.H>
#include <cobj/CObjRootSingleRep.H>

Wire *Wire::TheWire;

void
Wire::ClassInit(VPNum vp)
{
    if (vp==0) {
	TheWire = new Wire();
	MetaWire::init();
	CObjRootSingleRepPinned::Create(TheWire);
    }
}

SysStatus
Wire::_Create(ObjectHandle & oh, ProcessID processID)
{
    return TheWire->giveAccessByServer(oh, processID);
}

SysStatusUval
Wire::write(uval channel, const char *buf, uval length)
{
    return ThinWireChan::twChannels[channel]->write(buf, length);
}

SysStatusUval
Wire::read(uval channel, char *buf, uval buflength)
{
    uval length = ThinWireChan::twChannels[channel]->read(buf, buflength);
    if (length > 0) {
	buf[length] = 0;			// put in string terminator
    }
    return length;
}

/* static */ SysStatusUval
Wire::SuspendDaemon()
{
    return ThinWireMgr::SuspendDaemon();
}

/* static */ SysStatus
Wire::RestartDaemon()
{
    return ThinWireMgr::RestartDaemon();
}
