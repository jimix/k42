/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: intctrSharedLocked.C,v 1.3 2002/10/10 13:09:34 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for dyn-switch.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "intctr.H"

/*virtual*/ SysStatus
SharedIntCtr::inc()
{
    lock.acquire();
    _value++;
    lock.release();
    return 0;
}

/*virtual*/ SysStatus
SharedIntCtr::dec()
{
    lock.acquire();
    _value--;
    lock.release();
    return 0;
}
