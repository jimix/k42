/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: misc.c,v 1.2 2002/09/08 22:58:23 mostrows Exp $
 *****************************************************************************/
char 	*strcat(char *s1, const char *s2);
char 	*strcpy(char *s1, const char *s2);

#if 0
void read1_stdin(char *c_ptr, char nl_flag)
{
	extern int stdin_ihandle;

	while ((of_read(stdin_ihandle, c_ptr, 1)) < 1)
		;
	printf("%c", *c_ptr);
	if (nl_flag)
		printf("\n\r");
}

int read_stdin_str(char *c_ptr)
{
	char *local;

	local = c_ptr;
	for (;;) {
		read1_stdin(local, 0);

		if ((*local == 0x08) && (local > c_ptr)) {
			/* handle backspaces */
			local--;
			printf(" \b");
		}
		else if (*local == 0x0D) {
			/* handle end of input, indicated by newline */
			*local = 0;
			printf("\n\r");
			return (0);
		}
		else
			local++;
	}
}

void read_gt1_stdin(char *c_ptr, int len)
{
	/*
	 * reads multiple characters from stdin without echo.
	 * useful for keyboard initiated events
	 *
	 * still needs some alarm stuff for timeouts
	 */
	extern int stdin_ihandle;
	int i;

	while ((of_read(stdin_ihandle, &c_ptr[i], len)) < len)
		;
}

void write2stdout(char c)
{
	/* print one character */
	extern int stdout_ihandle;

	of_write(stdout_ihandle, &c, 1);
}
#endif

typedef unsigned long size_t;
size_t strlen(const char *s)
{
	register int i = 0;

	while (s[i] != '\0')			/* up to NULL */
		++i;
	return (i);
}

int isprint(int c)
{
	return (( c >= 0x20 ) && ( c <= 0x7E ));
}

int isspace(int c)
{
    return ((c == ' ') || (c == '\f') || (c == '\n')
	   || (c == '\r') || (c == '\t') || (c == '\v'));
}

int isdigit(int c)
{
    return ((c >= '0') && (c <= '9'));
}

int atoi(const char *s)
{
    register int i = 0, n, sign = 1;

    for (i = 0; isspace(s[i]); i++)
	;
    if (s[i] == '-')
	sign = -1, i++;
    if (s[i] == '+')
	i++;
    for (n = 0; isdigit(s[i]); i++)
	n = 10 * n + (s[i] - '0');

    return (sign * n);
}

/*
 *
 *  hexdump(buf,cc) - Format and display hex dump of data block
 *
 *          Inputs:
 *		buf  - Pointer to input buffer to dump
 *		cc   - Character count to dump
 *
 */

hexdump(char * buf,int cc)
{
    int    i,j;
    char   hstr[60];
    char   astr[19];
    char   tbuf[10];

    while (cc)
    {
	strcpy(astr,"[................]");
	strcpy(hstr,"");

	for (i=0,j=1;i<16 && cc>0;i++,j++)
	{
	    cc--;
	    sprintf(tbuf,"%02X",*(buf+i));
	    strcat(hstr,tbuf);
	    if (!(j%4))
		strcat(hstr," ");

	    if (isprint(*(buf+i)))
	    {
		astr[j] = *(buf+i);
	    }
	}

	for (; j<=16; j++) {
	    strcat(hstr,"  ");
	    if (!(j%4))
		strcat(hstr," ");
	}
	buf+=16;
	printf("\t%s %s\n\r",hstr,astr);
    }
}

int strcmp( const char *s1, const char *s2 )
{
	register char *str1 = (char *)s1;
	register char *str2 = (char *)s2;

	while (*str1 && (*str1 == *str2))
		str1++, str2++;
	return ((int)(*str1 - *str2));
}

char *strcpy( char *s1, const char *s2 )
    {
	register char *dest = (char *)s1;
	register char *src  = (char *)s2;
						/* copy up to NULL */
	while (( *dest++ = *src++ ) != '\0' )
	    ;
	return ( s1 );
    }

char *strcat( char *s1, const char *s2 )
    {
	register char *dest = (char *)s1;
	register char *src  = (char *)s2;

	while ( *dest != '\0' )			/* look for dest null char */
	    dest++;
	while (( *dest++ = *src++ ) != '\0')	/* copy up to src null char */
	    ;
	return ( s1 );
    }

#define MAXEXITS        32                      /* max number of atexits */

static void (*_atexit[ MAXEXITS ])(void);       /* atexit functions */
static unsigned _n_atexit = 0;                  /* number of functions */

extern void __exit(int status) __attribute__((noreturn));
void exit ( int status )
    {
        while ( _n_atexit-- )                   /* functions to call? */
            (*_atexit[ _n_atexit ])();          /* call them */

        __exit( status );                       /* return to calling prog */
    }

int atexit( void (*func)(void))
    {
        if ( _n_atexit == MAXEXITS-1 )          /* out of room? */
            return ( -1 );                       /* return error */
        else
            _atexit[ _n_atexit++ ] = func;      /* else store function */
        return ( 0 );                            /* no error */
    }
