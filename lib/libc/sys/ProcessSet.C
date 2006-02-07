/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessSet.C,v 1.15 2004/10/08 21:40:07 jk Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A class that keeps a really trival list of
 * processes, this is only accessed not from exception level, so it
 * should be paged.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "ProcessSet.H"
#include <misc/HashNonBlocking.I>

template class HashSNBBase<AllocGlobal,0, ProcessSet::defNumPid>;
template class HashNonBlockingBase<AllocGlobal,
    HashSNBBase<AllocGlobal, 0, ProcessSet::defNumPid>::
    HashSNBNode, ProcessSet::defNumPid>;
template class HashSimpleNonBlocking<ProcessID,
    BaseProcessRef, AllocGlobal, 0, ProcessSet::defNumPid>;
