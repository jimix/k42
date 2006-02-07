#ifndef __AMD64_HW_H_
#define __AMD64_HW_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: amd64_hw.H,v 1.12 2002/01/09 21:32:42 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: mmu and other parapharnelia of the amd64
 * **************************************************************************/

/*
 * In what follows legacy mode means the traditional ia32 mode,
 * while in long mode there are 2 submodes, compatibility
 * (sub)mode (application level only) or 64 bit (sub)mode for
 * both applications and the kernel (the only mode for the
 * kernel... unless legacy).
 *
 * In long mode which is turned on/off by a separate mechanism the
 * segment descriptors can be divided in 3 classes.
 *
 * - vanilla segment descriptors, to be loaded in the code, data
 * or stack segments registers. Data and stack segment registers
 * will be loaded normally but their content ignored, only the
 * code segment descriptor register is used, and only its DPL,
 * and the L bit and D fields are significant.
 *
 * The L bit (previously a reserved bit) set to 1 indicates 64
 * bit mode, and the D bit must then be zero (by default the data
 * size is 32 bit).
 *
 * If the L bit is zero this indicates compatibility mode and the
 * D bit specify the data size like in legacy mode.  Therefore
 * those segment descriptors remain essentially unchanged, in
 * particular remain 32 bit long.
 *
 * - system segment registers, i.e. call gate, trap gate (not
 * used in our design) ldt (not used in our design), interrupt
 * gate and tss segment descriptors registers are all expanded to
 * 64 bits.  Task gates disappear in long mode.  (Pseudo) system
 * descriptors loaded into the GDTR, IDTR and TR's are expanded
 * in 64 bit mode (that is the only time they may be used, at
 * least loaded). The tss in long mode is common for 32
 * compatibility and 64 bit.
 *
 * - FS and GS segment descriptors are treated specially. Their
 * content is not ignored in long mode and being expanded to 64
 * bits they could be used to access quickly thread, task control
 * block or per processr private data but there is no advantage
 * to do that (only NT similarity ... what an advantage is that).
 *
 * When switching from legacy mode to long mode the GDTR, IDTR
 * and TR must be reloaded with the 64 bit versions, avoiding
 * exceptions or interrupts until at least reloading the IDTR.
 *
 * THIS DOES NOT ADDRESS COMPATIBILITY MODE XXX */

// VVV

/* structure descriptions of amd64 architected blocks */
/*
 * N.B. these must NOT have virtual functions Note that the
 * segment descriptors remain 8 bytes long for code, data and
 * stack even in 64 bit mode. Their content is ignored in long
 * mode except the L/D bits.  The other descriptors and pseudo
 * descriptors are extended to 16 bytes to accommodate the 64 bit
 * addressing in long mode (both 64 bit mode and compatibility
 * mode).
 */

class SegDesc {
public:
    uval limit_l:16;

    uval base_l:16;				// all segment bases are 32 bit

    uval base_m:8;
    enum {Accessed=1} A:1;
    enum {WriteDate=1,ReadCode=1} WR:1;		// W for data, R for Code
    enum {ExpandDown=1,Conforming=1} EC:1; 	// E for data, C for Code
    enum {Data,Code} use:1;
    enum {system} S:1;
    uval dpl:2;
    enum {Absent,Present} P:1;

    uval limit_h:4;
    uval AVL:1;

  // If EFER.LMA and CR0.PG then longMode=1 means 64 bit mode,
  // longmode=0 compatibility. In legacy mode, longmode must be zero
    uval longMode:1;

  // In 64 bit mode, the D bit bit must be zero, default operand
  // size is 32 bit.  In compatibilty mode the D bit indicates
  // the default 16/32 bit operand size. In legacy specify data size
    uval D:1;

    uval G:1;
    uval base_h:8;

    inline uval base(){return ((base_h<<24)+(base_m<<16)+base_l);}
    inline void setbase(uval in)
    {
	base_l = in;
	base_m = in>>16;
	base_h = in>>24;
    }
    inline uval limit(){return ((limit_h<<16)+limit_l);}
    inline void setlimit(uval in)
    {
	limit_l = in;
	limit_h = in>>16;
    }
}__attribute__((packed));

/* the only system segment descriptor types which we are going to
 * use here are tss (SysDesc) and call gates interrupt gates
 * (SysGate) all expanded to 16 bytes.  */
class SysDesc {
public:
    enum SysType
    {
	r1,
	tss16_a,		// available 16 bit task
	LDT,
	tss16_b,		// busy 16 bit task

	callg16,
	taskg,			// disallowed in long mode
	intrg16,
	trapg16,

	r2,
	tss64_a,		// also 64 bit TSS, a for not busy (initialize)
	r3,
	tss64_b,		// also 64 bit TSS

	callg,
	r4,
	intrg,
	trapg};

    uval limit_l:16;
    uval base_l:16;
    uval base_m:8;
    SysType type:4;
    enum {system} S:1;
    uval dpl:2;
    enum {Absent,Present} P:1;
    uval limit_h:4;
    uval reserved0:3;
    uval G:1;
    uval base_h:40;		// only for the TSS (and LDT unused)
    uval reserved1:32;		// only for the TSS (and LDT unused)

    inline uval base(){return ((base_h<<24)+(base_m<<16)+base_l);}
    inline void setbase(uval in)
    {
	tassertMsg(this->type == tss64_b || type == tss64_a,
		   "only TSS can use setbase\n");
	base_l = in;
	base_m = in>>16;
	base_h = in>>24;
	reserved1 = reserved0 = 0;
    }
    inline uval limit(){return ((limit_h<<16)+limit_l);}
    inline void setlimit(uval in)
    {
	tassertMsg(this->type == tss64_b || type == tss64_a,
		   "only TSS can use setlimit\n");
	limit_l = in;
	limit_h = in>>16;
    }
}__attribute__((packed));

class SysGate {
public:
    enum SysType
    {
	r1,
	tss16_a,		// available 16 bit task
	LDT,
	tss16_b,		// busy 16 bit task

	callg16,
	taskg,			// disallowed in long mode
	intrg16,
	trapg16,

	r2,
	tss32_a,		// also 64 bit TSS
	r3,
	tss32_b,		// also 64 bit TSS

	callg,
	er4,
	intrg,
	trapg};

    uval offset_l:16;
    uval segment:16;		// code segment selector

    uval ist:3;
    uval reserved0:5;
    SysType type:4;
    enum {system} S:1;
    uval dpl:2;
    enum {Absent,Present} P:1;
    uval offset_h:48;

    uval reserved1:32;


    inline codeAddress offset()
    {
	tassertMsg(this->type == callg || this->type == intrg,
		   "only callg or intrg can use offset\n");
	return (codeAddress) ((offset_h<<16) + offset_l);
    }

    inline void setoffset(codeAddress in)
    {
	tassertMsg(this->type == callg || this->type == intrg,
		   "only callg or intrg can use offset\n");
	offset_l = ((uval) in) & 0xffff;
	offset_h = ((uval) in) >> 16;
	reserved1 = reserved0 = 0;
    }
}__attribute__((packed));




class CR0_amd64 {
public:
    uval PE:1;				// Protected Mode
    uval MP:1;				// Monitor Coprocessor
    uval EM:1;				// No FPU if set
    uval TS:1;				// Task Switched (for FP Avail)
    uval ET:1;				// 1 (obsolete)
    uval NE:1;				// FPU error control (normally 1)
    uval R1:10;				// reserved
    uval WP:1;				// Sup. write protect if 1
    uval R2:1;				// reserved
    uval AM:1;				// Aligment chech when 1
    uval R3:10;				// reserved
    uval NW:1;				// 1 Enables write throughs
    uval CD:1;				// 1 disables cache
    uval PG:1;				// 1 paging enabled
    enum {
	PE_bit=1,
	MP_bit=2,
	EM_bit=4,
	TS_bit=8,
	WP_bit=0x10000,
	AM_bit=0x40000,
	NW_bit=0x20000000,
	CD_bit=0x40000000,
	PG_bit=0x80000000
    };
}__attribute__((packed));

class CR3_amd64 {
public:
    uval r1:3;				// reserved
    uval PWT:1;				// 1 for write through of page dir
    uval PCD:1;				// 1 for no caching of page dir
    uval r2:7;				// reserved
    uval Frame:28;			// pml4 directory frame
    uval ReservedZero:12;		// reserved must be zero
    uval SFTW:12;			// available to software
}__attribute__((packed));

class CR4_amd64 {
public:
    uval VME:1;				// Virtual Mode Extensions when 1
    uval PVI:1;				// Virtual Int. Flag when 1
    uval TSD:1;				// RDTSC priv. when set
    uval DE:1;				// Debug Extensions when set
    uval PSE:1;				// Allow 4meg pages (PentPro)
    uval PAE:1;				// 36 Bit Physical when set
    uval MCE:1;				// Enable Machine Check when set
    uval PGE:1;				// Global bit honored
    uval PCE:1;				// Support RDPMC for non-sup.
}__attribute__((packed));

/* parameter to load/store gdt and idt instructions */
struct DTParam {
    uval16 limit   __attribute__((__packed__));
    uval64 base    __attribute__((__packed__));
};

// temporary fixme XXX
#define amd64tss X86Task

struct amd64tss {
    uval32 R0;				// reserved, ignored

    uval RSP0;				// rsp of ring 0
    uval RSP1;
    uval RSP2;

    uval R1;				// reserved, ignored

    uval IST1;				// alternate stack pointer 1
    uval IST2;				// alternate stack pointer 2
    uval IST3;				// alternate stack pointer 3
    uval IST4;				// alternate stack pointer 4
    uval IST5;				// alternate stack pointer 5
    uval IST6;				// alternate stack pointer 6
    uval IST7;				// alternate stack pointer 7

    uval R2;				// reserved, ignored

    uval16 R3;				// reserved, ignored
    uval16 IOMAP;			// I/O map base address

}__attribute__((packed));


#endif /* #ifndef __AMD64_HW_H_ */
