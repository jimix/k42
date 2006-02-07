/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysFS.C,v 1.4 2004/09/17 12:54:25 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface between linux/k42 devfs code
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/BaseObj.H>
#include <cobj/XHandleTrans.H>
#include <cobj/TypeMgr.H>
#include <stub/StubFileSystemDev.H>
#include <stub/StubSysFSAttrFile.H>
#include <cobj/CObjRootSingleRep.H>
#include <io/VirtFile.H>
#include <meta/MetaVirtFile.H>
#include <linux/limits.h>
#include <limits.h>
#include <sys/stat.h>
#include <misc/HashSimple.H>
#include <misc/baseStdio.H>
#include <alloc/PageAllocatorDefault.H>
#include <sys/ppccore.H>
extern "C"{
//asmlinkage int printk(const char * fmt, ...)
//	__attribute__ ((format (printf, 1, 2)));
#undef major
#undef minor
#undef MAJOR
#undef MINOR
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
#include <linux/kdev_t.h>
#include <linux/k42devfs.h>
}

extern "C" {
#undef PAGE_SIZE
#undef PAGE_MASK
#undef INT_MAX
#undef UINT_MAX
#undef LONG_MAX
#undef LONG_MIN
#undef ULONG_MAX
#undef __attribute_used__
#undef __attribute_pure__
#undef likely
#undef unlikely
#define ffs lk_ffs
#define eieio linux_eieio
#include <asm/bitops.h>
#undef eieio
#undef ffs
#define new __C__new
#define private __C__private
#include <asm/system.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#undef new
#undef private
}

#include "LinuxEnv.H"

#define to_subsys(k) container_of(k,struct subsystem,kset.kobj)
#define to_sattr(a) container_of(a,struct subsys_attribute,attr)

/**
 * Subsystem file operations.
 * These operations allow subsystems to have files that can be
 * read/written.
 */
static ssize_t
subsys_attr_show(struct kobject * kobj, struct attribute * attr, char * page)
{
	struct subsystem * s = to_subsys(kobj);
	struct subsys_attribute * sattr = to_sattr(attr);
	ssize_t ret = 0;

	if (sattr->show)
		ret = sattr->show(s,page);
	return ret;
}

static ssize_t
subsys_attr_store(struct kobject * kobj, struct attribute * attr,
		  const char * page, size_t count)
{
	struct subsystem * s = to_subsys(kobj);
	struct subsys_attribute * sattr = to_sattr(attr);
	ssize_t ret = 0;

	if (sattr->store)
		ret = sattr->store(s,page,count);
	return ret;
}

static struct sysfs_ops subsys_sysfs_ops = {
    subsys_attr_show, subsys_attr_store
};


struct SysFSNode;
typedef SysFSNode** SysFSNodeRef;
struct SysFSNode:public VirtFile {
    DEFINE_GLOBAL_NEW(SysFSNode);
    DEFINE_REFS(SysFSNode);
    DEFINE_ACCESS_SERVER(VirtFile, MetaObj::none, MetaObj::none);
protected:
    FairBLock lock;
    uval refcount;
    uval exportCount;
    struct kobject *kobj;
    SysFSNodeRef parent;
    ObjectHandle exportOH;  // OH we give to fs server
			    // This could be the same one for this node (dir)
			    // and all attribute files within it
    ObjectHandle dirOH;
    uval baseToken;

    HashSimple<uval, struct attribute*, AllocGlobal, 0> hashAttr;
    static ObjectHandle SysRoot;
public:
    struct OpenFile {
	DEFINE_GLOBAL_NEW(OpenFile);
	char* buffer;
	uval count;
	uval position;
	struct attribute *attr;
	struct sysfs_ops *ops;
	OpenFile(struct attribute * _attr):
	    buffer(NULL), count(0), position(0), attr(_attr),ops(NULL) {};
	~OpenFile() {
	    if (buffer) {
		DREFGOBJ(ThePageAllocatorRef)->deallocPages((uval)buffer,
							    PAGE_SIZE);
	    }
	}
    };
protected:
    HashSimple<uval, struct OpenFile*, AllocGlobal, 0> hashOpen;
    virtual SysStatus getFilledOpenFile(OpenFile* &of,uval srvToken);
public:
    virtual SysStatus addAttr(struct attribute *attr);

    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _readOff(__outbuf(__rc:length) char  *buf,
				   __in uval length, __in uval offset,
				   __in uval userData, __in uval srvToken);
    virtual SysStatusUval locked_readOff(char  *buf, uval length, uval offset,
					 uval userData, uval srvToken);

    // synchronous read interface without offset
    virtual SysStatusUval _read(char  *buf, uval buflength,
				uval userData, uval srvToken);
    virtual SysStatusUval _write(__inbuf(length) const char *buf,
				 __in uval length, __in uval userData,
				 __in uval srvToken) {
	return 0;
    }
    virtual SysStatus _getMaxReadSize(__out uval &max_read_size,
				      __in uval srvToken);
    virtual SysStatus _open(__in uval flags, __in uval userData,
			    __inout uval &srvToken);
    virtual SysStatus _close(__in uval userData, __in uval srvToken);
    virtual SysStatus _setFilePosition(__in sval position, __in uval at,
				       __in uval userData,
				       __in uval srvToken);

    static SysStatus Create(struct kobject *obj, mode_t mode);
    virtual SysStatus init(struct kobject *obj, mode_t mode);

    virtual SysStatus locked_fillBuffer(OpenFile *of);
    virtual SysStatus destroy();
    SysFSNode() {};
    static void ClassInit();
protected:
    virtual SysStatus get() {
	AtomicAdd(&refcount, 1);
	return 0;
    }
    virtual SysStatus put() {
	AtomicAdd(&refcount, ~0ULL);
	return 0;
    }
    virtual SysStatus getOH(ObjectHandle &oh) {
	oh = dirOH;
	return 0;
    }
    static uval getID() {
	static uval fileID = 0;
	AtomicAdd(&fileID, 1);
	return fileID;
    }
};


void
SysFSNode_ClassInit()
{
    SysFSNode::ClassInit();
}

ObjectHandle SysFSNode::SysRoot;

/* static */ void
SysFSNode::ClassInit()
{
    MetaVirtFile::init();
    SysStatus rc = StubFileSystemDev::_GetRoot("/sys", SysRoot);
    passertMsg(_SUCCESS(rc),"can't get /lsys root: %lx\n", rc);

}

/* static */ SysStatus
SysFSNode::Create(struct kobject *obj, mode_t mode)
{
    SysFSNode *sn = new SysFSNode;
    return sn->init(obj, mode);
}

/* virtual */ SysStatus
SysFSNode::init(struct kobject *obj, mode_t mode)
{
    SysStatus rc = 0;
    ObjectHandle p;

    kobj = kobject_get(obj);
    if (kobj && kobj->parent) {
	parent = (SysFSNodeRef)kobj->parent->dentry;

	rc = DREF(parent)->get();
	tassertRC(rc, "Failed to get parent\n");

	rc = DREF(parent)->getOH(p);
	tassertRC(rc, "Failed to get parent OH\n");
    } else {
	p = SysRoot;
    }
    get();  // inc our ref count to account from reference
	    // from obj->parent->dentry


    ObjectHandle newOH;

    CObjRootSingleRep::Create(this);

    baseToken = getID();

    rc = StubFileSystemDev::_Create((char *)kobject_name(kobj), mode, p, dirOH);

    if (_FAILURE(rc) && _SGENCD(rc)==EEXIST) {
	char buf[strlen(kobject_name(kobj))+20];
	ProcessID pid = DREFGOBJ(TheProcessRef)->getPID();
	baseSprintf(buf, "%s@%ld", kobject_name(kobj), pid);
	rc = StubFileSystemDev::_Create(buf, mode, p, dirOH);

    }
    tassertWrn(_SUCCESS(rc),"Can't make sysfs dir node %s: %lx\n",
	       kobject_name(kobj), rc);

    kobj->dentry = (struct dentry*)getRef();

    return rc;
}

/* virtual */ SysStatus
SysFSNode::addAttr(struct attribute *attr)
{
    SysStatus rc;

    uval id = getID();
    AutoLock<FairBLock> al(&lock);
    hashAttr.add(id, attr);
    ObjectHandle attrOH;
    rc = giveAccessByServer(exportOH, dirOH.pid());
    tassertRC(rc, "giveAccess failure\n");

    rc = StubSysFSAttrFile::_CreateNode((char *)attr->name, S_IFREG| attr->mode,
					dirOH, exportOH, id, attrOH);

    if (_SUCCESS(rc)) {
	AtomicAdd(&exportCount, 1);
    }
    tassertWrn(_SUCCESS(rc), "create sysfs node %lx\n",rc);

    return 0;
}

/* virtual */ SysStatus
SysFSNode::_open(__in uval flags, __in uval userData,
		 __inout uval &srvToken)
{
    // srvToken identifies the directory containing the attributes.
    // Replace it with a token identifying the open instance of an
    // attribute file.

    AutoLock<FairBLock> al(&lock);
    struct attribute * attr;
    if (!hashAttr.find(srvToken, attr)) {
	return _SERROR(2782, 0, ENOENT);
    }
    uval newToken = getID();

    struct OpenFile* of = new OpenFile(attr);
    srvToken = newToken;
    hashOpen.add(newToken, of);
    return 0;
}


/* virtual */ SysStatusUval
SysFSNode::_read(char  *buf, uval buflength,
		 uval userData, uval srvToken)
{
    uval position;
    AutoLock<FairBLock> al(&lock);
    OpenFile *of;
    if (!hashOpen.find(srvToken, of)) {
	return _SERROR(2781, 0, ENOENT);
    }
    position = of->position;

    // FIXME race, we've unlocked
    SysStatusUval rc = locked_readOff(buf, buflength, position,
				      userData, srvToken);
    if (_SUCCESS(rc)) {
	of->position += _SGETUVAL(rc);
    }
    return rc;
}

/* virtual */ SysStatusUval
SysFSNode::_readOff(__outbuf(__rc:length) char  *buf,
		    __in uval length, __in uval offset,
		    __in uval userData, __in uval srvToken)
{
    AutoLock<FairBLock> al(&lock);
    return locked_readOff(buf, length, offset, userData, srvToken);
}

/* virtual */ SysStatus
SysFSNode::_getMaxReadSize(__out uval &max_read_size,
			   __in uval srvToken)
{
    // FIXME: this's a copy of the definition of FileLinuxVirtFile::MAX_IO_LOAD
    max_read_size = PPCPAGE_LENGTH_MAX - 2*sizeof(uval) - sizeof(__XHANDLE);
    return 0;
}

/* virtual */ SysStatus
SysFSNode::locked_fillBuffer(OpenFile *of)
{
    LinuxEnv le(SysCall);
    struct sysfs_ops * ops = NULL;
    if (kobj->kset && kobj->kset->ktype)
	of->ops = kobj->kset->ktype->sysfs_ops;
    else if (kobj->ktype)
	of->ops = kobj->ktype->sysfs_ops;
    else
	of->ops = &subsys_sysfs_ops;

    ssize_t ret = (*of->ops->show)(kobj, of->attr, of->buffer);

    if (ret>=0) {
	of->count = ret;
    } else {
	return _SERROR(2785, 0, -ret);
    }
    return 0;
}

/* virtual */ SysStatus
SysFSNode::getFilledOpenFile(OpenFile* &of, uval srvToken)
{
    SysStatusUval rc = 0;
    if (!hashOpen.find(srvToken, of)) {
	return _SERROR(2784, 0, ENOENT);
    }
    if (!of->buffer) {
	uval page;
	rc = DREFGOBJ(ThePageAllocatorRef)->allocPages(page, PAGE_SIZE);
	_IF_FAILURE_RET(rc);
	of->buffer = (char*)page;

	return locked_fillBuffer(of);
    }
    return 0;
}

/* virtual */ SysStatusUval
SysFSNode::locked_readOff(char  *buf, uval length, uval offset,
			  uval userData, uval srvToken)
{
    OpenFile *of;
    SysStatus rc = getFilledOpenFile(of, srvToken);
    if (offset > of->count) {
	length = 0;
    }
    if (length> of->count - offset) {
	length = of->count - offset;
    }
    memcpy(buf, of->buffer, length);
    return length;
}


/* virtual */ SysStatus
SysFSNode::_setFilePosition(__in sval position, __in uval at,
			    __in uval userData, __in uval srvToken)
{
    SysStatus rc;
    AutoLock<FairBLock> al(&lock);
    OpenFile *of = NULL;
    if (!hashOpen.find(srvToken, of)) {
	return _SERROR(2785, 0, ENOENT);
    }

    // FIXME these should come from FileLinux.H, but we can't include it easily
    // enum for operations that take offset
    enum At {APPEND,RELATIVE,ABSOLUTE};

    switch (at) {
    case ABSOLUTE:
	of->position = position;
	break;
    case RELATIVE:
	of->position += position;
	break;
    case APPEND:
	of->position = of->count + position;
	break;
    default:
	rc = _SERROR(2783, 0, EINVAL);
    }
    return rc;
}

/* virtual */ SysStatus
SysFSNode::_close(__in uval userData, __in uval srvToken)
{
    AutoLock<FairBLock> al(&lock);
    OpenFile *of = NULL;
    if (!hashOpen.remove(srvToken, of)) {
	return _SERROR(2787, 0, EINVAL);
    }
    delete of;
}

/* virtual */ SysStatus
SysFSNode::destroy()
{
    SysStatus rc;
    AutoLock<FairBLock> al(&lock);
    // FIXME: Blow everything away.  No regard for any open files.
    uval restart;
    uval token;
    OpenFile *of;

    while (hashOpen.removeNext(token, of, restart)) {
	delete of;
    }

    Obj::AsyncReleaseAccess(dirOH);

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }
    return rc;
}

int
sysfs_create_dir(struct kobject *kobj)
{
    if (!kobj) return 0;

    LinuxEnvSuspend();
    SysStatus rc = SysFSNode::Create(kobj, S_IFDIR|0755);
    tassertRC(rc, "SysFSNode creation\n");
    LinuxEnvResume();

    return 0;
}

int
sysfs_create_file(struct kobject *kobj, const struct attribute *attr)
{
    if (!kobj || !kobj->dentry) return 0;

    SysFSNodeRef ref = (SysFSNodeRef)kobj->dentry;

    LinuxEnvSuspend();
    SysStatus rc = DREF(ref)->addAttr((struct attribute*)attr);
    LinuxEnvResume();

    tassertRC(rc, "Adding attr\n");

    return  0;
}

extern "C" void
sysfs_remove_dir(struct kobject *kobj);

void
sysfs_remove_dir(struct kobject *kobj)
{
    SysFSNodeRef ref = (SysFSNodeRef)kobj->dentry;
    LinuxEnvSuspend();
    DREF(ref)->destroy();
    LinuxEnvResume();
}

extern "C" int call_usermodehelper(char *path, char *argv[],
				   char *envp[], int wait);

int
call_usermodehelper(char *path, char *argv[], char *envp[], int wait)
{
    // One day we could hook into this.  Probably could even use std linux udev
    // utilities
#if 0
    int x = 0;
    err_printf("user mode helper: ");
    while (argv[x]) {
	err_printf("%s ", argv[x++]);
    }
    x = 0;
    while (envp[x]) {
	err_printf("%s ", envp[x++]);
    }
    err_printf("\n");
#endif
    return 0;
}
