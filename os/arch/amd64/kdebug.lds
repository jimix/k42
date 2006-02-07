/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Loader script for creating AMD64 kernel image
 *
 * $Id: kdebug.lds,v 1.3 2001/10/22 20:18:55 pdb Exp $
 *****************************************************************************/


OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386)

SECTIONS
{
  . = 0x100000 ;
  ENTRY(start_real)
  _text = . ;                    /* Text and read-only data */
  .lowcore : AT ( 0x100000 ) {	 /* the low-order 16/32-bit real/protected code */
	start_real = . ;
	kernel/init/part2.o
        } = 0x9090

  . += 0x0000700000000000;	/* now advance to high-order virtual memory */

  .text : 
	AT ( ADDR(.lowcore) + SIZEOF(.lowcore) ) {  /* but load just after .lowcore */
	kernVirtStart = . -SIZEOF(.lowcore);
        *(.text)
        *(.fixup)
        *(.gnu.warning)
        } = 0x9090
  _etext = .;                   /* End of text section */

  .rodata : { *(.rodata) *(.rodata.*) }
  .kstrtab : { *(.kstrtab) }

  . = ALIGN(16);                /* Exception table */
  __start___ex_table = .;
  __ex_table : { *(__ex_table) }
  __stop___ex_table = .;

  __start___ksymtab = .;        /* Kernel symbol table */
  __ksymtab : { *(__ksymtab) }
  __stop___ksymtab = .;

  .data : {                     /* Data */
        *(.data)
        CONSTRUCTORS
        }

  _edata = .;                   /* End of data section */

  __bss_start = .;              /* BSS */
  .bss : {
        *(.bss)
        }
  __bss_end = .;

  . = ALIGN(64);
  .data.cacheline_aligned : { *(.data.cacheline_aligned) }


  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }

}
