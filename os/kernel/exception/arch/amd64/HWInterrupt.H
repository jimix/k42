#ifndef __HWINTERRUPT_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __HWINTERRUPT_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWInterrupt.H,v 1.9 2003/06/04 14:17:36 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent interrupt functionality
 * **************************************************************************/

#ifdef INCLASS
public:
		// those assignments to be revisited XXX
    enum {
	MAX_IRQ = 15,
	IO_IRQ0_INTERRUPT = 104,
	IP_INTERRUPT = 120
    };


    static uval PhysCPU(VPNum vp);

#else /* #ifdef INCLASS */

extern "C" void HWInterrupt_TimerInterrupt();
extern "C" void HWInterrupt_IPInterrupt();

#endif /* #ifdef INCLASS */
