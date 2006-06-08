#ifndef __BOOT_INFO_H_
#define __BOOT_INFO_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BootInfo.H,v 1.54 2005/03/15 00:56:53 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: info tranfered from boot to kernel powerpc
 * **************************************************************************/

/*
 * when used in boot program, typdef uval64 to unsigned long long
 * and sval32 to int.
 * N.B. this header file is use in a c program - avoid C++ only notations
 */

/*
 * These types may or may not have been picked up from either of two linux
 * header files.  #Define them here in case they haven't been typedef'd.
 */

#if !defined _PPC64_TYPES_H
#define u64   uval64
#define u32   uval32
#define u16   uval16
#define u8    uval8
#define ptr64 uval64
#else

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;
typedef unsigned long	u64;
typedef unsigned long	ptr64;
#endif

#include "prom.h"

/* Platform types */
#define PLATFORM_PSERIES      0x0100
#define PLATFORM_PSERIES_LPAR 0x0101
#define PLATFORM_ISERIES_LPAR 0x0201
#define PLATFORM_LPAR         0x0001
#define PLATFORM_POWERMAC     0x0400
#define PLATFORM_MAPLE        0x0500
#define PLATFORM_BPA          0x1000

/* This is taken from linux include/asm-ppc64/naca.h */
/* Must be kept in sync */
struct LinuxNaca {
	/*==================================================================
	 * Cache line 1: 0x0000 - 0x007F
	 * Kernel only data - undefined for user space
	 *==================================================================
	 */
	fill32;
	void *xItVpdAreas;              /* VPD Data                  0x00 */
	fill32;
	void *xRamDisk;                 /* iSeries ramdisk           0x08 */
	u64   xRamDiskSize;		/* In pages                  0x10 */
	fill32;
	struct paca_struct *paca;	/* Ptr to an array of pacas  0x18 */
	u64 debug_switch;		/* Debug print control       0x20 */
	u64 banner;                     /* Ptr to banner string      0x28 */
	u64 log;                        /* Ptr to log buffer         0x30 */
	u64 serialPortAddr;		/* Phy addr of serial port   0x38 */
	u64 interrupt_controller;	/* Type of int controller    0x40 */
	u64 slb_size;			/* SLB size in entries       0x48 */
	u64 pftSize;			/* Log 2 of page table size  0x50 */
	fill32;
	void *systemcfg;		/* Pointer to systemcfg data 0x58 */
	u32 dCacheL1LogLineSize;	/* L1 d-cache line size Log2 0x60 */
	u32 dCacheL1LinesPerPage;	/* L1 d-cache lines / page   0x64 */
	u32 iCacheL1LogLineSize;	/* L1 i-cache line size Log2 0x68 */
	u32 iCacheL1LinesPerPage;	/* L1 i-cache lines / page   0x6c */
	u64 smt_snooze_delay;           /* Delay (in usec) before    0x70 */
                                        /* entering ST mode               */
	u8  smt_state;                  /* 0 = SMT off               0x78 */
	                                /* 1 = SMT on                     */
	                                /* 2 = SMT dynamic                */
	u8  resv0[7];                   /* Reserved           0x70 - 0x7F */
};


/* The following is based on lmb definitions in linuxppc64, and must be
 * kept in sync with those definitions.  All definitions tagged with "K42"
 * to distinguish from the Linux definitions
 */
#define K42_MAX_LMB_REGIONS 128

struct k42_lmb_property {
	uval64 base;
	uval64 physbase;
	uval64 size;
};

struct k42_lmb_region {
	uval64 cnt;
	uval64 size;
	struct k42_lmb_property region[K42_MAX_LMB_REGIONS+1];
};

struct k42_lmb {
	uval64 debug;
	uval64 rmo_size;
	struct k42_lmb_region memory;
	struct k42_lmb_region reserved;
};

#define RTAS_INSTANTIATE_MAX (1UL<<30) /* Don't instantiate rtas at/above this value */

#ifdef BOOT_PROGRAM
#define k42_rtas_t rtas_t
#endif
struct k42_rtas_t {
	uval64 entry;		/* physical address pointer */
	uval64 base;		/* physical address pointer */
	uval64 size;
	//spinlock_t lock;
	uval64 lock[2];

	fill32;
        struct device_node *dev;	/* virtual address pointer */
};


#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

// Values for onSim
#define SIM_NONE	0  /* running on hardware */
#define SIM_SIMOSPPC	1  /* running on SimOS-PPC */
#define SIM_MAMBO	2  /* running on Mambo */

// For the version struct
#define SYSTEMCFG_MAJOR 1
#define SYSTEMCFG_MINOR 1

#define systemcfg BootInfo
struct BootInfo {
    // struct systemcfg {
    u8  eye_catcher[16];		/* Eyecatcher: SYSTEMCFG:PPC64	0x00 */
    struct {				/* Systemcfg version numbers	     */
	u32 major;			/* Major number			0x10 */
	u32 minor;			/* Minor number			0x14 */
    } version;

    u32 platform;			/* Platform flags		0x18 */
    u32 processor;			/* Processor type		0x1C */
    u64 processorCount;			/* # of physical processors	0x20 */
    u64 physicalMemorySize;		/* Size of real memory(B)	0x28 */
    u64 tb_orig_stamp;			/* Timebase at boot		0x30 */
    u64 tb_ticks_per_sec;		/* Timebase tics / sec		0x38 */
    u64 tb_to_xs;			/* Inverse of TB to 2^20	0x40 */
    u64 stamp_xsec;			/*				0x48 */
    u64 tb_update_count;		/* Timebase atomicity ctr	0x50 */
    u32 tz_minuteswest;			/* Minutes west of Greenwich	0x58 */
    u32 tz_dsttime;			/* Type of dst correction	0x5C */
    u32 dCacheL1Size;			/* L1 d-cache size		0x60 */
    u32 dCacheL1LineSize;		/* L1 d-cache line size		0x64 */
    u32 iCacheL1Size;			/* L1 i-cache size		0x68 */
    u32 iCacheL1LineSize;		/* L1 i-cache line size		0x6C */
    // }

    struct k42_rtas_t rtas;
    struct prom_t prom;

    uval8 onHV;
    uval8 onSim;
    uval8 wireChanOffset;
    uval8 wireInit;
    uval64 argString;
    uval64 argLength;
    uval64 controlFlags;		/* initial control flags setting */
    uval64 availCPUs;			/* bit array of CPUs present */
    uval64 masterCPU;			/* CPU number of master */
    struct {
	uval64 startIAR;		/* if set to 1, means not spinning */
	uval64 startR3;
    } startCPU[32];

    uval64 cpu_version;
    uval64 clock_frequency;		/* CPU clock frequency */
    uval64 bus_frequency;
    uval64 timebase_frequency;

    uval64 physEnd;
    uval64 kernelImage;			/* kernel is mapped here */
    uval64 kernelImageSize;
    uval64 rebootImage;			/* boot image preserved here */
    uval64 rebootImageSize;

    uval32 L2cachesize;
    uval32 L2linesize;

    uval64 hwData;
    uval64 hwDataSize;

    /* Maintain doubleword alignment */
    sval32 get_time_of_day;
    sval32 set_time_of_day;
    sval32 display_character;
    sval32 set_indicator;
    sval32 power_off;
    sval32 system_reboot;
    sval32 read_pci_config;
    sval32 write_pci_config;
    sval32 freeze_time_base;
    sval32 thaw_time_base;
    sval32 event_scan;
    sval32 check_exception;
    sval32 rtas_last_error;
    sval32 ibm_scan_log_dump;

    sval32 rtas_parameter_buf[12];

#define RTAS_INFO_BUF_MAX 2048
    char rtas_info_buf[RTAS_INFO_BUF_MAX];

#define BOOT_DATA_MAX 4096
    char boot_data[BOOT_DATA_MAX];
    char modelName[16];
    uval64 simosDisk0Number;
    uval64 simosDisk1Number;

    char spinLoopCode[16*4];		/* spin code for secondary procs */

    sval32 machine_type;  /* FIXME - temp - 85 for S85, 44 for 44p */
    sval32 machine_type_pad;	/* maintain alignment */

    struct LinuxNaca naca;
    struct k42_lmb lmb;

    uval64 realOffset;		/* phys addr OS is to regard as "0" */
};

extern struct BootInfo *_BootInfo;

#define VER_APACHE 0x32
#define VER_NSTAR  0x33
#define VER_PULSAR 0x34
#define VER_GP     0x35
#define VER_GQ     0x38
#define VER_970	   0x39
#define VER_970FX  0x3C
#define VER_630    0x40
#define VER_630_1  0x41
#define VER_BE_PU  0x70


#endif /* #ifndef __BOOT_INFO_H_ */
