/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: parseExecutable.C,v 1.3 2002/11/15 15:43:23 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: has functions that takes the address of
 *                     the beginning of an pe file and returns
 *                     pointers to its data, text, entry, bss,
 *                     and other critical components.  Code
 *                     modified from code in elf.c taken from
 *                     Toronto
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/sysTypes.H>
#include <misc/execute.H>

SysStatus
PutAuxVector(uval memStart, uval &offset, BinInfo &info)
{
    return 0;
}

sval
parseExecutable(uval vaddr, BinInfo *info)
{
    return 0;
}
