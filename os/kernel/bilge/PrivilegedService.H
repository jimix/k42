#ifndef __PRIVILEGED_SERVICE_H_
#define __PRIVILEGED_SERVICE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PrivilegedService.H,v 1.5 2003/12/03 15:23:10 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Kernel services exported to privileged servers.
 * **************************************************************************/

#include <meta/MetaPrivilegedService.H>
#include <cobj/CObjRootMultiRep.H>

// FIXME: fix stub-compiler grammar so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class PrivilegedService;

typedef PrivilegedService **PrivilegedServiceRef;
extern PrivilegedServiceRef TheResourceManagerKern;

class PrivilegedService : public Obj {
protected:
#ifndef _IN_STUBGEN
    class PrivilegedServiceRoot : public CObjRootMultiRep {
    public:
	virtual CObjRep * createRep(VPNum vp);
	PrivilegedServiceRoot();
	PrivilegedServiceRoot(RepRef ref);

	DEFINE_GLOBALPADDED_NEW(PrivilegedServiceRoot);
    };
    friend class PrivilegedService::PrivilegedServiceRoot;
    PrivilegedServiceRoot *root() { return (PrivilegedServiceRoot *)myRoot; }
#else /* #ifndef _IN_STUBGEN */
    struct PrivilegedServiceRoot;  // needed for declarations later in this file
#endif /* #ifndef _IN_STUBGEN */

    static PrivilegedServiceRoot *ThePrivilegedServiceRoot;

    struct CreateDispatcherMsg;
    static SysStatus CreateServerDispatcherLocal(DispatcherID dspid,
						 EntryPointDesc entry,
						 uval dispatcherAddr,
						 uval initMsgLength,
						 char *initMsg,
						 ProcessID caller);

public:
    DEFINE_GLOBALPADDED_NEW(PrivilegedService);

    DEFINE_ACCESS_SERVER(PrivilegedService,
			 MetaObj::controlAccess,
			 MetaObj::none);

    static void ClassInit(VPNum vp);

__xpublic:
    /*
     * The kernel accepts the first call to _Create() and no others.  The
     * assumption is that an initial server, launched by the kernel and running
     * before anything else can even exist, will acquire access to the
     * privileged services and can then forever mediate access to those
     * services.
     */
    static SysStatus _Create(__out ObjectHandle &oh, __CALLER_PID caller);

    virtual SysStatus _setProcessOSData(__in XHandle procXH, __in uval data,
					__CALLER_PID caller);

    virtual SysStatus _setTimeOfDay(uval sec, uval usec);

    virtual SysStatus _launchProgram(__inbuf(*) char *name,
				     __inbuf(*) char *arg1,
				     __inbuf(*) char *arg2,
				     __in uval wait);

    virtual SysStatus _createServerDispatcher(__in DispatcherID dspid,
					      __in EntryPointDesc entry,
					      __in uval dispatcherAddr,
					      __in uval initMsgLength,
					      __inbuf(initMsgLength)
							  char *initMsg,
					      __CALLER_PID caller);

    virtual SysStatus _accessKernelSchedulerStats(__out ObjectHandle& statsFROH,
						  __out uval& statsRegionSize,
						  __out uval& statsSize,
						  __CALLER_PID caller);

    virtual SysStatus _createCPUContainer(__out ObjectHandle& cpuContainerOH,
					  __in uval priorityClass,
					  __in uval weight,
					  __in uval quantumMicrosecs,
					  __in uval pulseMicrosecs,
					  __CALLER_PID caller);

    virtual SysStatus _pidFromProcOH(__in ObjectHandle procOH,
				     __in ProcessID parentPID,
				     __out ProcessID &pid,
				     __CALLER_PID caller);
};

#endif /* #ifndef __PRIVILEGED_SERVICE_H_ */
