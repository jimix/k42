/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelExit.C,v 1.3 2003/06/04 14:17:50 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *		KernelExit(), which on amd64 doesn't do anything yet.
 * **************************************************************************/

#include <kernIncs.H>

void
KernelExit(uval /*killThinwire*/, uval /*physProcs*/, uval /*ctrlFlags*/)
{
    err_printf("KernelExit() not yet implemented.\n");
}
