#ifndef __DEVFS_BLK_H_
#define __DEVFS_BLK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevFSBlk.H,v 1.8 2004/02/27 17:14:20 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: DevFS entries for block devices.  These devices
 *		       may be opened as files, and thus have ServerFile
 *		       objects associated with them.
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#ifndef EXPORT_DEVFSBLK
#define EXPORT_DEVFSBLK
#include <sys/types.h>
#endif //EXPORT_DEVFSBLK

#include <cobj/Obj.H>
#include <cobj/ObjectHandle.H>
#include <misc/ListSimpleKeyLocked.H>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <stub/StubFRProvider.H>
#include "FileSystemDev.H"
#include "FileInfoDev.H"
#ifndef _IN_STUBGEN
#include <meta/MetaDevFSBlk.H>
#else
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(a,b,c)
#endif // _IN_STUBGEN

class DevFSBlk;
typedef DevFSBlk** DevFSBlkRef;
class DevFSBlk: public Obj {

    char *name;
    uval token;
#ifndef _IN_STUBGEN
    class __DevFSBlk: public FileInfoDev {
	StubFRProvider stubFRP;
	friend class DevFSBlk;
    public:
	virtual void init(const char* n, dev_t dev,
			  uval mode, uval rdev,
			  uval uid, uval gid, ObjectHandle oh);
	__DevFSBlk():stubFRP(StubBaseObj::UNINITIALIZED) {};
	virtual SysStatus createServerFileBlock(ServerFileRef &fref);
	DEFINE_GLOBAL_NEW(__DevFSBlk);
    };

    __DevFSBlk *dfsb;
#endif // _IN_STUBGEN
public:
    DevFSBlk() {};
    virtual SysStatus destroy();
    DEFINE_GLOBALPADDED_NEW(DevFSBlk);
    DEFINE_REFS(DevFSBlk);
    DEFINE_ACCESS_SERVER(DevFSBlk, MetaObj::controlAccess, MetaObj::none);
    virtual SysStatus init(char* name, dev_t dev, mode_t mode,
			   ObjectHandle remotePageServer, uval token);
    static void ClassInit();
    virtual SysStatus exportedXObjectListEmpty();
__xpublic:

     // frProvider object has method to signal completion
     static SysStatus _CreateNode(__inbuf(*) char* name,
				  __in dev_t devID,
				  __in mode_t mode,
				  __in ObjectHandle parent,
				  __in ObjectHandle frProvider,
				  __out ObjectHandle &nodeOH,
				  __in uval token=0,
				  __CALLER_PID pid=0); //StubDevFSBlk
    static __async SysStatus _KickCreateNode(__in ObjectHandle oh,
					     __CALLER_PID pid);
};


#endif /* #ifndef __DEVFS_BLK_H_ */
