/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PrivilegedServiceWrapper.C,v 1.5 2003/12/03 15:23:13 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Wrapper object for calling privileged kernel services
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "PrivilegedServiceWrapper.H"
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubPrivilegedService.H>

/*static*/ PrivilegedServiceRef
		PrivilegedServiceWrapper::_ThePrivilegedServiceRef = NULL;

/*static*/ SysStatus
PrivilegedServiceWrapper::ClassInit()
{
    ObjectHandle oh;
    SysStatus rc;

    passertMsg(_ThePrivilegedServiceRef == NULL,
	       "PrivilegedServiceWrapper::ClassInit called more than once.\n");

    /*
     * The kernel accepts the first call to PrivilegedService::_Create() and
     * no others.  The assumption is that an initial server, launched by the
     * kernel and running before anything else can even exist, will acquire
     * access to the privileged services and can then forever mediate access
     * to those services.
     */
    rc = StubPrivilegedService::_Create(oh);
    _IF_FAILURE_RET(rc);

    PrivilegedServiceWrapper *wrapper = new PrivilegedServiceWrapper;
    passertMsg(wrapper != NULL,
	       "failed to create privileged service wrapper\n");

    wrapper->stub.setOH(oh);

    new CObjRootSingleRep(wrapper);
    _ThePrivilegedServiceRef = (PrivilegedServiceRef) wrapper->getRef();

    return 0;
}

/*virtual*/ SysStatus
PrivilegedServiceWrapper::setProcessOSData(XHandle procXH, uval data)
{
    return stub._setProcessOSData(procXH, data);
}

/*virtual*/ SysStatus
PrivilegedServiceWrapper::setTimeOfDay(uval sec, uval usec)
{
    return stub._setTimeOfDay(sec, usec);
}

/*virtual*/ SysStatus
PrivilegedServiceWrapper::launchProgram(char *name, char *arg1, char *arg2,
					uval wait)
{
    return stub._launchProgram(name, arg1, arg2, wait);
}

/*virtual*/ SysStatus
PrivilegedServiceWrapper::createServerDispatcher(DispatcherID dspid,
						 EntryPointDesc entry,
						 uval dispatcherAddr,
						 uval initMsgLength,
						 char *initMsg)
{
    return stub._createServerDispatcher(dspid, entry, dispatcherAddr,
					initMsgLength, initMsg);
}

/*virtual*/ SysStatus
PrivilegedServiceWrapper::accessKernelSchedulerStats(ObjectHandle& statsFROH,
						     uval& statsRegionSize,
						     uval& statsSize)
{
    return stub._accessKernelSchedulerStats(statsFROH,
					    statsRegionSize,
					    statsSize);
}

/*virtual*/ SysStatus
PrivilegedServiceWrapper::createCPUContainer(ObjectHandle& cpuContainerOH,
					     uval priorityClass, uval weight,
					     uval quantumMicrosecs,
					     uval pulseMicrosecs)
{
    return stub._createCPUContainer(cpuContainerOH,
				    priorityClass, weight,
				    quantumMicrosecs, pulseMicrosecs);
}

/*virtual*/ SysStatus
PrivilegedServiceWrapper::pidFromProcOH(ObjectHandle procOH,
					ProcessID parentPID,
					ProcessID &pid)
{
    return stub._pidFromProcOH(procOH, parentPID, pid);
}
