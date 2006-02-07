#ifndef __KERNEL_SCHEDULER_H_
#define __KERNEL_SCHEDULER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelScheduler.H,v 1.3 2003/09/19 14:03:17 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     Defines an interface to the kernel scheduler, including
 *     a shared structure by which the kernel publishes DispatchQueue
 *     statistics.  Access is provided only to privileged processes.
 * **************************************************************************/

class KernelScheduler {
public:
    enum {
	PRIORITY_CLASS_KERNEL		= 1,
	PRIORITY_CLASS_GANG_SCHEDULING	= 2,
	PRIORITY_CLASS_SOFT_REAL_TIME	= 3,
	PRIORITY_CLASS_TIME_SHARING	= 4,
	PRIORITY_CLASS_BACKGROUND	= 5,
	PRIORITY_CLASS_IDLE		= 6,
	PRIORITY_CLASS_UNRUNNABLE	= 7,

	NUM_PRIORITY_CLASSES            = 8
    };

    static const uval MIN_WEIGHT = 1;
    static const uval MAX_WEIGHT = 4096;

    static uval const MIN_QUANTUM_MICROSECS = 100;
    static uval const MAX_QUANTUM_MICROSECS = 40000;

    static uval const MIN_PULSE_MICROSECS = 100;
    static uval const MAX_PULSE_MICROSECS = 40000;

    struct Stats {

	static const uval DECAY_NUMERATOR = 7;
	static const uval DECAY_DENOMINATOR_LOG = 3;

	struct Smooth {
	    uval dispatchTimeAccum;
	    uval runnableWeightAccum;

	    void init() {
		dispatchTimeAccum = 0;
		runnableWeightAccum = 0;
	    }

	    void update(uval newDspTimeAccum, uval newRunWgtAccum) {
		dispatchTimeAccum =
		    ((dispatchTimeAccum * DECAY_NUMERATOR) >>
				    DECAY_DENOMINATOR_LOG) + newDspTimeAccum;
		runnableWeightAccum =
		    ((runnableWeightAccum * DECAY_NUMERATOR) >>
				    DECAY_DENOMINATOR_LOG) + newRunWgtAccum;

	    }
	};

	uval runnableCPUDomainCount;
	uval runnableDispatcherCount;

	uval updateInterval;
	uval smoothingInterval;

	Smooth smooth[NUM_PRIORITY_CLASSES];

	void init(uval updtInterval) {
	    runnableCPUDomainCount = 0;
	    runnableDispatcherCount = 0;

	    updateInterval = updtInterval;
	    smoothingInterval =
		(updateInterval * (uval(1) << DECAY_DENOMINATOR_LOG)) /
		    ((uval(1) << DECAY_DENOMINATOR_LOG) - DECAY_NUMERATOR);
	    
	    for (uval pclass = 0; pclass < NUM_PRIORITY_CLASSES; pclass++) {
		smooth[pclass].init();
	    }
	}



	void incRunnableCPUDomainCount() { runnableCPUDomainCount++; }
	void decRunnableCPUDomainCount() { runnableCPUDomainCount--; }

	void incRunnableDispatcherCount() { runnableDispatcherCount++; }
	void decRunnableDispatcherCount() { runnableDispatcherCount--; }
    };
};

#endif /* #ifndef __KERNEL_SCHEDULER_H_ */
