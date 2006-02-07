/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: early_printk.c,v 1.8 2001/10/08 18:08:11 pdb Exp $
 *****************************************************************************/

#include "io.h"
#include <sys/types.H>

extern int
wrapper_printfBuf(const char *fmt0, va_list argp, char *buf, sval buflen);

#ifndef MINIKERNEL
#define VGABASE		0xb8000ul	/* this address good enuff until we remove the V=R mapping of the first 2MB */	
#else
#define VGABASE		0xb8000ul
#endif

#define MAX_YPOS	25
#define MAX_XPOS	80

static int current_ypos = 1, current_xpos = 0; /* We want to print before clearing BSS */

#ifndef MINIKERNEL
void
early_clear (void)
{
	int k, i;
	for(k = 0; k < MAX_YPOS; k++)
		for(i = 0; i < MAX_XPOS; i++)
			writew(0, VGABASE + 2*(MAX_XPOS*k + i));
	current_ypos = 0;
}

void
early_puts (const char *str)
{
	char c;
	int  i, k, j;

	while ((c = *str++) != '\0') {
		if (current_ypos >= MAX_YPOS) {
#if 1
			/* scroll 1 line up */
			for(k = 1, j = 0; k < MAX_YPOS; k++, j++) {
				for(i = 0; i < MAX_XPOS; i++) {
					writew(readw(VGABASE + 2*(MAX_XPOS*k + i)),
					       VGABASE + 2*(MAX_XPOS*j + i));
				}
			}
			for(i = 0; i < MAX_XPOS; i++) {
				writew(0x720, VGABASE + 2*(MAX_XPOS*j + i));
			}
			current_ypos = MAX_YPOS-1;
#else
			/* MUCH faster, use it from home w/ a dial up line  */
			early_clear();
			current_ypos = 0;
#endif
		}
		if (c == '\n') {
			current_xpos = 0;
			current_ypos++;
			break;
		} else if (c != '\r')  {
			writew(((0x7 << 8) | (unsigned short) c),
			       VGABASE + 2*(MAX_XPOS*current_ypos + current_xpos++));
			if (current_xpos >= MAX_XPOS) {
				current_xpos = 0;
				current_ypos++;
			}
		}
	}
}
#else
#define putc(c, x) writew(((0x7 << 8) | (unsigned short) (c)), \
			  VGABASE + (x) * 2);

void
early_puts (const char *str)
{
	char c;
	int  i, rest;

	while ((c = *str++) != '\0') {
		if (c == '\n' || c == '\r') {
			/* blank rest of the line */
			rest = MAX_XPOS - (current_xpos % MAX_XPOS);
			for (i=0; i < rest; i++) {
				putc(' ', current_xpos++);
			}
		} else {
			putc(c, current_xpos);
			current_xpos++;
		}
		current_xpos %= MAX_XPOS * MAX_YPOS;
	}
}
#endif /* MINIKERNEL */

#define BUF_MAX		(1024)

static char buf[BUF_MAX];
volatile int early_printk__already_in_use; // should be atomic XXX

void early_printk(const char *fmt, ...)
{
	va_list args;
	int i;

	if(early_printk__already_in_use == 0) {
		early_printk__already_in_use = 1;
		va_start(args, fmt);
		i = wrapper_printfBuf(fmt, args, buf, BUF_MAX);
		va_end(args);

		early_puts(buf);
		early_printk__already_in_use = 0;
	}
}
