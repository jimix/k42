/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRep.C,v 1.8 2001/04/12 19:00:25 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: base clustered object
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/CObjRep.H>
#include <cobj/CObjRoot.H>

void *
CObjRep::operator new(size_t size)
{
    tassert(0, err_printf("A new of an object without "
			  "an implementation of new\n"));
    return (void *)0;
}
