/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: GDBIOKern.C,v 1.11 2005/04/27 14:19:20 apw Exp $
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
#include <sys/thinwire.H>
#include "GDBIO.H"
#include <sys/IOChan.H>

/*static*/ IOChan* GDBIO::GDBChan = NULL;
/*static*/ uval GDBIO::Initialized = 0;
/*static*/ uval GDBIO::DebugMeValue = 0;
/*static*/ StubSocketServer *GDBIO::Sock = NULL;

/* static */ uval
GDBIO::GDBRead(char *buf, uval length)
{
    uval ret;
    ret = GDBChan->read(buf, length);
    return ret;
}

/* static */ void
GDBIO::GDBWrite(char *buf, uval length)
{
    GDBChan->write(buf, length);
}

/* static */ void
GDBIO::PostFork()
{
    passertMsg(0, "not available for kernel.\n");
}

/* static */ void
GDBIO::ClassInit()
{
    tassertMsg(!Initialized, "GDBIO already initialized\n");
    Initialized = 1;
    err_printf("Kernel Connecting to GDB via thinwire channel\n"
               "(use kvictim to find gdb target machine and port)\n");
}

/* static */ void
GDBIO::GDBClose()
{
    Initialized = 0;
}

/* static */ uval
GDBIO::IsConnected()
{
    return Initialized;
}
