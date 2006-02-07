#ifndef __PROCESS_SET_H_
#define __PROCESS_SET_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessSet.H,v 1.23 2003/07/10 14:56:33 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A class that keeps a really trival list of
 * processes, this is only accessed not from exception level, so it
 * should be paged.
 * **************************************************************************/
#include <misc/HashNonBlocking.H>

class ProcessSet : public Obj {
public:
        enum {defNumPid=8};

protected:
    DEFINE_GLOBALPADDED_NEW(ProcessSet);
    // 0 is right shift used as a hash of the key
    // processid's are dense, don't need a shift
    HashSimpleNonBlocking
     <ProcessID, BaseProcessRef, AllocGlobal, 0, defNumPid> processList;

public:

    virtual SysStatus getRefFromPID(ProcessID pid, BaseProcessRef &pref)=0;

    virtual SysStatus addEntry(ProcessID pid, BaseProcessRef pref) {
	if (!processList.add(pid, pref)) {
	    return _SERROR(2379, 0, EEXIST);
	}
	return 0;
    }

    virtual SysStatus installEntry(ProcessID pid, BaseProcessRef pref) {
	// entry should already be there, and should have old pref == 0
	if (!processList.update(pid, pref) || (pref != 0)) {
	    return _SERROR(2380, 0, EINVAL);
	}
	return 0;
    }

    virtual SysStatus remove(ProcessID pid) {
	BaseProcessRef ref;
	if (processList.remove(pid, ref)) {
	    return 0;
	} else {
	    return _SERROR(1118, 0, EINVAL);
	}
    }

    // used for information support - to list information about all
    // know processes.
    // for performance reasons, results are NOT sorted by pid number
    // returns 1 if value found, 0 if no more, error if something changed
    // must pass in the last pid each time
    virtual SysStatus getNextPID(ProcessID& pid, BaseProcessRef &pref) {
	if(pid == ProcessID(-1)) {
	    return processList.getFirst(pid, pref);
	} else {
	    return processList.getNext(pid, pref);
	}
    }

    /*
     * servers can force the hash tables to grow so that
     * performance is good from the start.
     * Really most useful for benchmarks so you don't have to
     * wait for the system to adapt.
     */
    virtual SysStatus numberOfPids(uval numPids) {
	processList.numberOfEntries(numPids);
	return 0;
    }
};

#endif /* #ifndef __PROCESS_SET_H_ */
