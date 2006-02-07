/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemExt2.C,v 1.13 2004/11/04 03:54:05 dilma Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileSystemExt2.H"
#include "FSFileExt2.H"
#include <cobj/CObjRootSingleRep.H>
#include <fslib/DirLinuxFS.H>
#include <fslib/NameTreeLinuxFS.H>

#include <io/DiskClient.H>
#include <fslib/PagingTransportPA.H>
#include <stub/StubKernelPagingTransportPA.H>

#include <lk/LinuxEnv.H>
#include "Ext2Disk.H"

extern "C" {
#include <linux/k42fs.h>
}

#include <stdio.h>

extern "C" void vfs_caches_init(unsigned long numpages);
extern "C" int init_ext2_fs();

extern void ConfigureLinuxEnv(VPNum vp,PageAllocatorRef pa);
extern void ConfigureLinuxGeneric(VPNum vp);
extern void ConfigureLinuxFinal(VPNum vp);
//extern void LinuxStartVP(VPNum vp, SysStatus (*initfn)(uval arg));

/* static */ SysStatus
FileSystemExt2::Create(char *diskPath, char *mpath, uval flags)
{
    err_printf("In FileSystemExt2::Create diskPath %s, mpath %s (pid 0x%lx)\n",
	       diskPath, mpath, _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));

    if (KernelInfo::OnSim() == 2 /* SIM_MAMBO*/ ) { // FIXME:use symbol
	// For now we have mambo disk 3 as the only place we boot with ext2
        // disk

	const char *expectedName[1] = {"/dev/mambobd/3"};
	tassertMsg(
	    strncmp(diskPath, expectedName[0], strlen(expectedName[0])) == 0,
	    "diskPath is %s\n", diskPath);
    } else {
	err_printf("KFS being initialized with device %s\n", diskPath);
    }

    FileSystemExt2 *obj = new FileSystemExt2();
    passertMsg(obj != NULL, "ops");
    void *mnt, *dentry;
    SysStatus rc = obj->init(diskPath, flags, &mnt, &dentry);
    tassertWrn(_SUCCESS(rc), "FileSystemExt2::init failed w/ rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    // create the clustered object for the file system
    FileSystemExt2Ref fsRef = (FileSystemExt2Ref)
	CObjRootSingleRep::Create(obj);
    (void)fsRef; // We're not using this reference for anything so far

    // create PagingTransport Object
    ObjectHandle fsptoh;
    rc = PagingTransportPA::Create(obj->tref, fsptoh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    ObjectHandle kptoh, sfroh;
    // asks the kernel to create a KernelPagingTransport
    rc = StubKernelPagingTransportPA::_Create(fsptoh, kptoh, sfroh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    tassertMsg(kptoh.valid(), "ops");
    DREF(obj->tref)->setKernelPagingOH(kptoh, sfroh);

    // Get the root directory
    DirLinuxFSRef dir;
    FSFile *fi = new FSFileExt2(obj->tref, mnt, dentry);
    passertMsg(fi != NULL, "new failed\n");
    DirLinuxFS::CreateTopDir(dir, ".", fi);

    // description of mountpoint
    char tbuf[256];
    const char *rw = (flags & MS_RDONLY ? "r" : "rw");
    sprintf(tbuf, "%s ext2 %s pid 0x%lx", diskPath, rw,
	    _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));

    NameTreeLinuxFS::Create(mpath, dir, tbuf, strlen(tbuf));

    return 0;
}

SysStatus
FileSystemExt2::init(char *diskPath, uval flags, void **mnt, void **dentry)
{
    SysStatus rc;

    /* invoking linux code to initialize this server */
    LinuxEnv le(SysCall);
    int ret = init_ext2_fs();
    err_printf("init_ext2_fs returned %d\n", ret);

    // FIXME: this would fit better in class Ext2Disk
    // get a handle to the disk
    DiskClientRef dcr;
    if (flags & MS_RDONLY) {
	rc = DiskClientRO::Create(dcr, diskPath);
    } else {
	rc = DiskClient::Create(dcr, diskPath);
    }

    tassertMsg(_SUCCESS(rc), "DiskClient(RO) failed with rc 0x%lx\n", rc);
    SysStatusUval rcuval = DREF(dcr)->getBlkSize();
    tassertMsg(_SUCCESS(rcuval), "getBlkSize failed with 0x%lx\n", rcuval);
    unsigned long blkSize = (unsigned long) _SGETUVAL(rcuval);
    err_printf("got blkSize %ld\n", blkSize);

    Ext2Disk *disk = new Ext2Disk(dcr, blkSize);
    passertMsg(disk != NULL, "no mem\n");

    err_printf("We're going to invoke k42_get_root (size of unsigned long"
	       " is %ld)\n", sizeof(unsigned long));
    ret = k42_get_root(0, (const char*)diskPath, NULL, blkSize,
		       mnt, dentry);
    err_printf("ret is %d, got mnt %p and rdentry %p\n", ret, mnt, dentry);

    return 0;
}

/* static */ SysStatus
FileSystemExt2::ClassInit(VPNum vp)
{

    ConfigureLinuxEnv(vp, GOBJ(ThePageAllocatorRef));
    ConfigureLinuxGeneric(vp);

    if (vp == 0) {

	LinuxEnv le(SysCall);
	err_printf("Invoking vfs_caches_init\n");
	vfs_caches_init(0x1<<8);
	//driver_init();

	//console_init();

	VPNum vpCnt = _SGETUVAL(DREFGOBJ(TheProcessRef)->vpCount());
	for (VPNum i = 1; i < vpCnt; i++) {
	    err_printf("Do we have anything to start on each virtual processor?\n");
	    //LinuxStartVP(i, LinuxPTYServer::ClassInit);
	}


	ConfigureLinuxFinal(0);
    }

    return 0;
}
