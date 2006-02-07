/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: atomic.C,v 1.7 2000/05/11 12:09:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Actual implementation of atomic inlined assembly
 * **************************************************************************/

// sysIncs probably includes atomic.h - so set up for compile here
// but include anyway below for robustness

#define __COMPILE_ATOMIC_C
#include <sys/sysIncs.H>

#include "atomic.h"
