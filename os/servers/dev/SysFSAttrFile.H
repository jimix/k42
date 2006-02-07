#ifndef __SYSFS_ATTRFILE_H_
#define __SYSFS_ATTRFILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysFSAttrFile.H,v 1.1 2004/02/27 17:14:19 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Sysfs node - dirs with attributes posing as files
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#ifndef EXPORT_SYSFSATTRFILE
#define EXPORT_SYSFSATTRFILE
#include <sys/types.h>
#endif //EXPORT_SYSFSATTRFILE

#include <cobj/Obj.H>
#include <cobj/ObjectHandle.H>
#include <misc/ListSimpleKeyLocked.H>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <stub/StubFRProvider.H>
#include "FileSystemDev.H"
#include "FileInfoDev.H"
#ifndef _IN_STUBGEN
#include <meta/MetaSysFSAttrFile.H>
#else
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(a,b,c)
#endif // _IN_STUBGEN

class SysFSAttrFile;
typedef SysFSAttrFile** SysFSAttrFileRef;
class SysFSAttrFile: public Obj {

    char *name;
    uval token;
#ifndef _IN_STUBGEN
    class __SysFSAttrFile: public FileInfoDev {
	ObjectHandle virtFileOH;
	uval token;
	friend class SysFSAttrFile;
    public:
	virtual void init(const char* n, dev_t dev,
			  uval mode, uval rdev,
			  uval uid, uval gid, uval tok,
			  ObjectHandle oh);
	__SysFSAttrFile() {};
	virtual SysStatusUval getServerFileType();
	virtual ~__SysFSAttrFile() {};
	virtual SysStatus createServerFileBlock(ServerFileRef &fref);
	virtual SysStatus getOH(ObjectHandle &oh) {
	    oh = virtFileOH;
	    return 0;
	}

	DEFINE_GLOBAL_NEW(__SysFSAttrFile);
    };

    __SysFSAttrFile *sfaf;
#endif // _IN_STUBGEN
public:
    SysFSAttrFile() {};
    virtual SysStatus destroy();
    DEFINE_GLOBALPADDED_NEW(SysFSAttrFile);
    DEFINE_REFS(SysFSAttrFile);
    DEFINE_ACCESS_SERVER(SysFSAttrFile, MetaObj::none|MetaObj::controlAccess,
			 MetaObj::none);
    virtual SysStatus init(char* name, dev_t dev, mode_t mode,
			   ObjectHandle virtFileProvider, uval token);
    static void ClassInit();
__xpublic:
    static SysStatus _CreateNode(__inbuf(*) char* name,
				 __in mode_t mode,
				 __in ObjectHandle parent,
				 __in ObjectHandle virtFileProvider,
				 __in uval token,
				 __out ObjectHandle &oh,
				 __CALLER_PID pid=0); //StubSysFSAttrFile
};


#endif /* #ifndef __SYSFS_ATTRFILE_H_ */
