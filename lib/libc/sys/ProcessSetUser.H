#ifndef __PROCESS_SET_USER_H_
#define __PROCESS_SET_USER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessSetUser.H,v 1.10 2002/05/02 02:00:51 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: User version of ProcessSet.
 *                     Implements getRefFromPID different from kernel version.
 * **************************************************************************/

#include <sys/ProcessSet.H>

class ProcessSetUser : public ProcessSet {
protected:
    uval waitingForWrapper;
    DEFINE_GLOBALPADDED_NEW(ProcessSetUser);
    ProcessSetUser() : ProcessSet() {
	waitingForWrapper = 0;
    }
public:
    //FIXME: COSMGR changed these to virtual
    //       must ensure that all external methods are virtual
    virtual SysStatus getRefFromPID(ProcessID pid, BaseProcessRef &pref);

    static void ClassInit(VPNum vp);
};

#endif /* #ifndef __PROCESS_SET_USER_H_ */
