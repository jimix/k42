/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FR.C,v 1.5 2000/05/11 12:09:57 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: lication interface to file, have one
 * per-open instance of file.
 * **************************************************************************/
#include "kernIncs.H"
#include "mem/FR.H"
#include "meta/MetaFR.H"
#include "stub/StubFR.H"

/* virtual */ SysStatus
FR::getType(TypeID &id)
{
    id = StubFR::typeID();
    return 0;
}
