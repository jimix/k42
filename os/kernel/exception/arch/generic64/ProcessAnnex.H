#ifndef __PROCESS_ANNEX_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __PROCESS_ANNEX_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessAnnex.H,v 1.3 2002/05/02 20:15:24 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Defines the machine-dependent parts of the
 *                     exception-level kernel structure that represents a
 *                     virtual processor.
 * **************************************************************************/

#include <misc/hardware.H>

struct EntryPointLauncher {
public:
    void set(uval userMode, EntryPointDesc entry) {
    }
    void setException(codeAddress handler) {
    }
};

struct ProcessAnnexMachine {
    uval dispatcherPhysAddr;
    uval excStatePhysAddr;

public:
    void init(uval userMode, Dispatcher *disp, SegmentTable *segTable);

    void setExcStateOffset(uval offset) {
    }

    void switchExcStateSaveArea() {
    }
};
