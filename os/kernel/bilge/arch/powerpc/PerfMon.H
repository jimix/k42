#ifndef __PERF_MON_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __PERF_MON_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PerfMon.H,v 1.8 2001/10/05 21:49:16 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent performance monitoring
 * **************************************************************************/

#ifdef INCLASS
private:
    struct MMCR0_old {
	uval64 soft:1;			// Software monitor mode enabled
	uval64 disable:1;		// Disable all counting unconditionally
	uval64 privilege:1;		// Disable in privileged state
	uval64 problem:1;		// Disable in problem state
	uval64 monitor_on:1;		// Disable when MSR[MM] = 1
	uval64 monitor_off:1;		// Disable when MSR[MM] = 0
	uval64 interrupt:1;		// DISABLE interrupts
	uval64 overflow:1;		// Suspend when any PMC negative
	uval64 user_on:1;
	uval64 user_off:1;
	uval64 tags_on:1;
	uval64 tags_off:1;
	uval64 reserved:20;
	uval64 pmc1:8;
	uval64 pmc2:8;
	uval64 pmc3:8;
	uval64 pmc4:8;
    };

    struct MMCR0_new {
	uval32 disable:1;		// Disable unconditionally
	uval32 supervisor:1;		// Disable in supervisor state
	uval32 problem:1;		// Disable in problem state
	uval32 monitor_on:1;		// Disable when MSR[PMM] = 1
	uval32 monitor_off:1;		// Disable when MSR[PMM] = 0
	uval32 interrupt:1;		// ENABLE interrupts
	uval32 event:1;
	uval32 tbselect:2;
	uval32 timebase:1;
	uval32 threshold:6;
	uval32 pmc1ovfl:1;
	uval32 pmcNovfl:1;
	uval32 trigger:1;
	uval32 pmc1:7;
	uval32 pmc2:6;
    };

    union MMCR0 {
	MMCR0_old mmcr0_old;
	MMCR0_new mmcr0_new;
    };

    struct MMCR1 {
	uval32 pmc3:5;
	uval32 pmc4:5;
	uval32 pmc5:5;
	uval32 pmc6:5;
	uval32 pmc7:5;
	uval32 pmc8:4;
	uval32 iabr:1;
	uval32 pmc1hist:1;
	uval32 pmcNhist:1;
    };

    static SysStatus GetMMCR0(MMCR0 *);
    static SysStatus SetMMCR0(MMCR0);
    static SysStatus GetMMCR1(MMCR1 *);
    static SysStatus SetMMCR1(MMCR1);
    static SysStatus GetPMC(uval, uval32 *);
    static SysStatus SetPMC(uval, uval32);
    static SysStatus Select(uval, uval32);

    static Events measure;
#endif /* #ifdef INCLASS */
