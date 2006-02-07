/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Loader script for creating the low-core part of the kernel for booting
 *
 * $Id: part1.lds,v 1.1 2001/09/17 22:06:26 peterson Exp $
 *****************************************************************************/


OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386)

SECTIONS
{
  . = 0x0 ;
  .part1 : 
	AT (0x92000)	/* load it at 0x9200, but relocated to zero */
	{		/* must set CS to offset from 0 to 92000 */
	*(.text)	/* the low-order 16/32-bit real/protected code */
        } = 0x9090

  . = ALIGN(16);                /* Read-only date */
  .rodata : { 
	*(.rodata) 
	*(.rodata.*) 
	}

  . = ALIGN(16);                /* Data */
  .data : {                     
        *(.data)
        }

  .bss : {
        *(.bss)
        }

  . = ALIGN(64);

  /* Sections to be discarded */
  /DISCARD/ : {
	*(.comment)
	 *(.debug*) 
	}
}
