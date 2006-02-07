/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DirBuf.C,v 1.7 2001/10/16 19:43:18 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Describes contents of a directory buffer
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <misc/StringTable.H>
#include <misc/StringTable.I>
#include "DirBuf.H"

template class _StrTable<DirBufEntry>;
