#ifndef __BUGS_H_
#define __BUGS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bugs.H,v 1.1 2002/09/27 16:38:47 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines various flags which drive workarounds
 * for bugs
 * **************************************************************************/
/* 
 * floating point non-aligned loads accross page boundaries not yet
 * supported, so have to initialize some structures carefully since
 * compiler will use floating point registers for quick
 * initialization.
 */
#define NO_DOUBLE_LOAD_ACROSS_PAGE

#endif /* #ifndef __BUGS_H_ */
