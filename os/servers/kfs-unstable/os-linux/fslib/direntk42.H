#ifndef __DIRENTK42_H_
#define __DIRENTK42_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: direntk42.H,v 1.1 2004/02/11 23:04:00 lbsoares Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

// For now dirent == direntk42, will change to dirent64 in ELF glibc
#ifndef direntk42
#define direntk42 dirent64
#endif //#ifndef direntk42

#endif // #ifndef __DIRENTK42_H_
