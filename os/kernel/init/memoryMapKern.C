/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: memoryMapKern.C,v 1.16 2001/11/01 01:09:25 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Check validity of well-known symbols.
 * **************************************************************************/

#include "kernIncs.H"
#include "init/memoryMapKern.H"
#include <sys/Dispatcher.H>
#include <sys/ppccore.H>
#include "exception/ExceptionLocal.H"

/*
 * These global pointers are declared simply for debugging convenience.  No
 * actual C++-level definitions for the processor-specific structures exist,
 * so the debugger has no type information for the structures themselves.
 */
const ExceptionLocal	*exceptionLocalDbg	= &exceptionLocal;

void
memoryMapCheckKern(VPNum vp)
{
    // does a bunch of sanity checks on address space
    if (vp != 0) return;

    memoryMapCheck();

    /*
     * The following wassert's are here simply to keep garbage-collecting
     * linkers from eliminating the *LocalDbg variables.
     */
    tassertWrn(exceptionLocalDbg  == &exceptionLocal,     "Huh?");
}
