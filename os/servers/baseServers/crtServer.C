/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: crtServer.C,v 1.4 2003/12/09 01:13:51 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: default implementation for a customization hook needed
 *                     by the initial server launched by the kernel
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>

#include "PrivilegedServiceWrapper.H"
#include "ProcessLinuxServer.H"
#include "ResMgr.H"

#include <sys/ResMgrWrapper.H>
#include <sys/ProcessLinuxClient.H>
#include <meta/MetaProcessServer.H>
#include <stub/StubProcessServer.H>

/*static*/ void
ProgExec::InitCustomizationHook(ObjectHandle processLinuxOH)
{
    SysStatus rc;
    ProcessID serverPID;
    ObjectHandle myProcOH;

    rc = PrivilegedServiceWrapper::ClassInit();
    if (_SUCCESS(rc)) {
	/*
	 * We succeeded in acquiring access to kernel privileged services.  We
	 * must be the initial server instance, so start the resource manager
	 * and ProcessLinuxServer.
	 */
	err_printf("ProcessLinuxServer starting\n");
	ProcessLinuxServer::ClassInit();
	err_printf("ProcessLinuxServer started\n");

	err_printf("Resource Manager starting\n");
	ResMgr::ClassInit();
	err_printf("Resource Manager started\n");
    } else {
	/*
	 * We're not the initial server instance, so instantiate the normal
	 * client wrappers for ProcessLinuxServer and the resource manager.
	 */

	/*
	 * The kernel will have passed us a null ProcessLinux object handle,
	 * so we obtain our own by communicating with the ProcessLinuxServer.
	 */
	passertMsg(processLinuxOH.invalid(), "processLinuxOH is valid?\n");
	rc = StubProcessLinuxServer::_GetPID();
	passertMsg(_SUCCESS(rc), "No ProcessLinuxServer?\n");
	serverPID = _SGETPID(rc);
	rc = DREFGOBJ(TheProcessRef)->
			    giveAccessByClient(myProcOH, serverPID,
					       MetaProcessServer::destroy,
					       MetaObj::none);
	passertMsg(_SUCCESS(rc), "Can't giveAccess to my own process?\n");
	rc = StubProcessLinuxServer::_Create(processLinuxOH, myProcOH);
	passertMsg(_SUCCESS(rc), "Can't connect to ProcessLinuxServer?\n");

	ProcessLinuxClient::ClassInit(processLinuxOH);
	ResMgrWrapper::CreateAndRegisterFirstDispatcher();
    }
}
