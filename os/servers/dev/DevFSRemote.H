#ifndef __DEVFS_REMOTE_H_
#define __DEVFS_REMOTE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevFSRemote.H,v 1.6 2004/02/27 17:14:20 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: DevFS files that are simply placeholders for
 *                     ObjectHandle's and giveAccess calls.
 *		       No ServerFile objects are ever created.
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#ifndef EXPORT_DEVFSREMOTE
#define EXPORT_DEVFSREMOTE
#include <sys/types.h>
#endif //EXPORT_DEVFSREMOTE

#include <cobj/Obj.H>
#include <cobj/ObjectHandle.H>
#include <misc/ListSimpleKeyLocked.H>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <stub/StubFRProvider.H>
#include <fslib/virtfs/FIVFAccessOH.H>
#include "FileSystemDev.H"
#ifndef _IN_STUBGEN
#include <meta/MetaDevFSRemote.H>
#else
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(a,b,c)
#endif // _IN_STUBGEN

class DevFSRemote;
typedef DevFSRemote** DevFSRemoteRef;
class DevFSRemote: public Obj {
#ifndef _IN_STUBGEN
    class __DevFSRemote: public FIVFAccessOH {
	char *name;
	uval token;
	friend class DevFSRemote;
    public:
	virtual SysStatus init(char* name, mode_t mode,
			       ObjectHandle oh, TypeID ohType, uval token);

	virtual SysStatus openCreateServerFile(ServerFileRef &fref,
					       uval oflag,
					       ProcessID pid,
					       ObjectHandle &oh,
					       uval &useType,
					       TypeID &type);
	virtual ~__DevFSRemote();
	DEFINE_GLOBAL_NEW(__DevFSRemote);
    };
    __DevFSRemote *dfsr;
#endif // _IN_STUBGEN

public:
    DevFSRemote() {};
    virtual ~DevFSRemote() {};
    DEFINE_GLOBALPADDED_NEW(DevFSRemote);
    DEFINE_REFS(DevFSRemote);
    DEFINE_ACCESS_SERVER(DevFSRemote, MetaObj::none, MetaObj::none);
    static void ClassInit();
    virtual SysStatus destroy();
    virtual SysStatus init(char* name, mode_t mode,
			   ObjectHandle oh, TypeID ohType,
			   ObjectHandle parent, ProcessID pid, uval token);

    virtual SysStatus exportedXObjectListEmpty();
__xpublic:
    static SysStatus _CreateNode(__inbuf(*) char* name,
				 __in mode_t mode,
				 __in ObjectHandle parent,
				 __in ObjectHandle obj,
				 __in TypeID objType,
				 __out ObjectHandle &nodeOH,
				 __in uval token = 0,
				 __CALLER_PID pid= 0); //StubDevFSRemote
};


#endif /* #ifndef __DEVFS_REMOTE_H_ */
