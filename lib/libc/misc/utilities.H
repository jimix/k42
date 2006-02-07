#ifndef __UTILITIES_H_
#define __UTILITIES_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: utilities.H,v 1.30 2004/01/16 16:37:40 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: assorted utilities used by kernel and base libs
 *                     must NOT name collide with unix or windows for not
 *                     (shared libraries/name spaces will eventually
 *                      untangle the names)
 * **************************************************************************/

void consoleWrite(const char *, uval len);
void vcprintf(const char *fmt, va_list ap);
/*
 * Print to the console
 */
extern void cprintf(const char *fmt, ...)
		__attribute__ ((format (printf, 1, 2)));

extern "C" void breakpoint(void);

#if defined(TARGET_powerpc)
#define BREAKPOINT asm("tw 31,0,0")
#define SIMOS_BREAKPOINT asm(".long 0x7C0007CE")
#elif defined(TARGET_mips64)
// make sure our breakpoints are never in a delay slot to make easier to skip
#define GDB_BREAKPOINT asm(".set noreorder; nop; break 0x5; nop; .set reorder")
#define GDB_BREAKPOINT_WORD 0x0005000d
#define SIMOS_BREAKPOINT \
	asm(".set noreorder; nop; break 100,100; nop; .set reorder")
#define SIMOS_BREAKPOINT_WORD 0x0064190d
#define BREAKPOINT SIMOS_BREAKPOINT
#define BREAKPOINT_WORD SIMOS_BREAKPOINT_WORD
#elif defined(TARGET_amd64)
#define BREAKPOINT __asm__("int $3" : : )
extern void hw_breakpoint(void);
// XXX may have to be changed for Simics
#elif defined(TARGET_generic64)
#define BREAKPOINT hw_breakpoint()
extern void hw_breakpoint(void);
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */

#ifndef SIMOS_BREAKPOINT
#define SIMOS_BREAKPOINT BREAKPOINT
#endif /* #ifndef SIMOS_BREAKPOINT */

#define CONSOLE_BUF_MAX 256

/* stuff that is stdlib like
   these are because and re-named becuase there was a concern that we might
   accidentally pul in a different stdlib with possibly obscure definions for
   some of these functions.  Once we decided to yank stdlib from our base tree
   we also needed to redo names - for the same reason, i.e., to avoid pulling
   in a surprising definition of one of these */
extern void baseAbort(void) __attribute__ ((noreturn));
extern SysStatusUval baseAtoi(const char *str);
extern SysStatus baseStrtol(const char *, char **, int, uval&);

#ifndef EOF
#define EOF	(-1)
#endif /* #ifndef EOF */

void EnterDebugger();
void ExitDebugger();
uval InDebugger();

#endif /* #ifndef __UTILITIES_H_ */
