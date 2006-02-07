/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: UsrTst.C,v 1.6 2000/10/30 18:43:34 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides simple test interface to a user program
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include "UsrTst.H"

/* virtual */ SysStatus
UsrTst::gotYa(uval i)
{
    err_printf("---- UsrTst, got a gotYa of %ld\n", i);
    return 0;
}

/* static */ UsrTstRef
UsrTst::Create()
{
    UsrTst *rep = new UsrTst();
    CObjRootSingleRep::Create(rep);
    return rep->getRef();
}
