#ifndef __LAZY_STATE_H_
#define __LAZY_STATE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LazyState.H,v 1.4 2003/04/01 01:18:21 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Preserves state of object reopened on demand
 * **************************************************************************/
#include <misc/ListSimpleKey.H>
class StubFileLinuxServerHolder;
class LazyState {
protected:
    class StubFileLinuxServerHolder;

    typedef BLock LockType;
    LockType      lock;			// lock on object

    // list of objects keyed by token
    ListSimpleKey<uval, StubFileLinuxServerHolder *, AllocGlobal> files;
    
    SysStatus lazyGiveAccess(sval file, StubFileLinuxServerHolder *s);
    SysStatus locked_lazyClose(sval file);

public:
    SysStatus lazyReOpen(ProcessID pid, sval file, uval &type, 
			 ObjectHandle &oh, char *data, uval &dataLen);
    
    SysStatus lazyClose(sval file) {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	return locked_lazyClose(file);
    }

    SysStatus lazyGiveAccess(sval file, uval type, ObjectHandle oh,
			     sval closeChain,
			     AccessRights match, AccessRights nomatch,
			     char *data, uval dataLen);

    SysStatus copyState(LazyState *from);

    void detach();
};
#endif /* #ifndef __LAZY_STATE_H_ */
