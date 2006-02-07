#ifndef __HARDWARE_H_
#define __HARDWARE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hardware.H,v 1.13 2005/02/09 18:32:45 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * Includes the machine dependent header file(s).
 * This is needed so a machine independent header can include machine
 * dependent code inside a class definition.  The machine dependent file
 * can't include the needed machine specific definitions itself since it
 * just contains part of the class body
 * **************************************************************************/
#include __MINC(hardware.H)

/* in each machine specific file the following must be defined
 * types:
 * -  InterruptState: an opaic type used by disable and enable interrupts
 *
 * Hardware interrupt management routines:
 * uval hardwareInterruptsEnabled()
 * void disableHardwareInterrupts(InterruptState &)
 * void enableHardwareInterrupts(InterruptState &)
 * void enableHardwareInterrupts()
 *
 * Note, the enable/disable interface allows for a variable to be passed with
 * the interrupt state.  This allows for a more efficient implementation that
 * restores rather than sets interrupts flags, but assertions are put in
 * to ensure that this is not used recursively.
 *
 * uval LocalToGlobalAddress(uval vaddr) - for hardware where kernel is
 *   booted in a node specific memory range, translates aliased memory
 *   range to a global address so it can be access from other nodes.
 */


#ifndef __ASSEMBLER__

/* Instantiate this object to disable interrupts for the scope of
 * variables of this type. */
struct HWInterruptDisableScope {
    InterruptState is;
    uval reenable;
    HWInterruptDisableScope() {
	if (hardwareInterruptsEnabled()) {
	    disableHardwareInterrupts(is);
	    reenable = 1;
	} else {
	    reenable = 0;
	}
    }
    ~HWInterruptDisableScope() {
	if (reenable) {
	    enableHardwareInterrupts(is);
	}
    }

};

#endif /* __ASSEMBLER__ */

#endif /* #ifndef __HARDWARE_H_ */
