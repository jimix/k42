#ifndef __DEVFS_NODE_H_
#define __DEVFS_NODE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevFSNode.H,v 1.5 2003/01/16 06:09:44 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface for keeping track of server states
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#ifndef EXPORT_DEVFSNODE
#define EXPORT_DEVFSNODE
#include <sys/types.h>
#endif //EXPORT_DEVFSNODE

#include <cobj/Obj.H>
#include <cobj/ObjectHandle.H>
#include <misc/ListSimpleKeyLocked.H>
#include <meta/MetaServerState.H>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <fslib/virtfs/FileSystemVirtFS.H>
#include <stub/StubFRProvider.H>
#ifndef _IN_STUBGEN
#include <meta/MetaDevFSNode.H>
#else
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(a,b,c)
#endif // _IN_STUBGEN

class DevFSNode;
typedef DevFSNode** DevFSNodeRef;
class DevFSNode: public FileInfoVirtFS __xbase(Obj) {

    char *name;
    ServerFileRef sfRef;
    StubFRProvider stubFRP;
    static FileSystemVirtFSRef fs;
public:
    DevFSNode():name(NULL),sfRef(NULL), stubFRP(StubBaseObj::UNINITIALIZED) {};
    virtual ~DevFSNode();
    DEFINE_GLOBALPADDED_NEW(DevFSNode);
    DEFINE_REFS(DevFSNode);
    DEFINE_ACCESS_SERVER(DevFSNode, MetaObj::none, MetaObj::none);
    virtual SysStatus init(char* name, uval namelen, mode_t mode,
			   ObjectHandle remotePageServer);
    virtual SysStatusUval getServerFileType(FileSystemGlobal::FileToken token);
    virtual SysStatus getServerFile(FSFile *fsFile, ServerFileRef &fref);
    virtual SysStatus deleteFile(FileSystemGlobal::FileToken token) {
	return 0;
    }
    static void ClassInit(VPNum vp, FileSystemVirtFSRef fsvRev);
__xpublic:
    static SysStatus _createNode(__inbuf(namelen) char* name,
				 __in uval namelen,
				 __in mode_t mode,
				 __in ObjectHandle frProvider,
				 __out ObjectHandle &oh,
				 __CALLER_PID pid); //StubDevFSNode
    virtual SysStatus _deleteNode();
};


#endif /* #ifndef __DEVFS_NODE_H_ */
