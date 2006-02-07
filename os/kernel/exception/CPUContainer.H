#ifndef __CPU_CONTAINER_H_
#define __CPU_CONTAINER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CPUContainer.H,v 1.2 2003/12/03 15:23:11 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines an exported interface for CPUDomainAnnexes.
 * **************************************************************************/

#include <cobj/Obj.H>
#include <meta/MetaCPUContainer.H>

class CPUContainer;
typedef CPUContainer** CPUContainerRef;


class CPUContainer : public Obj {
public:
#ifndef _IN_STUBGEN
    DEFINE_ACCESS_SERVER(CPUContainer, MetaObj::none, MetaObj::none);
#endif

__xpublic:
    virtual SysStatus _setPriorityClass(__in uval pclass) = 0;
    virtual SysStatus _setWeight(__in uval wght) = 0;

    virtual SysStatus _createFirstDispatcher(__in ObjectHandle childOH,
					     __in ProcessID parentPID,
					     __in EntryPointDesc entry,
					     __in uval dispatcherAddr,
					     __in uval initMsgLength,
					     __inbuf(initMsgLength)
					     char *initMsg) = 0;

    virtual SysStatus _createDispatcher(__in ProcessID pid,
				        __in DispatcherID dspid,
				        __in EntryPointDesc entry,
				        __in uval dispatcherAddr,
				        __in uval initMsgLength,
				        __inbuf(initMsgLength)
					char *initMsg) = 0;

    virtual SysStatus _detachDispatcher(__in ProcessID pid,
				        __in DispatcherID dspid) = 0;
    virtual SysStatus _attachDispatcher(__in ProcessID pid,
				        __in DispatcherID dspid) = 0;
};

#endif /* #ifndef __CPU_CONTAINER_H_ */
