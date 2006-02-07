#ifndef __TYPES_H_
#define __TYPES_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: types.H,v 1.23 2001/10/05 21:48:38 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Include TYPES from machine specific file,
 * the basic types that should be defined are:
 *   _SIZEUVAL     - a #define to get the natural size of the machine
 *   sval          - signed value of the natural machine type
 *   uval          - unsigned value of the natural machine type
 *                   is large enough to hold a pointer
 *   sval8/uval8   - signed/unsigned 8 bit quantities
 *   sval16/uval16 - signed/unsigned 16 bit quantities
 *   sval32/uval32 - signed/unsigned 32 bit quantities
 *   sval64/uval64 - signed/unsigned 64 bit quantities
 * **************************************************************************/
#include <sys/kinclude.H>
#include __MINC(types.H)

#endif /* #ifndef __TYPES_H_ */
