#ifndef __HWINTERRUPT_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __HWINTERRUPT_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWInterrupt.H,v 1.18 2005/06/06 19:08:24 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent interrupt functionality
 * **************************************************************************/

#ifdef INCLASS
public:
    static const uval MAX_PHYS=32;
    struct hwInterrupt_t {
	uval16 ppToPhys[MAX_PHYS];	// ppNum to hardware number
	uval availPhys;			// bit vector of unstarted
	uval totalPhys;			// number of physProcs that exist
	uval cpuMask;			// mask of CPUs handling interrupts
	uval ipiVecID;			// id of IPI interrupt vector
    };
    static hwInterrupt_t hwInterrupt;
    enum {
	MAX_VECTOR = 31,	// really 2048 sources mapped to 256 vectors
    };

    static SysStatus StartCPU(VPNum newvp, uval iar, uval r3);
    static void KickAllCPUs(uval maxNumCPUs);

    //See comments about inlining in HWInterrupt.C
    static uval PhysCPU(VPNum vp);

    static uval TotalPhys();

    static void Init();

    static void ProcessInterrupt(uval intr);

#else /* #ifdef INCLASS */

extern "C" void HWInterrupt_DecInterrupt();

#endif /* #ifdef INCLASS */
