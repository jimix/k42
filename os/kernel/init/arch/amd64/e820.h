/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: e820.h,v 1.3 2001/11/15 23:00:23 peterson Exp $
 *
 *****************************************************************************/

/* this file has been brought over from the Linux source code: include/asm-x86_64/e820.h */

/*
 * structures and definitions for the int 15, ax=e820 memory map
 * scheme.
 *
 * In a nutshell, arch/i386/boot/setup.S populates a scratch table
 * in the empty_zero_block that contains a list of usable address/size
 * duples.   In arch/i386/kernel/setup.c, this information is
 * transferred into the e820map, and in arch/i386/mm/init.c, that
 * new information is used to mark pages reserved or not.
 *
 */
#ifndef __E820_H_
#define __E820_H_

#define E820MAP	0x2d0		/* our map */
#define E820MAX	32		/* number of entries in E820MAP */
#define E820NR	0x1e8		/* # entries in E820MAP */

#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3 /* usable as RAM once ACPI tables have been read */
#define E820_NVS	4

#define HIGH_MEMORY	(1024*1024)

#ifndef __ASSEMBLER__

struct e820map {
    int nr_map;
    struct e820entry {
      /* Maybe even i386 should be changed to look like this? it is
	 cleaner than relying on long long               --pavel */
	u64 addr __attribute__((packed));	/* start of memory segment */
	u64 size __attribute__((packed));	/* size of memory segment */
	u32 type __attribute__((packed));	/* type of memory segment */
    } map[E820MAX];
};

extern struct e820map e820;
#endif /* #ifndef __ASSEMBLY__ */

#endif /* #ifndef __E820_H_ */
