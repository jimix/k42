/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessClient.C,v 1.1 2000/07/05 14:00:24 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/ProcessClient.H>
#include <stub/StubProcessClient.H>
#include <meta/MetaProcessClient.H>

/* virtual */ SysStatus
ProcessClient::getType(TypeID &id)
{
    id = StubProcessClient::typeID();
    return 0;
}

void
ProcessClient::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaProcessClient::init();
}
