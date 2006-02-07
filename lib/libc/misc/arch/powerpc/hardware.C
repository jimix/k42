/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hardware.C,v 1.7 2003/11/08 17:36:14 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: PwrPC implementation of hardware specific features
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <misc/arch/powerpc/simSupport.H>

uval
getInstrCount()
{
    if (KernelInfo::OnSim()) {
	return((uval)SimOSSupport(SimGetInstrCountK));
    } else {
	return((uval)0);
    }
}
