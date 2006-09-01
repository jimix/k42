#ifndef IO_FORK_MANAGER_DEFH
#define IO_FORK_MANAGER_DEFH
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IOForkManager.H,v 1.12 2003/04/17 02:26:06 dilma Exp $
 *****************************************************************************/
#include <defines/experimental.H>

class IOForkManager {
    // returns back size of space used
    virtual SysStatusUval copyEntries(uval buf, uval size)=0;
    virtual SysStatus preFork(XHandle target)=0;
public:
    static IOForkManager *obj;
    static SysStatus IsInitialized(void) {
	return (obj != NULL);
    }
    static SysStatusUval CopyEntries(uval buf, uval size) {
	return obj->copyEntries(buf, size);
    }
    static SysStatusUval PreFork(XHandle target) {
	return obj->preFork(target);
    }
    DEFINE_NOOP_NEW(IOForkManager);
    virtual ~IOForkManager() { }
};
#endif /* #ifndef IO_FORK_MANAGER_DEFH */
