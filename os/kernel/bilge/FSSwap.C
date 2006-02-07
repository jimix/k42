/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSSwap.C,v 1.3 2002/10/10 13:08:31 rosnbrg Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "FSSwap.H"
#include <defines/paging.H>
#include "FSFRSwap.H"

#if SWAPFS == RAMSWAPFS
#include "FSRamSwap.H"

SysStatus
FSSwap::ClassInit(VPNum vp)
{

    /*
     * RAMSWAP frames for main memory - rest is swap
     * Defined this way so we can specify a constant memory
     * size for experiments, independent of the size of the
     * machine
     */
    uval swap = 0;
#ifdef RAMSWAP
    swap = RAMSWAP;
#endif
    err_printf("initializing FSRamSwap with swap %ld\n", swap);
    FSRamSwap::ClassInit(vp, swap);
    return 0;
}

#elif SWAPFS == FRSWAPFS
SysStatus
FSSwap::ClassInit(VPNum vp)
{
    err_printf("initializing FSFRSwap\n");
    FSFRSwap::ClassInit(vp);
    return 0;
}
#elif SWAPFS == DISKSWAP
#include "DiskSwap.H"
SysStatus
FSSwap::ClassInit(VPNum vp)
{
    err_printf("initializing DiskSwap\n");
    DiskSwap::ClassInit(vp);
    return 0;
}

#else
    <<< ---- error no swapping FS defined
#endif
