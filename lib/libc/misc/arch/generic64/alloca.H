#ifndef __GENERIC64_ALLOCA_H_
#define __GENERIC64_ALLOCA_H_
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: alloca.H,v 1.2 2001/10/05 21:47:58 peterson Exp $
 *************************************************************************** */

/* the compiler understands this command */

#include <alloca.h>

#define ALLOCA(dst,size) (dst = (char*)alloca(size))


#endif /* #ifndef __GENERIC64_ALLOCA_H_ */
