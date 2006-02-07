/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: head64.c,v 1.7 2001/12/30 17:38:32 peterson Exp $
 *****************************************************************************/

/*
 *  linux/arch/x86_64/kernel/head64.c -- prepare to run common code
 *
 *  Copyright (C) 2000 Andrea Arcangeli <andrea@suse.de> SuSE
 *
 *  $Id: head64.c,v 1.7 2001/12/30 17:38:32 peterson Exp $
 */

#include <sys/types.H>
#include <string.h>		/* memset, memcpy */
#include <asm/processor.h>	/* picked up from the cross-compiled tools */


struct cpuinfo_x86 boot_cpu_data;  // defined in glibc/.../include/asm/processor.h
extern void early_printk(const char *fmt, ...);
extern void early_clear(void);



static void clear_bss(void)
{
  extern char __bss_start[];
  extern char __bss_end[];

  early_clear();
#ifdef DEBUG_BOOT
  early_printk("Clearing %ld bss bytes...\n", (unsigned long) __bss_end - (unsigned long) __bss_start);
#endif /* #ifdef DEBUG_BOOT */
  memset(__bss_start, 0,
	 (unsigned long) __bss_end - (unsigned long) __bss_start);
#ifdef DEBUG_BOOT
  early_printk("ok\n");
#endif /* #ifdef DEBUG_BOOT */
}

char x86_boot_params[2048];
char saved_command_line[2048];

#define NEW_CL_POINTER		0x228	/* Relative to real mode data */
#define OLD_CL_MAGIC_ADDR	0x90020
#define OLD_CL_MAGIC            0xA33F
#define OLD_CL_BASE_ADDR        0x90000
#define OLD_CL_OFFSET           0x90022


static void copy_bootdata(char *real_mode_data)
{
#ifndef CONFIG_SIMICS
	int new_data;
	char * command_line;
#endif /* #ifndef CONFIG_SIMICS */

	memcpy(x86_boot_params, real_mode_data, 2048);
#ifndef CONFIG_SIMICS
	/*
	 * If running on Simics we boot using load_kernel after starting
	 * Simics. Control is passed in 16 bit real mode directly to the loaded
	 * kernel. After going to 32 bit protected and setting up a stack
	 * control is passed to the common setup code  which detects memory sets up the VGA
         * and establish 64 bit mode: there is no command line nor any of this.
	 */
	new_data = *(int *) (x86_boot_params + NEW_CL_POINTER);
	if (!new_data) {
		if (OLD_CL_MAGIC != * (uval16 *) OLD_CL_MAGIC_ADDR) {
			early_printk("so old bootloader that it does not support commandline?!\n");
			return;
		}
		new_data = OLD_CL_BASE_ADDR + * (uval16 *) OLD_CL_OFFSET;
		early_printk("old bootloader convention, maybe loadlin?\n");
	}
	command_line = (char *) ((uval64)(new_data));
	memcpy(saved_command_line, command_line, 2048);
	early_printk("Bootdata ok (command line is %s)\n", saved_command_line);
#endif /* #ifndef CONFIG_SIMICS */
}

static void setup_boot_cpu_data(void)
{
	int dummy, eax;

	/* get vendor info */
	cpuid(0, &boot_cpu_data.cpuid_level,
	      (int *)&boot_cpu_data.x86_vendor_id[0],
	      (int *)&boot_cpu_data.x86_vendor_id[8],
	      (int *)&boot_cpu_data.x86_vendor_id[4]);

	/* get cpu type
	 * &dummy, &boot_cpu_data.x86_capability); gets a warning
	 * because this address is bigger than 4GB, but we fortunately still
	 * have the V=R mapping.
	 */
	cpuid(1, &eax, &dummy, &dummy, &boot_cpu_data.x86_capability[0]);
	boot_cpu_data.x86 = (eax >> 8) & 0xf;
	boot_cpu_data.x86_model = (eax >> 4) & 0xf;
	boot_cpu_data.x86_mask = eax & 0xf;
}

extern void start_kernel(void);

void pda_init(int level)
{
  /* XXX */
}

void  x86_64_start_kernel(char * real_mode_data)
{
	extern void kinit();

	clear_bss(); /* must be the first thing in C and must not depend on .bss to be zero */

	early_printk("booting amd64 k42...\n");
	copy_bootdata(real_mode_data);
	setup_boot_cpu_data();
	pda_init(0);

	kinit();
}
