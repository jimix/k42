#ifndef _IO_FORWARDER_H_
#define _IO_FORWARDER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IOForwarder.H,v 1.3 2005/07/15 17:14:35 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Object to forward data from one IO object to another
 * **************************************************************************/
#include <sync/BLock.H>
#include <io/FileLinux.H>
#include <io/IORingBuffer.H>
#include <scheduler/Scheduler.H>
#include <sync/FairBLock.H>

struct IOForwarder: public IONotif{
    typedef FairBLock LockType;
    LockType lock;
    IORingBuffer buf;
    FileLinuxRef target;
    FileLinuxRef src;
    DEFINE_GLOBALPADDED_NEW(IOForwarder);
    IOForwarder();
    virtual ~IOForwarder();
    SysStatus init(FileLinuxRef from, FileLinuxRef to);
    virtual void processData();
    static void ProcessData(uval iofUval);
    virtual void ready(FileLinuxRef fl, uval state);
};

#endif /* _IO_FORWARDER_H_ */
