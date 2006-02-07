/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWInterrupt.C,v 1.1 2001/06/12 21:53:41 peterson Exp $
 *************************************************************************** */

#include <kernIncs.H>
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>

#include "exception/HWInterrupt.H"
#include "exception/KernelTimer.H"
#include "exception/ExceptionLocal.H"
#include "exception/ProcessAnnex.H"

/*static*/ void
HWInterrupt::SendIPI(VPNum vp)
{
}


