/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentHATPrivate.C,v 1.1 2001/06/12 21:54:01 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/SegmentHATPrivate.H"
#include "mem/PageAllocatorKern.H"
#include "proc/Process.H"
#include "alloc/AllocPool.H"
#include <scheduler/Scheduler.H>
#include <cobj/CObjRoot.H>


