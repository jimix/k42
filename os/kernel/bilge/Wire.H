#ifndef __WIRE_H_
#define __WIRE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Wire.H,v 1.35 2004/09/30 15:20:26 butrico Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: header file for writing to the WIRE
 * **************************************************************************/


// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

#include "meta/MetaWire.H"


class Wire : public Obj {
    // just a single instance of this class
    static Wire *TheWire;
protected:
    Wire() 
    {
      /* empty body */
    }
    DEFINE_GLOBALPADDED_NEW(Wire);
public:
    static void ClassInit(VPNum vp);
    DEFINE_ACCESS_SERVER(Wire,MetaObj::controlAccess,0);
__xpublic:
    virtual SysStatusUval write(__in uval channel,
				__inbuf(length) const char* buf,
				__in uval length);

    virtual SysStatusUval read(__in uval channel,
			       __outbuf(__rc:buflength) char* buf,
			       __in uval buflength);

    // allow user-level to start/stop thinwiremgr daemon
    static SysStatusUval SuspendDaemon();// returns > 0 if previously running
    static SysStatusUval RestartDaemon(); // returns > 0 if previously running

    static SysStatus _Create(__out ObjectHandle & oh, __CALLER_PID pid);
};

#endif /* #ifndef __WIRE_H_ */
