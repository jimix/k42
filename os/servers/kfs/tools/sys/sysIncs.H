#ifdef __SYS_INCS_H_
#error <sysIncs.H> should be included exactly once from every .C file.
#else
#define __SYS_INCS_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sysIncs.H,v 1.1 2003/10/23 17:40:18 dilma Exp $
 *****************************************************************************/

#ifdef KFS_TOOLS
#include "../../kfsIncs.H"

#else
#error This should not be included out of tools!
#endif // #ifdef KFS_TOOLS

#endif // #ifdef __SYS_INCS_H_
