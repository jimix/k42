#ifndef __PRIVILEGED_SERVICE_WRAPPER_H_
#define __PRIVILEGED_SERVICE_WRAPPER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PrivilegedServiceWrapper.H,v 1.5 2003/12/03 15:23:13 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Wrapper object for calling privileged kernel services
 * **************************************************************************/

#include <stub/StubPrivilegedService.H>
#include <meta/MetaPrivilegedService.H>

class PrivilegedServiceWrapper;
typedef PrivilegedServiceWrapper** PrivilegedServiceRef;

class PrivilegedServiceWrapper : public Obj {
private:
    StubPrivilegedService stub;
    static PrivilegedServiceRef _ThePrivilegedServiceRef;

protected:
    DEFINE_GLOBAL_NEW(PrivilegedServiceWrapper);
    PrivilegedServiceWrapper() : stub(StubObj::UNINITIALIZED) { }

public:
    static SysStatus ClassInit();

    static PrivilegedServiceRef ThePrivilegedServiceRef() {
	return _ThePrivilegedServiceRef;
    }

    virtual SysStatus setProcessOSData(XHandle procXH, uval data);

    virtual SysStatus setTimeOfDay(uval sec, uval usec);

    virtual SysStatus launchProgram(char *name, char *arg1, char *arg2,
				    uval wait);

    virtual SysStatus createServerDispatcher(DispatcherID dspid,
					     EntryPointDesc entry,
					     uval dispatcherAddr,
					     uval initMsgLength,
					     char *initMsg);

    virtual SysStatus accessKernelSchedulerStats(ObjectHandle& statsFROH,
						 uval& statsRegionSize,
						 uval& statsSize);

    virtual SysStatus createCPUContainer(ObjectHandle& cpuContainerOH,
					 uval priorityClass, uval weight,
					 uval quantumMicrosecs,
					 uval pulseMicrosecs);

    virtual SysStatus pidFromProcOH(ObjectHandle procOH,
				    ProcessID parentPID,
				    ProcessID &pid);
};

#endif /* #ifndef __PRIVILEGED_SERVICE_WRAPPER_H_ */
