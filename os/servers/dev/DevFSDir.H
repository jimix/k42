#ifndef __DEVFS_DIR_H_
#define __DEVFS_DIR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevFSDir.H,v 1.6 2004/02/27 17:14:20 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface for keeping track of server states
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#ifndef EXPORT_DEVFSDIR
#define EXPORT_DEVFSDIR
#include <sys/types.h>
#endif //EXPORT_DEVFSDIR


#include <io/PathName.H>
#include <cobj/Obj.H>
#include <cobj/ObjectHandle.H>
#include <misc/ListSimpleKeyLocked.H>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <stub/StubFRProvider.H>
#ifndef _IN_STUBGEN
#include <meta/MetaDevFSDir.H>
#else
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(a,b,c)
#endif // _IN_STUBGEN

class FileSystemDev;
class DevFSDir;
typedef DevFSDir** DevFSDirRef;

class DevFSDir: public Obj {
#ifndef _IN_STUBGEN
protected:
    class __DevFSDir: public FileInfoVirtFSDirStatic {
	char *name;
	friend class DevFSDir;
    public:
	__DevFSDir():name(NULL) {};
	virtual ~__DevFSDir();
	virtual SysStatus init(char* name, mode_t mode);
	virtual SysStatusUval getServerFileType();
	virtual SysStatus createServerFileBlock(ServerFileRef &fref);
	virtual SysStatus destroy();
	virtual SysStatus deleteFile() {
	    return 0;
	}
	DEFINE_GLOBAL_NEW(__DevFSDir);

    };
    __DevFSDir *dir;
#endif // _IN_STUBGEN

public:

    DEFINE_GLOBALPADDED_NEW(DevFSDir);
    DEFINE_REFS(DevFSDir);
    DEFINE_ACCESS_SERVER(DevFSDir, MetaObj::none|MetaObj::controlAccess,
			 MetaObj::none);
    DevFSDir();
    virtual SysStatus getType(TypeID &id);
    virtual ~DevFSDir() {};
    virtual SysStatus destroy();
    static void ClassInit();
    static SysStatus Create(DevFSDirRef &ref, char* name, mode_t mode,
			    ObjectHandle parent, ProcessID pid);
    virtual SysStatus add(const char *nm, uval len, FileInfoVirtFS* finfo);
    virtual SysStatus getDir(FileInfoVirtFSDir* &dir);
    __xpublic:
    virtual SysStatus _unused() { return 0; };
};



#endif /* #ifndef __DEVFS_DIR_H_ */
