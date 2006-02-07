/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProgExecUsr.C,v 1.2 2003/11/24 12:33:36 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: default implementation for a customization hook needed
 *                     by the initial server launched by the kernel
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>
#include <sys/ResMgrWrapper.H>
#include <sys/ProcessLinuxClient.H>

/*static*/ void
ProgExec::InitCustomizationHook(ObjectHandle processLinuxOH)
{
    ProcessLinuxClient::ClassInit(processLinuxOH);
    ResMgrWrapper::Create();
}
