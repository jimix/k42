#ifndef __GDBIO_H_
#define __GDBIO_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: GDBIO.H,v 1.10 2005/02/09 18:45:40 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A class that defines the IO interface for debugger
 * **************************************************************************/

#include <stub/StubSocketServer.H>
#include <sys/IOChan.H>

class GDBIO {
private:
    static uval Initialized;
    static uval DebugMeValue;
    static StubSocketServer *Sock;
public:
    static IOChan* GDBChan;
    // stub reads from debugger
    static uval GDBRead(char *buf, uval length);

    // stub writes to debugger
    static void GDBWrite(char *buf, uval length);

    static void ClassInit();
    static void PostFork();
    static void GDBClose();
    static uval IsConnected();
    enum {NO_DEBUG, DEBUG, USER_DEBUG};
    static uval SetDebugMe(uval db) {
 	uval old = DebugMeValue;
 	DebugMeValue = db;
 	return old;
    }
    static uval GetDebugMe(void) {
 	return DebugMeValue;
    }
};

#endif /* #ifndef __GDBIO_H_ */
