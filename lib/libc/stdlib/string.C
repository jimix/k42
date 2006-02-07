/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: string.C,v 1.33 2004/02/27 17:14:34 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    ANSI String facilities
 * **************************************************************************/

#include <sys/sysIncs.H>

/* The behaviour of these routines is not defined for passing in
   NULL pointers.  Most likely we will seg fault.  We could check
   for NULL pointers, but (a) it is unlikely, (b) it takes
   cycles, (c) since we have no error reporting mechanism, at
   best we can return 0, which may just delay discovering a
   problem, (d) the reference implementation doesn't check and
   (e) the documentation does mention these cases. */

extern "C" {
#ifdef __GNU_AS__
/*
 * These are available for glibc so they must be weak.
 */
int strcmp(const char *s1, const char *s2) __attribute__ ((weak));
int strncmp(const char *s1, const char *s2, size_t len) __attribute__ ((weak));
size_t strlen(const char *s1) __attribute__ ((weak));
size_t strnlen (const char *s1, size_t len) __attribute__ ((weak));
char *strcpy (char *s1, const char *s2) __attribute__ ((weak));
char *strncpy (char *s1, const char *s2, size_t len) __attribute__ ((weak));
char *strcat (char *s1, const char *s2) __attribute__ ((weak));
char *strchr (const char *s1, int c) __attribute__ ((weak));
char *strrchr (const char *s1, int c) __attribute__ ((weak));
void *memchr (const void *s1, int c, size_t len) __attribute__ ((weak));
void *memset (void *s1, int cin, size_t len) __attribute__ ((weak));
void *memcpy (void *s2, const void *s1, size_t len) __attribute__ ((weak));
void *memmove (void *s2, const void *s1, size_t len) __attribute__ ((weak));
size_t strlcpy(char *dest, const char *src, size_t size) __attribute__((weak));
#else
  /* this one os only used by the linux kernel so we extern it here */
size_t strnlen (const char *s1, size_t len);
#endif
}


/* ************************************************************ */
/* Compare two strings */
int
strcmp (const char *s1, const char *s2)
{
  /* cases to consider:
     s1 is NULL, s2 is NULL
     strings completely equal (0);
     strings completely different (compare);
     strings equal for a while then different (compare);
  */

  while (*s1 == *s2) {
    /* characters are equal; if we are at the end, return EQUAL */
    if (*s1 == '\0') {
      return (0);
    }
    /* advance pointers to next character */
    s1++, s2++;
  }
  return (*s1 - *s2);
}

int
strncmp (const char *s1, const char *s2, size_t len)
{
  /* cases to consider:
     s1 is NULL, s2 is NULL, len is <= 0
     strings different in the first len characters (compare).
     strings completely equal (0);
     strings equal for len but maybe not after that (0);
  */

  if (len <= 0) {
    /* if len=0, nothing to compare, exit */
    return (0);
  }

  while (len-- > 0) {
    if (*s1 != *s2) {
      /* characters are different; report how */
      return (*s1 - *s2);
    }
    /* characters are equal; if we are at the end, return EQUAL */
    if (*s1 == '\0') {
      return (0);
    }
    /* advance pointers to next character */
    s1++, s2++;
  }

  /* length goes to zero before we find a difference */
  return (0);
}


/* ************************************************************ */
/* determine string length  */
size_t
strlen(const char *s1)
{
  size_t count = 0;

  /* cases to consider:
     s1 is NULL (0);
     strings is empty (0);
     strings is not empty (count);
  */

  while (*s1++ != '\0') {
    count++;
  }
  return (count);
}

size_t
strnlen (const char *s1, size_t len)
{
  size_t count = 0;

  /* cases to consider:
     s1 is NULL (0);
     strings is empty (0);
     strings is not empty, less than len in length (count);
     strings is not empty, equal to len in length (count, len);
     strings is not empty, greater than len in length (len);
  */

  if (len <= 0) {
    /* if len=0, nothing to count, exit */
    return (0);
  }

  while ((len-- > 0) && (*s1++ != '\0')) {
    count++;
  }
  return (count);
}

/* ************************************************************ */
/* Copy one string (s2) to another (s1) */
char *
strcpy (char *s1, const char *s2)
{
  char *s1p;

  /* cases to consider:
     s1 is NULL, s2 is NULL;
     s2 is empty (0);
     s2 is not empty;
  */

  s1p = s1;

  do {
    *s1p++ = *s2;
  } while (*s2++ != '\0');
  return (s1);
}

char *
strncpy (char *s1, const char *s2, size_t len)
{
  char *s1p;

  /* cases to consider:
     s1 is NULL, s2 is NULL;
     s2 is empty (0);
     s2 is not empty, less than len;
     s2 is not empty, equal to len;
     s2 is not empty, greater than len;
  */

  if (len <= 0) {
    /* if len=0, nothing to copy, exit */
    return (s1);
  }

  s1p = s1;
  do {
    *s1p++ = *s2;
    --len;
  } while ((*s2++ != '\0') && (len > 0));

  /* clear remainder of buffer (if any);  ANSI semantics */
  while (len > 0) {
    *s1p++ = '\0';
    --len;
  }
  return (s1);
}

char *
strcat (char *s1, const char *s2)
{
  /* cases to consider:
     s1 is NULL, s2 is NULL;
     s1 is empty;
     s2 is empty;
     s2 is not empty
  */

  char *s1p = s1;

  /* find end of first string */
  while (*s1p != '\0') {
    s1p++;
  }

  /* copy second string to the end of first */
  /* same basic code as in strcpy */

  do {
    *s1p++ = *s2;
  } while (*s2++ != '\0');
  return (s1);
}

/* ************************************************************ */
/* search for a character (c) in a string (s1), return pointer to c in s1 */
/* unusual case: what if c is '\0'? */
char *
strchr (const char *s1, int c)
{
  /* cases to consider:
     s1 is NULL;
     s1 is empty;
     c is in s1;
     c is not in s1;
     multiple c in s1;
     c is '\0';
  */

  for (;;) {
    if (*s1 == c) {
      return ((char *) s1);
    }
    if (*s1 == '\0') {
      return (NULL);
    }
    s1++;
  }
}

char *
strrchr (const char *s1, int c)
{
  char *t = NULL /* pointer to last c found */;

  /* cases to consider:
     s1 is NULL;
     s1 is empty;
     c is in s1;
     c is not in s1;
     multiple c in s1;
     c is '\0';
  */

  for (;;) {
    if (*s1 == c) {
      t = (char *) s1;
    }
    if (*s1 == '\0') {
      return (t);
    }
    s1++;
  }
}


void *
memchr (const void *s1, int c, size_t len)
{
  unsigned char *s1p = (unsigned char *) s1;

  if (len <= 0) {
    /* if len=0, nothing to find, exit */
    return (NULL);
  }

  while (len-- > 0) {
    if (*s1p == c) {
      return ((void *) s1p);
    }
    s1p++;
  }
  return (NULL);
}

/* ************************************************************ */
/* This code tries to operate by words, rather than bytes.  The
   assumption is that this (a) will be faster, and (b) must be
   done on word alignments.  So, we break everything down into 3
   phases: (1) Anything that is before we are word-aligned, (2)
   the word-aligned portion, and (3) the left-over parts after
   the last word-aligned word.

   For example, if we say to set 16 bytes to 0 starting at
   location 3, we (1) set 3 to 0 (as a character), then (2) set
   4, 8, 12 to 0 as words, and (3) set 16, 17, 18 to 0 as
   characters.  */

#define UNIT             uval
#define UNITSIZE         sizeof(UNIT)
#define UNITALIGNMENT(p) (((uval)(p)) & (UNITSIZE-1))
/* note that the above requires implicit knowledge that the
   number of bytes in a UNIT is a power of two, so that
   UNITSIZE-1 is a bit mask */

/* memset (destination value length) */
void *
memset (void *s1, int cin, size_t len)
{
  unsigned char *s1p = (unsigned char *) s1;
  unsigned char c = cin;

  if (len <= 0) {
    /* if len=0, nothing to set, exit */
    return (s1);
  }

  if (len >= UNITSIZE) {
    UNIT w = 0;

    /* set by character until we get aligned */
    while ((UNITALIGNMENT(s1p) != 0) && (len > 0)) {
      *(s1p++) = c;
      len--;
    }

    {
      /* construct an entire word of replicas of the char value */
      /* so for 'a', we want 'aaaa' */
      unsigned int i;
      for (i = 0; i < UNITSIZE; i++) {
	w = (w << 8) | c;
      }
    }

    /* set value word at a time */
    while (len >= UNITSIZE) {
      *(UNIT *) s1p = w;
      s1p += UNITSIZE;
      len -= UNITSIZE;
    }
  }

  /* set rest */
  while (len-- > 0) {
    *(s1p++) = c;
  }
  return (s1);
}

/* memcpy (destination source length) */
void *
memcpy (void *s2, const void *s1, size_t len)
{
  char *s1p = (char *) s1;
  char *s2p = (char *) s2;

  if (len <= 0) {
    /* if len=0, nothing to copy, exit */
    return (s2);
  }

  if (len >= UNITSIZE) {
    /* if same alignment go by word */
    if (UNITALIGNMENT(s1p) == UNITALIGNMENT(s2p)) {
      /* get aligned */
      while ((UNITALIGNMENT(s1p) != 0) && (len > 0)) {
	*(s2p++) = *(s1p++);
	len--;
      }
      /* move words */
      while (len >= UNITSIZE) {
	*(UNIT *) s2p = *(UNIT *) s1p;
	s1p += UNITSIZE;
	s2p += UNITSIZE;
	len -= UNITSIZE;
      }
    }
  }

  /* move rest (the unaligned trailing part) */
  while (len-- > 0) {
    *(s2p++) = *(s1p++);
  }
  return (s2);
}

void *
memmove (void *s2, const void *s1, size_t len)
{

  if (len <= 0) {
    /* if len=0, nothing to copy, exit */
    return (s2);
  }

  if ((uval) s2 > (uval) s1) {
    /* move from high end to low end, in case of overlap */
    char *s1p = (char *) s1 + len;
    char *s2p = (char *) s2 + len;

    /* if same alignment go by word */
    if (UNITALIGNMENT(s1p) == UNITALIGNMENT(s2p)) {
      /* get aligned */
      while ((UNITALIGNMENT(s1p) != 0) && (len > 0)) {
	*(--s2p) = *(--s1p);
	len--;
      }
      /* move words */
      while (len >= UNITSIZE) {
	s1p -= UNITSIZE;
	s2p -= UNITSIZE;
	*(UNIT *) s2p = *(UNIT *) s1p;
	len -= UNITSIZE;
      }
    }

    /* move rest */
    while (len-- > 0) {
      *(--s2p) = *(--s1p);
    }
    return (s2);
  }
  else {
    /* move from low end to high end, in case of overlap */
    return (memcpy(s2, s1, len));
  }
}


// From Linux kernel
/**
 * strlcpy - Copy a %NUL terminated string into a sized buffer
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @size: size of destination buffer
 *
 * Compatible with *BSD: the result is always a valid
 * NUL-terminated string that fits in the buffer (unless,
 * of course, the buffer size is zero). It does not pad
 * out the result like strncpy() does.
 */
size_t
strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size-1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}
