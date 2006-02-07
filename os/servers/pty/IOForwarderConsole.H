#ifndef _IO_FORWARDER_CONSOLE_H_
#define _IO_FORWARDER_CONSOLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IOForwarderConsole.H,v 1.2 2005/07/15 17:14:35 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Object to forward data from one IO object to another
 * **************************************************************************/
#include "IOForwarder.H"
struct IOForwarderConsole: public IOForwarder{
    //These are copies of args from ready();
    DEFINE_GLOBALPADDED_NEW(IOForwarderConsole);
    SysStatus init(FileLinuxRef from);
    virtual void processData();
};

#endif /* _IO_FORWARDER_CONSOLE_H_ */
