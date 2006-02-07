/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: libksup.C,v 1.9 2001/12/07 23:25:14 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: x86 support routines needed by machine-independent
 * libksup.C.
 * **************************************************************************/

#include <init/arch/amd64/bios.H>
#include "init/arch/amd64/bios_client.H"

sval
localPeekChar(void)
{
   /* should not happen (on simics err_printf(0 is early_printk()
    */
   breakpoint();
// return bios_peekchar();
   return(0);
}

void
localPutChar(char c)
{
  extern void comPutChar(char c);
  
  /* should not happen (on simics err_printf(0 is early_printk() */
  comPutChar(c);
  // bios_putchar(c);
}

/* should never be used
 */
void
localInitPutChar(char c)
{
    breakpoint();
}

void
baseAbort(void)
{
    for (;;) {
	breakpoint();
    }
}

void
breakpoint(void)
{
    err_printf("breakpoint ...\n");
    asm("int $3");
}


