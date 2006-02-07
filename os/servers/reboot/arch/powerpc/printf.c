/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: printf.c,v 1.5 2004/02/27 17:14:18 mostrows Exp $
 *****************************************************************************/
#include <sys/types.h>
#include <stdarg.h>
/*
 * change these defines
 */
#define TTY_OPEN()
#define TTY_PUTC(a) writeZilog(a)
#define TTY_CLOSE()

#define isdigit(c) (((c)>='0')&((c)<='9'))

static __inline void
ioOutUchar(char* addr,char val)
{
    __asm __volatile(
	" stbx    %0,0,%1;"
	" eieio ;"
        :
        : "r"(val),"r"(addr));
}
static __inline char
ioInUchar(char* addr)
{
    unsigned long val;
    __asm __volatile(
	" lbzx   %0,0,%1;"
	" eieio ;"
        : "=r"(val)
        : "r"(addr));
    return val;
}


#define LCR 3		// Line Control Register
#define MCR 4		// Modem Control Register
#define BD_UB 1		// Baudrate Divisor - Upper Byte
#define BD_LB 0		// Baudrate Divisor - Lower Byte
#define LSR 5		// Line Status Register
#define MSR 6		// Modem Status Register
#define LCR_BD 0x80	// LCR value to enable baudrate change
#define LCR_8N1 0x03	// LCR value for 8-bit word, no parity, 1 stop bit
#define MCR_DTR 0x01	// MCR value for Data-Terminal-Ready
#define MCR_RTS 0x02	// MCR value for Request-To-Send
#define LSR_THRE 0x20	// LSR value for Transmitter-Holding-Register-Empty
#define LSR_DR 0x01	// LSR value for Data-Ready
#define MSR_DSR 0x20	// MSR value for Data-Set-Ready
#define MSR_CTS 0x10	// MSR value for Clear-To-Send

void
writeCOM2(char c)
{
    unsigned int lsr;
    volatile unsigned long _base = 0xF80002F8ULL;
    ioOutUchar((char*)(_base + MCR),   MCR_DTR);
    do {
	lsr = ioInUchar((char*)(_base + LSR));
    } while ((lsr & LSR_THRE) != LSR_THRE);
    ioOutUchar((char*)_base, c);
}

extern __writeZilog(char c, void *port);

void
writeZilog(char c)
{
    __writeZilog(c, (void*)0x80013020);
}

static char *printn(long long, char, char *, char, int);


/*
 * Scaled down version of C Library printf and sprintf.
 * Only %s %u %d (==%u) %o %x %X %D are recognized.
 * Supports 'l' and 'll' argument length specifiers.
 * Also supports 0 or blank filling and specified length,
 * e.g., %08x means print 8 characters, 0 filled.
 */

int
rmdb_doprnt(fmt, adx, b)
register char *fmt, *b;
va_list adx;
{
	int len, c;
	char fill, *s, *b0 = b;
	long long arg = 0;  /* initialize to keep the compiler happy */

	for (;;) {
		while ((c = *fmt++) != '%') {
			if (c == '\0') {
				*b = 0;
				return b-b0;
			}
			*b++ = c;
		}
		c = *fmt++;
		/* Setup the fill character. */
		fill = ' ';
		if ((c==' ')||(c=='0')) {
			fill = (char)c;
			c = *fmt++;
		};
		/* Setup the desired output length */
		len = 0;
		if (c == '*') {		/* dynamic length */
		    len = va_arg(adx, int);
		    c = *fmt++;
		} else
		    for (; isdigit(c); ) {
			len = (len*10)+((int)((char)c-'0'));
			c = *fmt++;
		    }
                /* Check for 'l' (long) or 'll' (long long) arg specifier */
		if (c == 'l') {
		    c = *fmt++;
		    if (c == 'l') {
		        arg = va_arg (adx, long long);  /* pick up a long long arg */
			c = *fmt++;
		    }
		    else
		        arg = va_arg (adx, long);       /* pick up a long arg */
		}
		else {
		    if (c != 's')
		        arg = va_arg (adx, int);        /* pick up an int arg */
		}
		/* Now we're at the format specifier. */
		switch (c) {
		case 'D': case 'd': case 'u': case 'o': case 'x': case 'X':
			b = printn(arg, c, b, fill, len);
			continue;
		case 'c':
			*b++ = arg;
			continue;
		case 's':
			s = (char *)va_arg(adx, char *);
			for (len -= strlen(s); --len >= 0; *b++ = ' ');
			while (*b++ = *s++);
			--b;
			continue;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*
 * NAME: printn
 *
 * FUNCTION: convert a number to a printable string
 *
 * RETURNS:  pointer to the end of the passed buffer
 *
 */

static char *
printn(
    long long n,	/* Number to convert		*/
    char ch,		/* Format specifier		*/
    char *p,		/* buffer ptr.			*/
    char f,		/* Fill character		*/
    int l)		/* # of characters to output	*/
{
	long long n1;
	int i=0, b, nd, c;
	int	flag;
	int	plmax;
	char d[12];

	/* Set the base */
	switch (ch) {
		case 'o':	b = 8;
				plmax = 23;  /* was 11 */
				break;
		case 'x': case 'X': b = 16;
				plmax = 16;   /* was 8 */
				break;
		default:	b=10;
				plmax = 20;  /* was 10 */
	}

	c = 1;
	flag = n < 0;
	if (flag)
		n = (-n);
	if (flag && b==10) {
		flag = 0;
		*p++ = '-';
		l--;
	}
	for (;i<plmax;i++) {
		nd = n % b;
		if (flag) {
			nd = (b - 1) - nd + c;
			if (nd >= b) {
				nd -= b;
				c = 1;
			} else
				c = 0;
		}
		d[i] = nd;
		n = n / b;
		if ((n==0) && (flag==0))
			break;
	}
	if (i==plmax)
		i--;
	/* Put in any fill characters. */
	for (l--; (l>i); l--)
		*p++ = f;
	/* Move converted # to the output. */
	for (;i>=0;i--) {
		*p++ = "0123456789ABCDEF"[d[i]];
	}
	return p;
}

/*----------------------------------------------------------------------------*/
/*
 * NAME: sprintf
 *
 * FUNCTION:
 *   Build a string using the supplied format.
 *   This works like the sprintf library routine using the
 *   limited Kernel printf formatting.
 *
 * RETURN VALUE:  none
 */
int
sprintf(char *buf, const char *fmt, ...)
/* VARARGS */
{
	va_list vap;

	va_start(vap, fmt);
	(void) rmdb_doprnt(fmt, vap, buf);
	va_end(vap);
	return 0;
}


/*----------------------------------------------------------------------------*/
/*
 * NAME: printf
 *
 * FUNCTION: This is the low-level debugger's printf.
 *
 * RETURN VALUE:  none
 */
int
printf(char *fmt, ...)
/* VARARGS */
{
	static struct {
		int csmt;
		int len;
		char buf[200];
	} eb;


	va_list vap;
	register int l;
	register char *p;
	register int i, j;

	for (i=0; i<200; i++)
		eb.buf[i]=0;

	va_start(vap, fmt);

	l = rmdb_doprnt(fmt, vap, eb.buf);
	va_end(vap);

	TTY_OPEN();

	/* print the stuff. */
	p = eb.buf;
	while (l--) {		/* use debugger version of putchar */
		TTY_PUTC(*p++);
	}

	TTY_CLOSE();
}
