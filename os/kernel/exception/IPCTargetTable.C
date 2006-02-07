/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IPCTargetTable.C,v 1.15 2003/06/04 14:17:30 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines CommID-to-ProcessAnnex mapping mechanism.
 * **************************************************************************/

#include "kernIncs.H"
#include "exception/ProcessAnnex.H"
#include "exception/IPCTargetTable.H"

void
IPCTargetTable::init(MemoryMgrPrimitive *memory)
{
    uval space;

    _tableSize = INITIAL_TABLE_SIZE;
    _tableIndexMask = _tableSize - 1;		// mask for table indices
    _tableOffsetMask = _tableIndexMask * sizeof(ProcessAnnex *);
						// mask for table offsets

    memory->alloc(space, _tableSize * sizeof(ProcessAnnex *), 8);
    _table = (ProcessAnnex **) space;

    for (uval i = 0; i < _tableSize; i++) {
	_table[i] = NULL;
    }
}

uval
IPCTargetTable::hash(CommID commID)
{
    return SysTypes::PID_FROM_COMMID(commID) ^
	    (SysTypes::RD_FROM_COMMID(commID) << RD_HASH_OFFSET);
}

void
IPCTargetTable::enter(ProcessAnnex *pa)
{
    tassertMsg(!hardwareInterruptsEnabled(),
	       "IPCTargetTable::enter must be called disabled\n");
    uval index = hash(pa->commID) & _tableIndexMask;
    pa->ipcTargetNext = _table[index];
    _table[index] = pa;
}

void
IPCTargetTable::remove(ProcessAnnex *pa)
{
    tassertMsg(!hardwareInterruptsEnabled(),
	       "IPCTargetTable::remove must be called disabled\n");
    uval index = hash(pa->commID) & _tableIndexMask;
    ProcessAnnex *last_pa = NULL;
    ProcessAnnex *cur_pa = _table[index];
    while (cur_pa != pa) {
	tassertMsg(cur_pa != NULL, "ProcessAnnex %p not found\n", pa);
	last_pa = cur_pa;
	cur_pa = cur_pa->ipcTargetNext;
    }
    if (last_pa != NULL) {
	last_pa->ipcTargetNext = pa->ipcTargetNext;
    } else {
	_table[index] = pa->ipcTargetNext;
    }
}

ProcessAnnex *
IPCTargetTable::lookupWild(CommID commID)
{
    tassertMsg(SysTypes::WILD_COMMID(commID) == commID,
	       "lookupWild target commID is not wild-carded.\n");
    ProcessAnnex *pa = _table[hash(commID) & _tableIndexMask];
    while ((pa != NULL) && (commID != SysTypes::WILD_COMMID(pa->commID))) {
	pa = pa->ipcTargetNext;
    }
    return pa;
}

ProcessAnnex *
IPCTargetTable::lookupExact(CommID commID)
{
    tassertMsg(SysTypes::WILD_COMMID(commID) != commID,
	       "lookupExact target commID is wild-carded.\n");
    ProcessAnnex *pa = _table[hash(commID) & _tableIndexMask];
    while ((pa != NULL) && (commID != pa->commID)) {
	pa = pa->ipcTargetNext;
    }
    return pa;
}
