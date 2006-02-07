/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test for pwrite system call (with largefile)
 * **************************************************************************/

#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE

#include "pwrite.c"
