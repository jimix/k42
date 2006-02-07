#ifndef __PROCESS_SET_KERN_H_
#define __PROCESS_SET_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessSetKern.H,v 1.11 2001/10/05 21:50:17 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Kernel version of ProcessSet.  Keeps extra state
 * **************************************************************************/

#include <sys/ProcessSet.H>

class ProcessSetKern : public ProcessSet __xbase(Obj) {
    ProcessID nextProcessID;
protected:
    DEFINE_GLOBALPADDED_NEW(ProcessSetKern);
    ProcessSetKern() : ProcessSet() { nextProcessID = 1; }

public:
    //FIXME: COSMGR changed these to virtual
    //       must ensure that all external methods are virtual
    virtual SysStatus getRefFromPID(ProcessID pid, BaseProcessRef &pref) {
	if (processList.find(pid, pref)) {
	    return 0;
	} else {
	    return _SERROR(1117, 0, EINVAL);
	}
    }
    virtual SysStatus getNextProcessID(ProcessID &processID);
    static void ClassInit(VPNum vp);

__xpublic:
    // FIXME eventually we will use type to determine the type
    //       of processWrapper to make, for now we only have one
    //       process type so we don't need it
    static SysStatus RegisterPIDGetOH(__out ObjectHandle &oh,
				      __out TypeID &procType,
				      __in ProcessID processID,
				      __CALLER_PID caller);
};

#endif /* #ifndef __PROCESS_SET_KERN_H_ */
