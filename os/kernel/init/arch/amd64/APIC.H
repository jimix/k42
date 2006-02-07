#ifndef __APIC_H_
#define __APIC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: APIC.H,v 1.4 2001/10/12 18:42:18 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides an interface to x86 APIC registers
 * **************************************************************************/

// from x86 XXX

#include <mem/HardwareSpecificRegions.H>

/* fields in VER */
#define APIC_VER_VERSION	0x000000ff
#define APIC_VER_MAXLVT		0x00ff0000
#define MAXLVTSHIFT		16

/* fields in SVR */
#define APIC_SVR_VECTOR		0x000000ff
#define APIC_SVR_VEC_PROG	0x000000f0
#define APIC_SVR_VEC_FIX	0x0000000f
#define APIC_SVR_ENABLE		0x00000100
#define APIC_SVR_SWDIS		0x00000000
#define APIC_SVR_SWEN		0x00000100
#define APIC_SVR_FOCUS		0x00000200
#define APIC_SVR_FEN		0x00000000
#define APIC_SVR_FDIS		0x00000200

/* fields in TPR */
#define APIC_TPR_PRIO		0x000000ff
#define APIC_TPR_INT		0x000000f0
#define APIC_TPR_SUB		0x0000000f


/* fields in ICR_LOW */
#define APIC_VECTOR_MASK	0x000000ff

#define APIC_DELMODE_MASK	0x00000700
#define APIC_DELMODE_FIXED	0x00000000
#define APIC_DELMODE_LOWPRIO	0x00000100
#define APIC_DELMODE_SMI	0x00000200
#define APIC_DELMODE_RR	0x00000300
#define APIC_DELMODE_NMI	0x00000400
#define APIC_DELMODE_INIT	0x00000500
#define APIC_DELMODE_STARTUP	0x00000600
#define APIC_DELMODE_RESV	0x00000700

#define APIC_DESTMODE_MASK	0x00000800
#define APIC_DESTMODE_PHY	0x00000000
#define APIC_DESTMODE_LOG	0x00000800

#define APIC_DELSTAT_MASK	0x00001000
#define APIC_DELSTAT_IDLE	0x00000000
#define APIC_DELSTAT_PEND	0x00001000

#define APIC_RESV1_MASK		0x00002000

#define APIC_LEVEL_MASK		0x00004000
#define APIC_LEVEL_DEASSERT	0x00000000
#define APIC_LEVEL_ASSERT	0x00004000

#define APIC_TRIGMOD_MASK	0x00008000
#define APIC_TRIGMOD_EDGE	0x00000000
#define APIC_TRIGMOD_LEVEL	0x00008000

#define APIC_RRSTAT_MASK	0x00030000
#define APIC_RRSTAT_INVALID	0x00000000
#define APIC_RRSTAT_INPROG	0x00010000
#define APIC_RRSTAT_VALID	0x00020000
#define APIC_RRSTAT_RESV	0x00030000

#define APIC_DEST_MASK		0x000c0000
#define APIC_DEST_DESTFLD	0x00000000
#define APIC_DEST_SELF		0x00040000
#define APIC_DEST_ALLISELF	0x00080000
#define APIC_DEST_ALLESELF	0x000c0000

#define APIC_RESV2_MASK		0xfff00000


/* fields in ICR_HIGH */
#define APIC_ID_MASK		0x0f000000
#define APIC_ID_BITOFFSET       24

/* fields in LVT1/2 */
#define APIC_LVT_VECTOR		0x000000ff
#define APIC_LVT_DM		0x00000700
#define APIC_LVT_DM_FIXED	0x00000000
#define APIC_LVT_DM_NMI	0x00000400
#define APIC_LVT_DM_EXTINT	0x00000700
#define APIC_LVT_DS		0x00001000
#define APIC_LVT_IIPP		0x00002000
#define APIC_LVT_IIPP_INTALO	0x00002000
#define APIC_LVT_IIPP_INTAHI	0x00000000
#define APIC_LVT_RIRR		0x00004000
#define APIC_LVT_TM		0x00008000
#define APIC_LVT_M		0x00010000


/* fields in LVT Timer */
#define APIC_LVTT_VECTOR	0x000000ff
#define APIC_LVTT_DS		0x00001000
#define APIC_LVTT_M		0x00010000
#define APIC_LVTT_TM		0x00020000


/* fields in TDCR */
#define APIC_TDCR_2		0x00
#define APIC_TDCR_4		0x01
#define APIC_TDCR_8		0x02
#define APIC_TDCR_16		0x03
#define APIC_TDCR_32		0x08
#define APIC_TDCR_64		0x09
#define APIC_TDCR_128		0x0a
#define APIC_TDCR_1		0x0b


/*
 * fields in IRR
 * ISA INTerrupts are in bits 16-31 of the 1st IRR register.
 * these masks DON'T EQUAL the isa IRQs of the same name.
 */
#define APIC_IRQ0		0x00000001
#define APIC_IRQ1		0x00000002
#define APIC_IRQ2		0x00000004
#define APIC_IRQ3		0x00000008
#define APIC_IRQ4		0x00000010
#define APIC_IRQ5		0x00000020
#define APIC_IRQ6		0x00000040
#define APIC_IRQ7		0x00000080
#define APIC_IRQ8		0x00000100
#define APIC_IRQ9		0x00000200
#define APIC_IRQ10		0x00000400
#define APIC_IRQ11		0x00000800
#define APIC_IRQ12		0x00001000
#define APIC_IRQ13		0x00002000
#define APIC_IRQ14		0x00004000
#define APIC_IRQ15		0x00008000
#define APIC_IRQ16		0x00010000
#define APIC_IRQ17		0x00020000
#define APIC_IRQ18		0x00040000
#define APIC_IRQ19		0x00080000
#define APIC_IRQ20		0x00100000
#define APIC_IRQ21		0x00200000
#define APIC_IRQ22		0x00400000
#define APIC_IRQ23		0x00800000

#define ICU_OFFSET	32		/* 0-31 are processor exceptions */
#define XSPURIOUSINT_OFFSET	(ICU_OFFSET + 223)

typedef unsigned char apicId;

class APIC {
    /*
     * virtual base address of APIC same address range used on all procs
     * must be mapped in uncached
     */

    enum {
	ID			= 0x020,
	VERSION			= 0x030,
	TASK_PRIORITY		= 0x080,
	ARBITRATION_PRIORITY	= 0x090,
	PROCESSOR_PRIORITY	= 0x0A0,
	EOI			= 0x0B0,
	LOGICAL_DESTINATION	= 0x0D0,
	DESTINATION_FORMAT	= 0x0E0,
	SPUR_INT_VECTOR	= 0x0F0,
	ISR1              	= 0x100,
	ISR2              	= 0x110,
	ISR3              	= 0x120,
	ISR4              	= 0x130,
	ISR5              	= 0x140,
	ISR6              	= 0x150,
	ISR7              	= 0x160,
	ISR8              	= 0x170,
	TMR1              	= 0x180,
	TMR2              	= 0x190,
	TMR3              	= 0x1A0,
	TMR4              	= 0x1B0,
	TMR5              	= 0x1C0,
	TMR6              	= 0x1D0,
	TMR7              	= 0x1E0,
	TMR8              	= 0x1F0,
	IRR1              	= 0x200,
	IRR2              	= 0x210,
	IRR3              	= 0x220,
	IRR4              	= 0x230,
	IRR5              	= 0x240,
	IRR6              	= 0x250,
	IRR7              	= 0x260,
	IRR8              	= 0x270,
	ERROR_STATUS      	= 0x280,
	INTR_CMD_00_31		= 0x300,
	INTR_CMD_32_63		= 0x310,
	LVT_TIMER 	        = 0x320,
	PERF_COUNTER_LVT  	= 0x340,
	LVT_LINT0             	= 0x350,
	LVT_LINT1             	= 0x360,
	LVT_ERROR         	= 0x370,
	INITIAL_COUNT_TIMER	= 0x380,
	CURRENT_COUNT_TIMER	= 0x390,
	TIMER_DIVIDE_CONFIG	= 0x3E0
    };

    /*
     * All read/write requests to local apic are 32 bit accesses.
     * Volatile so they don't get optimized away, and uncached.
     * through baseAddr above
     */
    static inline uval32 getReg(uval off) {
        uval32 retvalue;
	retvalue = *((volatile uval32 *)(
	    HardwareSpecificRegions::GetAPICVaddr()+off));
	return(retvalue);
    }

    static inline void setReg(uval off, uval32 val) {
	*((volatile uval32 *)(
	    HardwareSpecificRegions::GetAPICVaddr()+off)) = val;
    }

public:
    // primative operations, should not really be exported
    static uval32 getInterruptCmdHi()   { return getReg(INTR_CMD_32_63); }
    static uval32 getInterruptCmdLo()   { return getReg(INTR_CMD_00_31); }
    static uval32 getID()               { return getReg(ID);}
    static uval32 getVersion()          { return getReg(VERSION);}
    static uval32 getSIV()	        { return getReg(SPUR_INT_VECTOR); }
    static uval32 getLvtLint0()	        { return getReg(LVT_LINT0); }
    static uval32 getLvtLint1()	        { return getReg(LVT_LINT1); }
    static uval32 getLvtTimer()         { return getReg(LVT_TIMER); }
    static uval32 getTaskPriority()     { return getReg(TASK_PRIORITY); }
    static uval32 getCurrentCountTimer(){ return getReg(CURRENT_COUNT_TIMER); }


    static void setSIV(uval32 val)            { setReg(SPUR_INT_VECTOR, val); }
    static void setInterruptCmdHi(uval32 val) { setReg(INTR_CMD_32_63, val); }
    static void setInterruptCmdLo(uval32 val) { setReg(INTR_CMD_00_31, val); }
    static void setTaskPriority(uval32 val)   { setReg(TASK_PRIORITY,val); }
    static void setLvtTimer(uval32 val)       { setReg(LVT_TIMER,val); }
    static void setTimerDivideConfig(uval32 val) {
	setReg(TIMER_DIVIDE_CONFIG,val);
    }
    static void setInitialCountTimer(uval32 val)  {
	setReg(INITIAL_COUNT_TIMER,val);
    }
    static void setEOI(uval32 val)            { setReg(EOI, val); }

    /* more interesting routines that actually should be exported */
    static void init();

    static void startSecondary(const apicId targetId, const unsigned char vector);


    // FIXME:  Many different variants are possible depending on
    //         delivery mode and destination mode (see Pentium Manual).
    //         This functions sends an IPI to a single target APIC
    //         with fixed delivery and physical destination modes.
    static void sendIPI(apicId target, unsigned char vector) {
	uval tmp=getInterruptCmdHi() & ~APIC_ID_MASK;
	tmp |= target << 24;
	setInterruptCmdHi(tmp);
	tmp = (getInterruptCmdLo() & APIC_RESV2_MASK) |
	    APIC_DEST_DESTFLD|APIC_DELMODE_FIXED|APIC_DESTMODE_PHY|vector;
	setInterruptCmdLo(tmp);
	// FIXME:  Must decide whether we really want to wait
	//         for ack.
	while (getInterruptCmdLo() & APIC_DELSTAT_MASK) {
	    // err_printf(".");
	}
    }

    static void ackIPI() {
	(void) getSIV();	// We need to read something to avoid doing
				// successive writes.
	setEOI(0);
    }

    static void print() {
	cprintf("APIC:\n lint0:0x%08lx lint1:0x%08lx TPR:0x%08lx SVR:0x%08lx\n",
		(long)getLvtLint0(), (long)getLvtLint1(),
		(long)getTaskPriority(), (long)getSIV());
    }

    static void setTimer(int value) {
	uval32  lvtt;
	sval32  ticks_per_microsec;

	// FIXME:  hacked value for the bus clock
	//         This value should be a value
	//         determined from the hardware
	const uval32 bus_clock=66000000;

	/*
	 * Calculate divisor and count from value:
	 *
	 *  timeBase == CPU bus clock divisor == [1,2,4,8,16,32,64,128]
	 *  value == time in uS
	 */
	setTimerDivideConfig(APIC_TDCR_1);
	ticks_per_microsec = bus_clock / 1000000;

	/* configure timer as one-shot */
	lvtt = getLvtTimer();
	lvtt &= ~(APIC_LVTT_VECTOR|APIC_LVTT_DS|APIC_LVTT_M|APIC_LVTT_TM);
	lvtt |= APIC_LVTT_M;
	/* no INT, one-shot */
	setLvtTimer(lvtt);

	/* */
	setInitialCountTimer(value * ticks_per_microsec);
    }

    static void wait(uval32 count) {
	setTimer(count);
	while (getCurrentCountTimer());
    }
};


#endif /* #ifndef __APIC_H_ */
