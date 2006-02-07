#ifndef __ALLOCA_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __ALLOCA_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: alloca.H,v 1.6 2001/10/05 21:48:05 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: (possibly) machine-dependent definition of ALLOCA.
 * **************************************************************************/

/* the compiler understands this command */
#define ALLOCA(dst,size) (dst = (char*)alloca(size))
