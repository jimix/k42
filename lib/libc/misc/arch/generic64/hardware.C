/* ***********************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file
 * LICENSE.html in the top-level directory for more details.
 *
 * $Id: hardware.C,v 1.2 2001/07/11 19:13:39 peterson Exp $
 ************************************************************************* */

/* ***********************************************************************
 * Module Description:
 * Generic implementation of hardware specific features
 * *********************************************************************** */

#include <sys/sysIncs.H>

uval
getInstrCount()
{
  return((uval)0);
}


void
hw_breakpoint()
{
}
