#ifndef __IPCTARGET_TABLE_H_
#define __IPCTARGET_TABLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IPCTargetTable.H,v 1.11 2003/06/04 14:17:31 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines CommID-to-ProcessAnnex mapping mechanism.
 * **************************************************************************/

#include <alloc/MemoryMgrPrimitive.H>

/*
 * Locking: only accessed at exception level - caller must
 * disable.
 */
class IPCTargetTable {
    friend void genConstants(void);
    friend class ExceptionLocal;
    static const uval RD_HASH_OFFSET = 4;
    static const uval INITIAL_TABLE_SIZE = (uval(1) << (RD_HASH_OFFSET + 1));
    uval _tableSize;
    uval _tableIndexMask;	// used in C code
    uval _tableOffsetMask;	// used in assembler code
    ProcessAnnex **_table;
public:
    void init(MemoryMgrPrimitive *memory);
    uval hash(CommID commID);
    void enter(ProcessAnnex *pa);
    void remove(ProcessAnnex *pa);
    ProcessAnnex *lookupWild(CommID commID);
    ProcessAnnex *lookupExact(CommID commID);
};

#endif /* #ifndef __IPCTARGET_TABLE_H_ */
