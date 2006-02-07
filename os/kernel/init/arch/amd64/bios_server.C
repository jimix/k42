/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bios_server.C,v 1.2 2001/10/25 21:00:33 peterson Exp $
 *****************************************************************************/

// probably not needed, staright from x86 XXX

#include <sys/sysIncs.H>
#include "disk.H"
#include "util.H"
#include "bios_server.H"
#include __MINC(x86.H)

uchar bios_buf[BIOS_MAX_SECTORS * BYTES_PER_SECTOR];

#define ENTER_REAL_MODE(num_args) \
	    enter_real_mode(num_args); \
	    asm(".code16")

#define LEAVE_REAL_MODE(num_args) \
	    leave_real_mode(num_args); \
	    asm(".code32")

sval
bios_reboot(void)
{
    ENTER_REAL_MODE(0);

    asm("int	$0x19");
    /* DOES NOT RETURN */

    LEAVE_REAL_MODE(0);

    return 0;
}

sval
bios_display_setmode(sval mode)
{
    ENTER_REAL_MODE(1);

    asm("int	$0x10"
	:
	:   "a" (0x0000 | (mode & 0xff))	// %ah = function, %al = mode
	:   "cc"
    );

    LEAVE_REAL_MODE(1);

    return 0;
}

sval
bios_putchar(sval c)
{
    ENTER_REAL_MODE(1);

    asm("int	$0x10"
	:   /* no outputs */
	:   "a" (0x0e00 | (c & 0xff)),	// %ah = function, %al = character
	    "b" (0x0001)		// %bh = page, %bl = fg color
	:   "%eax", "cc"
    );

    LEAVE_REAL_MODE(1);

    return 0;
}

sval
bios_getchar(void)
{
    sval c;

    ENTER_REAL_MODE(0);

    asm("int	$0x16"
	:   "=a" (c)
	:   "a" (0x0000)		// %ah = function
	:   "cc"
    );

    LEAVE_REAL_MODE(0);

    return (c & 0xff);
}

sval
bios_peekchar(void)
{
    unsigned char no_character;
    sval c;

    ENTER_REAL_MODE(0);

    asm("int $0x16; setz %0"
	:   "=r" (no_character)
	:   "a" (0x0100)		// %ah = function (keystroke status)
	:   "%eax", "cc"
    );

    if (no_character) {
	c = -1;
    } else {
	asm("int $0x16"
	    :   "=a" (c)
	    :   "a" (0x0000)		// %ah = function (keyboard read)
	    :   "cc"
	);
	c &= 0xff;
    }

    LEAVE_REAL_MODE(0);

    return c;
}

sval
bios_serial_putchar(sval c)
{
    ENTER_REAL_MODE(1);

    asm("int	$0x14"
	:   /* no outputs */
	:   "a" (0x0100 | (c & 0xff)),	// %ah = function, %al = character
	    "d" (0)			// %dx = port number
	:   "%eax", "cc"
    );

    LEAVE_REAL_MODE(1);

    return 0;
}

sval
bios_serial_getchar(void)
{
    sval c;

    ENTER_REAL_MODE(0);

    asm("int	$0x14"
	:   "=a" (c)
	:   "a" (0x0200),		// %ah = function
	    "d" (0)			// %dx = port number
	:   "cc"
    );

    LEAVE_REAL_MODE(0);

    return (c & 0xff);
}

sval
bios_memsize(sval mtype)
{
    sval size;

    ENTER_REAL_MODE(1);

    if (mtype == 0) {
	asm("int $0x12"			// get conventional memory size
	    :   "=a" (size)
	    :   /* no inputs */
	    :   "cc"
	);
    } else {
	asm("int $0x15"			// get extended memory size
	    :   "=a" (size)
	    :   "a" (0x8800)		// %ah = function
	    :   "cc"
	);
    }

    LEAVE_REAL_MODE(1);
    return size;
}

sval
bios_diskinfo(sval drive)
{
    sval rcode, cyl_sec, track;
    sval cylinders, tracks, sectors;

    ENTER_REAL_MODE(1);

    asm("int	$0x13; setc %%al"
	:   "=a" (rcode),
	    "=c" (cyl_sec),
	    "=d" (track)
	:   "a" (0x0800),		// %ah = function
	    "d" (drive & 0xff)		// %dl = drive number
	:   "cc", "%ebx"
    );

    LEAVE_REAL_MODE(1);

    if ((rcode && 0xff) != 0) {
	// bios call failed, assume 1.2Mb floppy
	cylinders = 80;
	tracks = 2;
	sectors = 15;
    } else {
	//
	// The 10-bit max cylinder is encoded in cyl_sec as llllllllhhxxxxxx,
	// where the l's are low-order bits and the h's are high-order bits.
	// We have to rearrange them and then add 1 to get cylinder count.
	//
	cylinders = (((cyl_sec & 0x00c0) << 2) | ((cyl_sec & 0xff00) >> 8)) + 1;
	//
	// Max track is in track.  Add 1 to get track count.
	//
	tracks = track + 1;
	//
	// Max sector (which is also the sector count, since BIOS's sector
	// numbering starts at 1) is in the bottom 6 bits of cyl_sec.
	//
	sectors = cyl_sec & 0x003f;
    }

    return (cylinders << 16) | (tracks << 8) | sectors;
}

sval
bios_diskread(sval drive, sval cylinder, sval track, sval sector,
						sval count, void *buffer)
{
    sval rcode;

    ENTER_REAL_MODE(6);

    asm("int	$0x13; setc %%al"
	:   "=a" (rcode)
			// %ah = function, %al = count
	:   "a" (0x0200 | (count & 0xff)),
			// %bx = local buffer (%es is already set)
	    "b" (&bios_buf),
			// %cx = cylinder and sector, munged together strangely
	    "c" (((cylinder & 0xff) << 8) |
		 ((cylinder & 0x0300) >> 2) |
		 ((sector + 1) & 0x3f)),
			// %dh = track, %dl = drive number
	    "d" (((track & 0xff) << 8) |
		 (drive & 0xff))
	:   "cc"
    );

    LEAVE_REAL_MODE(6);

    rcode &= 0xff;
    if (rcode == 0) {
	copy_from_real(&bios_buf, buffer, count * BYTES_PER_SECTOR);
    }
    return rcode;
}

sval
bios_diskwrite(sval drive, sval cylinder, sval track, sval sector,
						sval count, void *buffer)
{
    ENTER_REAL_MODE(6);
    LEAVE_REAL_MODE(6);
    return -1;
}

sval
bios_xlate_on(uval cr3, DTParam *gdtrp)
{
    asm volatile (
	"lgdtl	%0\n"
	"movl	%1,%%cr3\n"
	"movl	%%cr0,%%eax\n"
	"orl	%2,%%eax\n"
	"movl	%%eax,%%cr0\n"
	"movw	%%ss,%%ax; movw	%%ax,%%ss\n"
	"movw	%%ds,%%ax; movw	%%ax,%%ds\n"
	"movw	%%es,%%ax; movw	%%ax,%%es\n"
    : : "m" (*gdtrp), "r" (cr3), "i"(CR0::PG_bit|CR0::WP_bit) : "eax");
    return 1;
}

sval
bios_startSecondary(void)
{

    ENTER_REAL_MODE(0);

    asm ("hlt");

    LEAVE_REAL_MODE(0);
    return 1;
}

//
// The address of bios_service() is passed to the loaded kernel as a
// parameter.  When the kernel calls it, data addressability is such that
// the caller's data is accessible, but the boot program's is not.  We have
// to be very careful not to touch global data.
//
// Bios_service() is invoked via an lcall and has to return with an lret.
// We can't do this cleanly in C, so bios_service() is an assembly-language
// stub that simply calls (via a near call) bios_service_local and then does
// an lret.  In bios_service_local(), the original return EIP and return CS
// appear simply as extra parameters on the stack.
//
extern "C" sval
bios_service_local(uval32 ret_eip, uval32 ret_cs, sval request, va_list ap)
{
    sval ret_value;

    //
    // We can't use a switch statement here, because for such a statement
    // gcc would generate a jump table which is accessed as global data,
    // and we can't address global data in this context.
    //
    if (request == BIOS_REBOOT) {
	ret_value = bios_reboot();
    } else if (request == BIOS_DISPLAY_SETMODE) {
	sval mode = va_arg(ap, sval);
	ret_value = bios_display_setmode(mode);
    } else if (request == BIOS_PUTCHAR) {
	sval c = va_arg(ap, sval);
	ret_value = bios_putchar(c);
    } else if (request == BIOS_GETCHAR) {
	ret_value = bios_getchar();
    } else if (request == BIOS_PEEKCHAR) {
	ret_value = bios_peekchar();
    } else if (request == BIOS_SERIAL_PUTCHAR) {
	sval c = va_arg(ap, sval);
	ret_value = bios_serial_putchar(c);
    } else if (request == BIOS_SERIAL_GETCHAR) {
	ret_value = bios_serial_getchar();
    } else if (request == BIOS_MEMSIZE) {
	sval mtype = va_arg(ap, sval);
	ret_value = bios_memsize(mtype);
    } else if (request == BIOS_DISKINFO) {
	sval drive = va_arg(ap, sval);
	ret_value = bios_diskinfo(drive);
    } else if (request == BIOS_DISKREAD) {
	sval   drive    = va_arg(ap, sval);
	sval   cylinder = va_arg(ap, sval);
	sval   track    = va_arg(ap, sval);
	sval   sector   = va_arg(ap, sval);
	sval   count    = va_arg(ap, sval);
	void * buffer   = va_arg(ap, void *);
	ret_value = bios_diskread(drive, cylinder, track, sector,
							    count, buffer);
    } else if (request == BIOS_DISKWRITE) {
	sval   drive    = va_arg(ap, sval);
	sval   cylinder = va_arg(ap, sval);
	sval   track    = va_arg(ap, sval);
	sval   sector   = va_arg(ap, sval);
	sval   count    = va_arg(ap, sval);
	void * buffer   = va_arg(ap, void *);
	ret_value = bios_diskwrite(drive, cylinder, track, sector,
							    count, buffer);
    }  else if (request == BIOS_XLATE_ON) {
	uval   cr3   = va_arg(ap, uval);
	DTParam * gdtrp = va_arg(ap, DTParam *);
	ret_value = bios_xlate_on(cr3, gdtrp);
    }  else if (request == 100) {
	ret_value = bios_startSecondary();
    }

    return ret_value;
}

asm("	.globl	_bios_service;"
    "	.text;"
    "	.align	2;"
    "_bios_service:;"
    "	call	_bios_service_local;"
    "	lret"
);
