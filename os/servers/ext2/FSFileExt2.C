/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFileExt2.C,v 1.42 2004/11/04 03:54:04 dilma Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FSFileExt2.H"
#include "ServerFileBlockExt2.H"
#include "ServerFileDirExt2.H"

#include "Ext2Conversion.H"

#include "Ext2Disk.H"

#include <lk/LinuxEnv.H>

extern "C" {
#include <linux/k42fs.h>
}

/* virtual */ SysStatus
FSFileExt2::createServerFileBlock(ServerFileRef &ref)
{
    ObjectHandle oh;
    tassertMsg(tref != NULL, "?");
    SysStatus rc = DREF(tref)->getKptoh(oh);
    tassertMsg(_SUCCESS(rc) && oh.valid(), "?");	
    return ServerFileBlockExt2::Create(ref, this, oh);
}

/* virtual */ SysStatus
FSFileExt2::createDirLinuxFS(DirLinuxFSRef &rf,
			     PathName *pathName, uval pathLen,
			     DirLinuxFSRef par)
{
    return ServerFileDirExt2::Create(rf, pathName, pathLen, this, par);
}

/* virtual */ SysStatus
FSFileExt2::getStatus(FileLinux::Stat *stat)
{
    LinuxEnv le(SysCall);
    return callStat(mnt, dentry, (void *) stat);
}

/* virtual */ SysStatusUval
FSFileExt2::getDents(uval &cookie, struct direntk42 *buf,
		     uval len)
{
    LinuxEnv le(SysCall);
    SysStatusUval ret = callGetDents(dentry, cookie, buf, len);

    return ret;
}

/* virtual */ SysStatus
FSFileExt2::getFSFileOrServerFile(
	char *entryName, uval entryLen, FSFile **entryInfo,
	ServerFileRef &ref, MultiLinkMgrLock* &lock,
	FileLinux::Stat *stat /* = NULL */)
{
    SysStatus rc = 0;
    void *d = NULL;
    char compname[PATH_MAX+1];
    memcpy(compname, entryName, entryLen);
    compname[entryLen] = '\0';

    LinuxEnv le(SysCall);
    int error = k42_lookup(dentry, (const char*) compname, mnt, &d);
    if (error != 0) {
	// I don't know if we get positive or negative errors
	tassertMsg(error < 0,"is this assumption wrong?");
	return _SERROR(2822, 0, -error);
    }

    if (d) {
	*entryInfo = new FSFileExt2(tref, mnt, d);
	passertMsg(*entryInfo != NULL, "new failed\n");
    
	// FIXME: handle multiple links
	
	if (stat) {
	    rc = callStat(mnt, d, (void *) stat);
	}
    } else {
	rc = _SERROR(2823, 0, ENOENT);
    }

    return rc;
}

/* virtual */ uval
FSFileExt2::isReadOnly()
{
    LinuxEnv le(SysCall);
    uval ret = k42_is_fs_read_only(this->dentry);

    return ret;
}

/* virtual */ SysStatus
FSFileExt2::updateFileLength(uval fileLength)
{
    err_printf("<-- %s: fileLength = %lu\n", __PRETTY_FUNCTION__, fileLength);

    SysStatus rc;

    LinuxEnv le(SysCall);
    rc = k42_update_file_length(this->dentry, this->mnt, fileLength);

    return rc;
}

/* virtual */ SysStatus
FSFileExt2::ftruncate(off_t length)
{
    SysStatus rc;

    LinuxEnv le(SysCall);
    rc = k42_ftruncate(this->dentry, this->mnt, length);

    return rc;
}

/* virtual */ SysStatus
FSFileExt2::writeBlockPhys(uval paddr, uval32 length, uval32 offset)
{
    unsigned long pblock = 0;

    // get from ext2 information about which physical block number
    // corresponds to this offset
    LinuxEnv le(SysCall);
    int ret = k42_getPblock(dentry, (unsigned long)offset, &pblock);
    if (ret < 0) {
	return _SERROR(2826, 0, -ret);
    }

    // offset+length should not cross this block boundary.
    // FIXME: not sure if this is a good test
    tassertMsg(offset % Ext2Disk::GetBlkSize() 
	       + length <= DiskClient::BLOCKSIZE,
	       "offset %d, length %d\n", offset, length);

    SysStatus rc = Ext2Disk::BwritePhys(pblock, paddr);
    tassertMsg(_SUCCESS(rc), "rc is 0x%lx\n", rc);

    return rc;
}

/* virtual */ SysStatus
FSFileExt2::readBlockPhys(uval paddr, uval32 offset)
{
    unsigned long pblock;

    // get from ext2 information about which physical block number
    // corresponds to this offset
    LinuxEnv le(SysCall);
    int ret = k42_getPblock(dentry, (unsigned long)offset, &pblock);
    if (ret < 0) {
	return _SERROR(2824, 0, -ret);
    }

    SysStatus rc = Ext2Disk::BreadPhys(pblock, paddr);
    tassertMsg(_SUCCESS(rc), "rc is 0x%lx\n", rc);

    return rc;
}

/* Creates a new file in this directory, file name and mode.  Returns
 * with fileInfo being a pointer to a FSFile object.  Assumes that the
 * required name does NOT exist.  VFS layers in K42 and Linux call
 * createFile() after making sure (through lookup()) that the filename
 * is not used.
 */
/* virtual */ SysStatus
FSFileExt2::createFile(char *name, uval namelen, mode_t mode,
		       FSFile **fileInfo,
		       FileLinux::Stat *stat /* = NULL */)
{
    err_printf("<-- %s: %s\n", __PRETTY_FUNCTION__, name);

    // create null terminated name to pass to linux side
    char buf[PATH_MAX+1];
    memcpy(buf, name, namelen);
    buf[namelen] = '\0';

    LinuxEnv le(SysCall);
    void *newdentry;
    int error = k42_create(mnt, dentry, buf, mode, &newdentry);
    // for now
    tassertMsg(error == 0, "?");
    if (error < 0) {
	return _SERROR(2825, 0, -error);
    }

    // create FSFileExt2 object for the new file
    *fileInfo = new FSFileExt2(tref, mnt, newdentry);
    passertMsg(*fileInfo != NULL, "no mem\n");

    SysStatus rc = 0;
    if (stat) {
	rc = callStat(mnt, newdentry, (void *) stat);
    }

    return rc;
}

/* virtual */ SysStatus
FSFileExt2::utime(const struct utimbuf *utbuf)
{
    err_printf("<-- %s: %p\n", __PRETTY_FUNCTION__, utbuf);

    LinuxEnv le(SysCall);
    return k42_utime(this->dentry, this->mnt, utbuf);
}


/* virtual */ SysStatus
FSFileExt2::fchmod(mode_t mode)
{
    err_printf("<-- %s: %x\n", __PRETTY_FUNCTION__, mode);

    LinuxEnv le(SysCall);
    return k42_fchmod(this->dentry, this->mnt, mode);
}

/* virtual */ SysStatus
FSFileExt2::fchown(uid_t uid, gid_t gid)
{
    err_printf("<-- %s: %i %i\n", __PRETTY_FUNCTION__, uid, gid);

    LinuxEnv le(SysCall);
    return k42_fchown(this->dentry, this->mnt, uid, gid);
}

/* virtual */ SysStatus
FSFileExt2::sync()
{
    err_printf("<-- %s\n", __PRETTY_FUNCTION__);

    LinuxEnv le(SysCall);
    return k42_sync();
}

/* virtual */ SysStatus
FSFileExt2::unlink(char *name, uval namelen,
		   FSFile *fileInfo, uval *nlinkRemain)
{
    err_printf("<-- %s: name = %s, nlinkRemain = %p\n",
	       __PRETTY_FUNCTION__, name, nlinkRemain);

    SysStatus rc = 0;
    void *d;

    char target[PATH_MAX+1];
    memcpy(target, name, namelen);
    target[namelen] = '\0';

    LinuxEnv le(SysCall);
    rc = k42_lookup(this->dentry, (const char*)target, this->mnt, &d);
    if (rc || d == NULL) {
	rc = _SERROR(2823, 0, ENOENT);
	goto out;
    }

    rc = k42_unlink(this->dentry, this->mnt, d, nlinkRemain);

 out:
    err_printf("--> %s: returning %lu\n", __PRETTY_FUNCTION__, rc);
    if (nlinkRemain)
        err_printf("--> %s: links = %lu\n", __PRETTY_FUNCTION__, *nlinkRemain);
    return rc;
}

/* virtual */ SysStatus
FSFileExt2::statfs(struct statfs *buf)
{
    err_printf("<-- %s\n", __PRETTY_FUNCTION__);

    int err = 0;

    LinuxEnv le(SysCall);
    err = k42_statfs(this->dentry, buf);

    err_printf("--> %s: returning %d\n", __PRETTY_FUNCTION__, err);

    return err;
}

/* virtual */ SysStatus
FSFileExt2::mkdir(char *compName, uval compLen, mode_t mode,
		  FSFile **finfo)
{
    char name[PATH_MAX+1];
    memcpy(name, compName, compLen);
    name[compLen] = '\0';

    err_printf("<-- %s: mkdir for %s\n", __PRETTY_FUNCTION__, name);

    LinuxEnv le(SysCall);
    void *newdentry;
    int error = k42_mkdir(dentry, (const char*) name, mode, &newdentry);

    if (error < 0 ) { // this needs to be checked
	return _SERROR(2828, 0, -error);
    }

    *finfo = new FSFileExt2(tref, mnt, newdentry);
    tassertMsg(*finfo != NULL, "new failed?\n");

    return 0;
}

/* virtual */ SysStatus
FSFileExt2::deleteFile()
{
    err_printf("<-- %s\n", __PRETTY_FUNCTION__);

    LinuxEnv le(SysCall);
    return k42_delete_file(this->dentry, this->mnt);
}

/* virtual */ SysStatus
FSFileExt2::link(FSFile *newDirInfo, char *newName, 
		 uval newLen, ServerFileRef fref)
{
    SysStatus rc = 0;

    char name[PATH_MAX + 1];
    memcpy(name, newName, newLen);
    name[newLen] = '\0';

    err_printf("<-- %s: %s\n", __PRETTY_FUNCTION__, name);

    LinuxEnv le(SysCall);

    rc = k42_link(this->dentry, this->mnt, 
		  ((FSFileExt2 *)newDirInfo)->dentry, name);

    err_printf("--> %s: returning %lu\n", __PRETTY_FUNCTION__, rc);
    return rc;
}

/* virtual */ SysStatus
FSFileExt2::symlink(char *compName, uval compLen, char *oldpath)
{
    SysStatus rc = 0;

    char name[PATH_MAX + 1];
    memcpy(name, compName, compLen);
    name[compLen] = '\0';

    err_printf("<-- %s: %s %s\n", __PRETTY_FUNCTION__, name, oldpath);

    LinuxEnv le(SysCall);

    rc = k42_symlink(this->dentry, this->mnt, name, oldpath);

    err_printf("--> %s: returning %lu\n", __PRETTY_FUNCTION__, rc);
    return rc;
}

/* virtual */ SysStatusUval
FSFileExt2::readlink(char *buf, uval bufsize)
{
    SysStatusUval rc = 0;

    LinuxEnv le(SysCall);

    rc = k42_readlink(this->dentry, buf, bufsize);

    return rc;
}

/* We are called on the directory containing the file to be moved, and
 * passed the name of the file to be moved in oldName, the directory to which
 * the file should be moved in newDirInfo, and the name it should be called
 * in newName.
 */
/* virtual */ SysStatus
FSFileExt2::rename(char *oldName, uval oldLen, FSFile *newDirInfo,
		   char *newName, uval newLen, FSFile *renamedFinfo)
{
    SysStatus rc = 0;
    void *newdentry;

    /* Null terminate second path.  */
    char new_name[PATH_MAX + 1];
    memcpy(new_name, newName, newLen);
    new_name[newLen] = '\0';

    err_printf("<-- %s: old = %s new = %s\n",
	       __PRETTY_FUNCTION__, oldName, new_name);

    /* We are calling into linux layer, so set up Linux environment.  */
    LinuxEnv le(SysCall);

    /* Call into the Linux layer to perform rename.  Note that we can
       cast to FSFileExt2 because rename only defined on same mount.  */
    rc = k42_rename(this->dentry, ((FSFileExt2 *)renamedFinfo)->dentry,
		    ((FSFileExt2 *)newDirInfo)->dentry, new_name);

#if 1
    rc = k42_lookup(((FSFileExt2 *)newDirInfo)->dentry, (const char*)new_name,
		    this->mnt, &newdentry);

    if (newdentry == NULL)
      err_printf("--> %s: PANIC: we just lost %s!\n", __FUNCTION__, new_name);
#endif

    err_printf("--> %s: returning %lu\n", __PRETTY_FUNCTION__, rc);

    return rc;
}

/* virtual */ SysStatus
FSFileExt2::rmdir(char *name, uval namelen)
{
    SysStatus rc = 0;
    void *target_dentry;

    /* Null terminate path.  */
    char target[PATH_MAX + 1];
    memcpy(target, name, namelen);
    target[namelen] = '\0';

    err_printf("<-- %s: target = %s\n", __PRETTY_FUNCTION__, target);

    /* We are calling into linux layer, so set up Linux environment.  */
    LinuxEnv le(SysCall);

    /* Get the dentry pointer for the file to be moved.  */
    rc = k42_lookup(dentry, (const char*)target, mnt, &target_dentry);

    /* Bail out if we could not look up the file to be moved.  */
    if (rc || target_dentry == NULL) {
	rc = _SERROR(2823, 0, ENOENT);
	goto out;
    }

    /* Call into the Linux layer to perform rmdir.  */
    rc = k42_rmdir(dentry, target_dentry);

 out:
    return rc;
}
