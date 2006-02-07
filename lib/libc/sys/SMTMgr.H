#ifndef SMTMGR_DEFH
#define SMTMGR_DEFH
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SMTMgr.H,v 1.5 2002/10/10 13:08:24 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared-Memory Transport Manager manages connections
 *		       of MemTrans objects between processes
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#include <meta/MetaObj.H>
#include <cobj/XHandleTrans.H>
#include <cobj/TypeMgr.H>
#include <sync/BLock.H>
#include <io/MemTrans.H>
#include <cobj/ObjectHandle.H>
#include <misc/AutoList.H>

class SMTMgr {
public:
    DEFINE_NOOP_NEW(SMTMgr);
    struct SMTHolder:public AutoListNode{
	uval key;
	XHandle	    remote;
	MemTransRef mtr;
	SMTHolder(MemTransRef m, XHandle x, uval k)
	    :key(k), remote(x), mtr(m) {
	      /* empty body */
	    }
	DEFINE_GLOBALPADDED_NEW(SMTHolder);
	~SMTHolder() { detach(); };
    };

    LockedAutoListHead holderList;

    // Retrieves local SMT object for specified pid/key
    // Returns xhandle used by remote end
    // remoteSMT is also an input --- it sets the xhandle associated
    // with the pid/key if it is currently not valid.
    SysStatus getLocalSMT(MemTransRef &mtr, XHandle &remoteSMT, uval key);



    SysStatus removeSMT(MemTransRef mtr);
    SysStatus addSMT(MemTransRef mtr, XHandle remoteSMT, uval key);
    SysStatus getMemTrans(ObjectHandle &oh,
			  uval key,
			  XHandle xhandle,
			  ProcessID pid);
};

#endif /* #ifndef SMTMGR_DEFH */
