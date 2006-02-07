#ifndef __DISPATCHER_MGR_H_
#define __DISPATCHER_MGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherMgr.H,v 1.5 2004/01/07 14:05:55 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Keeps track of all dispatchers.
 * **************************************************************************/

#include <sys/Dispatcher.H>

class DispatcherMgr : public Obj {
public:
    struct Published {
	XBaseObj *xhandleTable;
	uval xhandleTableLimit;
    };

private:
    typedef BLock LockType;
    LockType lock;
    Published published;
    Dispatcher *dispatcher[Scheduler::RDLimit][Scheduler::VPLimit];
    DEFINE_GLOBALPADDED_NEW(DispatcherMgr);

    void init();
    void locked_publish();

    virtual SysStatus enter(DispatcherID dspid, Dispatcher *dsp);

public:
    virtual SysStatus publishXHandleTable(XBaseObj *table, uval limit);
    virtual SysStatus publishProgInfo(ProcessID procID, char *name);

    virtual SysStatus getDispatcher(DispatcherID dspid, Dispatcher* &dsp) {
	RDNum rd; VPNum vp;
	SysTypes::UNPACK_DSPID(dspid, rd, vp);
	if ((rd >= Scheduler::RDLimit) || (vp >= Scheduler::VPLimit)) {
	    return _SERROR(2614, 0, ESRCH);
	}
	dsp = dispatcher[rd][vp];
	return 0;
    }

    virtual SysStatus postFork();
    static void ClassInit(VPNum vp);
};

#endif /* #ifndef __DISPATCHER_MGR_H_ */
