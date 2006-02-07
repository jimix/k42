/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxDevFS.C,v 1.6 2004/09/17 12:54:25 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface between linux/k42 devfs code
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stub/StubFileSystemDev.H>
#include <linux/limits.h>
#include <limits.h>
#include <sys/stat.h>

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

extern void LinuxEnvSuspend();
extern void LinuxEnvResume();

FairBLock devfsLock;

extern "C" void devfs_mk_dir (const char *fmt, ...);
extern "C" void devfs_remove (const char *fmt, ...);
extern "C" void devfs_mk_cdev (dev_t dev, mode_t mode, const char *fmt, ...);
extern "C" void devfs_mk_bdev (dev_t dev, mode_t mode, const char *fmt, ...);
extern "C" int vsnprintf(char *str, size_t size,
			 const char *format, va_list ap);

//
// This is what we use to keep track of the devfs entries that Linux
// code expects to be aroun, and the relationships between them
//
struct devfsEntry: public  devfs_entry{
    ObjectHandle nodeOH;   // OH to real entry in k42's devfs
    DEFINE_GLOBAL_NEW(devfsEntry);
    devfsEntry(const char *n, ObjectHandle oh, dev_t num = 0,
	       void* data=NULL)
	: nodeOH(oh) {
	parent = child = right = left = NULL;
	devInode= NULL;
	dev = num;

	info = data;
	strncpy(name, n, 64);
    };
    void addChild(devfsEntry *p) {
	p->parent = this;
	if (child) {
	    p->right = child;
	    p->left  = child->left;
	    // child->left == child?  , hence ordering is critical
	    child->left->right = p;
	    child->left = p;
	} else {
	    child = p;
	    p->left = p->right = p;
	}
    }
    void delChild(devfsEntry *p) {
	if (child==p && p->left==p) {
	    child = NULL;
	} else if (child==p) {
	    child = p->left;
	}
	if (child) {
	    devfs_entry* right = p->right;
	    devfs_entry* left = p->left;
	    right->left = p->left;
	    left->right = p->right;
	    p->left = p->right = p;
	}
    }
    devfsEntry* findChild(const char* n) {
	if (!child) return NULL;
	devfs_entry *curr = child;
	do {
	    if (strncmp(curr->name, n, 64)==0) {
		return (devfsEntry*)curr;
	    }
	    curr = curr->left;
	} while (curr!=child);
	return NULL;
    }
};

void
getDevFSOH(devfs_handle_t node, ObjectHandle &oh) {
    oh = ((devfsEntry*)node)->nodeOH;
}

//
// Every app/kernel needs to define these.  This is the mechanism by
// which K42 code gets a handle to linux device implementations.
extern SysStatus CreateDevice(ObjectHandle dirOH,
			      const char *name, mode_t mode,
			      unsigned int major,
			      unsigned int minor, ObjectHandle &device,
			      void* &devData);
extern SysStatus DestroyDevice(devfs_handle_t dev, unsigned int major,
			       unsigned int minor, void* devData);

static devfsEntry *root = NULL;

// This initialization step needs to be called from LinuxArchSys::PreInit(),
// if the caller wants to use devfs services
void k42devfsInit() {
    devfsLock.init();
    if (root == NULL) {
	ObjectHandle rootOH;
	SysStatus rc = StubFileSystemDev::_GetRoot("/dev", rootOH);
	passertMsg(_SUCCESS(rc),"can't get /dev root: %lx\n", rc);

	root = new devfsEntry("root", rootOH);
    }

}

devfs_handle_t
k42devfs_register (devfs_handle_t dir, const char *name,
		   unsigned int flags,
		   unsigned int major, unsigned int minor,
		   mode_t mode)
{
    SysStatus rc = 0;
    AutoLock<FairBLock> al(&devfsLock);
    devfsEntry* location = (devfsEntry*)dir;

    if (location==NULL) {
	location = root;
    }

    char buf[256];
    char buf2[256];
    const char* slash = NULL;
    const char* tmp = name;
    const char* x = tmp;
    const char* node = name;
    //Only looking for last '/' --- intermediate dirs created by devfs_mk_dir
    while (tmp=strchr(tmp,'/')) {
	slash = tmp;
	if (slash != x) {
	    memcpy(buf, x, slash-x);
	    buf[slash-x] = 0;
	    devfsEntry *next = location->findChild(buf);
	    if (!next) {
		memcpy(buf2, name, slash-name);
		buf2[slash-name] = 0;
		devfs_mk_dir(buf2);
		next = location->findChild(buf);
	    }
	    tassertMsg(next, "Missing dir\n");
	    location = next;
	}
	x = slash + 1;
	tmp = tmp+1;
	node = tmp;
    }

    tassertMsg(location->findChild(name)==NULL,
	       "Already have an entry for %s\n", name);

    ObjectHandle newNodeOH;
    // Tell k42 code that a device has appeared, to be associated with
    // location->nodeOH
    void* devData;
    LinuxEnvSuspend();
    rc = CreateDevice(location->nodeOH, node, mode, major, minor,
		      newNodeOH, devData);
    LinuxEnvResume();
    if (_FAILURE(rc)) {
	return NULL;
    }

    devfsEntry *de = new devfsEntry(node, newNodeOH,
				    new_encode_dev(MKDEV(major,minor)),
				    devData);

    de->nodeOH = newNodeOH;
    location->addChild(de);

    return (devfs_handle_t)de;
}



devfs_handle_t
k42devfs_mk_cdev (dev_t dev, mode_t mode, char *name)
{
    return k42devfs_register(NULL, name, 0, MAJOR(dev), MINOR(dev), mode);
}

devfs_handle_t
k42devfs_mk_bdev (dev_t dev, mode_t mode, char *name)
{
    return k42devfs_register(NULL, name, 0, MAJOR(dev), MINOR(dev), mode);
}

devfs_handle_t
k42devfs_mk_dir (const char *name)
{
    SysStatus rc;
    char tmp[PATH_MAX+1];
    char* slash= strchr(name,'/');
    const char* next = name;

    devfsEntry* pwd = root;

    devfsEntry* node = NULL;
    do {
	name = next;
	if (slash) {
	    memcpy(tmp, name, slash-name);
	    tmp[slash-name]=0;
	    next = slash+1;
	    slash = strchr(next,'/');
	} else {
	    strcpy(tmp,name);
	    next = NULL;
	}
	node = pwd->findChild(tmp);
	if (!node) {
	    ObjectHandle newOH;
	    LinuxEnvSuspend();
	    rc = StubFileSystemDev::_Create(tmp, S_IFDIR|0755,
					    pwd->nodeOH, newOH);
	    LinuxEnvResume();
	    tassertWrn(_SUCCESS(rc),"Can't make devfs dir node %s: %lx\n",
		       tmp, rc);

	    node = new devfsEntry(tmp, newOH);
	    pwd->addChild(node);
	}
	pwd = node;
    } while (next);
    return node;
}

void
k42devfs_remove(const char* name) {
    SysStatus rc;
    char tmp[PATH_MAX+1];
    char* slash= strchr(name,'/');
    const char* next = name;

    devfsEntry* pwd = root;

    devfsEntry* node = NULL;
    AutoLock<FairBLock> al(&devfsLock);
    do {
	name = next;
	if (slash) {
	    memcpy(tmp, name, slash-name);
	    tmp[slash-name]=0;
	    next = slash+1;
	    slash = strchr(next,'/');
	} else {
	    strcpy(tmp,name);
	    next = NULL;
	}
	node = pwd->findChild(tmp);
	if (!node) {
	    break;
	}
	pwd = node;
    } while (next);
    if (!node) {
	err_printf("Couldn't destroy %s\n", name);
	return;
    }

    if (node->parent) {
	((devfsEntry*)node->parent)->delChild(node);
    }
    LinuxEnvSuspend();
    rc = DestroyDevice(node, MAJOR(node->dev), MINOR(node->dev), node->info);
    LinuxEnvResume();
    tassertRC(rc, "device node destruction");

    Obj::AsyncReleaseAccess(node->nodeOH);
    delete node;

}

void
devfs_remove (const char *fmt, ...)
{
    char buf[64];
    int n;
    va_list args;
    va_start(args, fmt);
    n = vsnprintf(buf, 64, fmt, args);
    va_end(args);
    k42devfs_remove(buf);
}

void
devfs_mk_dir (const char *fmt, ...)
{
    char buf[64];
    int n;
    va_list args;
    va_start(args, fmt);
    n = vsnprintf(buf, 64, fmt, args);
    va_end(args);
    k42devfs_mk_dir(buf);
}

void
devfs_mk_bdev (dev_t dev, mode_t mode, const char *fmt, ...)
{
    char buf[64];
    int n;
    va_list args;
    va_start(args, fmt);
    n = vsnprintf(buf, 64, fmt, args);
    va_end(args);
    k42devfs_mk_bdev(dev, mode, buf);
}

void
devfs_mk_cdev (dev_t dev, mode_t mode, const char *fmt, ...)
{
    char buf[64];
    int n;
    va_list args;
    va_start(args, fmt);
    n = vsnprintf(buf, 64, fmt, args);
    va_end(args);
    k42devfs_mk_cdev(dev, mode, buf);
}



devfs_handle_t devfs_get_parent (devfs_handle_t de)
{
    if (de) {
	return (devfs_handle_t)((devfsEntry*)de)->parent;
    }
    return (devfs_handle_t)NULL;
}


int devfs_generate_path(devfs_handle_t de, char* buf, int buflen)
{
    devfs_entry *d = (devfs_entry*)de;
    int end=buflen-1;

    buf[end] = 0;
    do {
	int currLen = strnlen(d->name,64);
	end-= currLen;
	if (end<0) return -ENAMETOOLONG;

	memcpy(buf+end, d->name, currLen);

	if (d->parent && d->parent!=root) {
	    --end;
	    if (end<0) return -ENAMETOOLONG;
	    buf[end]='/';
	}
    } while ((d=d->parent) && (devfs_entry*)d->parent!=root);

    return end;
}


void k42devfs_unregister (devfs_handle_t dir)
{
    AutoLock<FairBLock> al(&devfsLock);
    if (!dir) return;
    devfsEntry* de = (devfsEntry*)dir;
    if (de->parent) {
	((devfsEntry*)de->parent)->delChild(de);
    }
    Obj::AsyncReleaseAccess(de->nodeOH);
    delete de;
}

