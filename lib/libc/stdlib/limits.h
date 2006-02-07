/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: limits.h,v 1.6 2000/05/11 11:48:18 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:  ANSI C functionlity.  Note, adding features as required
 * these should be moved to a machine specific directory when need 64 bit
 * **************************************************************************/

/*
 * Get the linux header, and keep trying until we get it!
 * it get the gcc header for us
 */

#ifndef _GCC_LIMITS_H_
#include_next <limits.h>
#endif
#ifndef LIMITS_DEFH
#define LIMITS_DEFH

/* FIXME
 * Linux will define PATH_MAX as 4096 and this blows out PPC page
 * so we redefine it here.
 * If the above statement is no longer true this entire file can be removed.
 */

#ifdef PATH_MAX
#undef PATH_MAX
#define PATH_MAX        1023		// largest file path name
#endif

#define MAX_PATH_NAME	PATH_MAX

#endif /* LIMITS_DEFH */
