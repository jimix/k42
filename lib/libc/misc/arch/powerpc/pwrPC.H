#ifndef __POWERPC_PWR_PC_H_
#define __POWERPC_PWR_PC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: pwrPC.H,v 1.18 2004/11/05 16:23:58 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * Architecture Descriptions for Power PC 64 bit
 * **************************************************************************/

/*
 * Segment Descriptors for Memory and I/O
 *
 * These definitions map the fields in the PowerPC Architecture for
 * segment table entries (STEs).  STEs are 16 bytes long, 2 doublewords.
 */
class SegDesc {
public:
    // First double word:
    uval64 ESID :                36;	// Effective Segment ID
    uval64 reserved1 :           19;
    uval64 Bolted :               1;	// Non-evictable (software-reserved bit)
    uval64 V :                    1;	// "Valid" bit
    enum {Memory,IO} T :          1;	// Memory for this format (T==0)
    uval64 Ks :                   1;	// Supervisor Key
    uval64 Kp :                   1;	// User Key
    uval64 NoExecute :            1;	// Execution not permitted
					//   (no HW support for this in PowerPC)
    uval64 Class :                1;    // SLBIE Class
    uval64 reserved2 :            2;

    // Second double word:
    uval64 VSID :                52;	// Virtual Segment ID
    uval64 reserved3 :           12;

    static const uval NumberOfSegments = 256;	// 4KB segTable, 16-byte STEs

    /*
     * When setting a segment table entry we have to synchronize with the
     * mapping fault handler in lolita.S.  We use the reserved bit adjacent to
     * the valid bit for this purpose.
     */
    static const uval64 STE_LOCKED_INVALID = 0x0000000000000100;
};

class SegDescIO {
    // First double word:
    uval64 ESID :                36;	// Effective Segment ID
    uval64 reserved1 :           20;
    uval64 V :                    1;	// "Valid" bit
    enum {Memory,IO} T :          1;	// I/O for this format (T==1)
    uval64 Ks :                   1;	// Supervisor Key
    uval64 Kp :                   1;	// User Key
    uval64 reserved2 :            4;

    // Second double word:
    uval64 IOInfo;			// I/O-specific information
					// FIXME - 32-bit had BUID:9 and DATA:20
};

/*
 * formats of two parameters to slbmte instruction for software
 * slb reload
 */
class SLB_VSID {
public:
    // First double word:
    uval64 VSID :                52;	// Effective Segment ID
    uval64 Ks :                   1;	// Supervisor Key
    uval64 Kp :                   1;	// User Key
    uval64 NoExecute :            1;	// Execution not permitted
					//   (no HW support for this in PowerPC)
    uval64 L :                    1;	// Large Page Segment
    uval64 Class :                1;    // SLBIE Class
    uval64 reserved :             4;
    uval64 LS :                   3;	// Large Page Size Selector
    SLB_VSID& operator= (const uval source) {
	*(uval*)this = source;
	return *this;
    }
    operator uval() const {
	return *(uval*)this;
    }
    static const uval NumberOfSLBs = 64;
};

class SLB_ESID {
public:
    uval64 ESID :                36;	// Virtual Segment ID
    uval64 V :                    1;	// "Valid" bit
    uval64 reserved :            15;
    uval64 index :               12;	// entry to load
    SLB_ESID& operator= (const uval source) {
	*(uval*)this = source;
	return *this;
    }
    operator uval() const {
	return *(uval*)this;
    }
};


class DSIStatusReg {
public:
    enum {
	notPresent = 0x40000000,        // Miss in page table, no BAT mapping
	protect    = 0x08000000,        // Protection violation
	writeFault = 0x02000000,        // Operation:  1 => store, 0 => load
	segMiss    = 0x00200000		// Miss in segment table
    };
};
#endif /* #ifndef __POWERPC_PWR_PC_H_ */
