#ifndef __AMD64_BIOS_H_
#define __AMD64_BIOS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bios.H,v 1.2 2001/10/05 21:47:55 peterson Exp $
 *****************************************************************************/

// fixme X86 XXX

#include __MINC(amd64_hw.H)			// for DTParam

#define BIOS_REBOOT		 0
#define BIOS_DISPLAY_SETMODE	 1
#define BIOS_PUTCHAR		 2
#define BIOS_GETCHAR		 3
#define BIOS_PEEKCHAR		 4
#define BIOS_SERIAL_PUTCHAR	 5
#define BIOS_SERIAL_GETCHAR	 6
#define BIOS_MEMSIZE		 7
#define BIOS_DISKINFO		 8
#define BIOS_DISKREAD		 9
#define BIOS_DISKWRITE		10
#define BIOS_XLATE_ON		11

#define BIOS_MAX_SECTORS	18

sval bios_reboot();
sval bios_display_setmode(sval mode);
sval bios_putchar(sval c);
sval bios_init_putchar(sval c);		// only used during intialization
sval bios_getchar(void);
sval bios_peekchar(void);
sval bios_serial_putchar(sval c);
sval bios_serial_getchar(void);
sval bios_memsize(sval mtype);
sval bios_diskinfo(sval drive);
sval bios_diskread(sval drive,
			sval cylinder, sval track, sval sector,
			sval count, void *buffer);
sval bios_diskwrite(sval drive,
			sval cylinder, sval track, sval sector,
			sval count, void *buffer);
sval bios_xlate_on(uval cr3, DTParam *gdtrp);

#endif /* #ifndef __AMD64_BIOS_H_ */
