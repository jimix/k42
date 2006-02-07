/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br).
 *
 * $Id: FSFileKFS.C,v 1.8 2005/04/08 16:29:18 okrieg Exp $
 *****************************************************************************/

#include <kfsIncs.H>

#ifndef KFS_TOOLS
#include <fslib/MultiLinkManager.H>
#endif

#include "FSFileKFS.H"
#include "LSOBasic.H"
#include "LSOBasicFile.H"
#include "LSOBasicDir.H"
#include "LSOBasicSymlink.H"
#include "KFSGlobals.H"

/* virtual */ SysStatus
FSFileKFS::getStatus(FileLinux::Stat *status)
{
    KFSStat stat;

    LSOBasic *lso = (LSOBasic *)baseLSO;
    tassertMsg(baseLSO != NULL, "?");
    lso->getAttribute(&stat);
    stat.copyTo(status);

    return 0;
}

/*
 * fchown()
 *
 *   Changes the ownership of the file referenced by the passed fileInfo
 */
SysStatus
FSFileKFS::fchown(uid_t uid, gid_t gid)
{
    LSOBasic *lso = (LSOBasic *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fchown() IN\n");

    // use the token to retrieve the proper LSO, and then the attributes
    tassertMsg(baseLSO != NULL, "?");

    // set the attributes & flush them to disk
    lso->chown(uid, gid);
    //lso->flush();

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fchown() OUT\n");

    return 0;
}

/*
 * fchmod()
 *
 *   Changes the mode of the file referenced by the passed fileInfo
 */
SysStatus
FSFileKFS::fchmod(mode_t mode)
{
    LSOBasic *lso = (LSOBasic *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fchmod() IN\n");

    // get the "object related state"
    tassertMsg(baseLSO != NULL, "?");

    // update the mode and flush
    lso->chmod(mode);
    //lso->flush();

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fchmod() OUT\n");
    return 0;
}

/*
 * ftruncate()
 *
 *   Truncates the file referenced by the passed fileInfo
 */
SysStatus
FSFileKFS::ftruncate(off_t length)
{
    LSOBasicFile *lso = (LSOBasicFile *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::ftruncate() length %ld IN\n", length);

    // we can't truncate to negative sizes
    if (length < 0) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::ftruncate() returning error\n");
	return _SERROR(2341, 0, EINVAL);
    }

    // call the proper truncate
    tassertMsg(lso != NULL, "?");
    lso->truncate(length);

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::ftruncate() OUT\n");
    return 0;
}

/*
 * deleteFile()
 *
 *  Does the actual data deletion of a file.
 *
 *  IMPORTANT: The caller must delete this FSFileKFS after this to
 *  ensure that all the in-memory objects related to this file are
 *  deleted too
 */
SysStatus
FSFileKFS::deleteFile()
{
    sval rc;
    LSOBasic *lso = (LSOBasic *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::deleteFile() IN this=0x%p\n", this);

    passertMsg(lso != NULL, "FSFileKFS::deleteFile() Corrupted token \n");

    // Really delete file.
    rc = lso->deleteFile();
    if (rc < 0) {
	tassertMsg(0, "look at failure\n");
        return _SERROR(2344, 0, EMLINK);
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::deleteFile() OUT this=0x%p\n", this);
    return 0;
}

/*
 * utime()
 *
 *  Sets the access and modification time of the file referenced by
 *  the given fileInfo.  If the requested utbuf is NULL, then the
 *  times are set to the current time.
 */
SysStatus
FSFileKFS::utime(const struct utimbuf *utbuf)
{
    LSOBasic *lso = (LSOBasic *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::utime_1() IN\n");

    tassertMsg(lso != NULL, "?");

    // set the times and flush the data to disk
    lso->utime(utbuf);
    //lso->flush();

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::utime_1() OUT\n");
    return 0;
}

/*
 * createFile()
 *
 *   Creates a new file in this directory, file name and mode.  Returns a
 *   fileInfo pointer to the file.
 *
 *   Important: The code assumes that the required name does _not_ exist.
 *              VFS layers in K42 and Linux call createFile() after making
 *              sure (through lookup()) that the filename is not used.
 */
SysStatus
FSFileKFS::createFile(char *name, uval namelen,
		      mode_t mode, FSFile **fileInfo,
		      FileLinux::Stat *status /* = NULL */)
{
    SysStatus rc;
    LSOBasicDir *lso = (LSOBasicDir *)baseLSO;
    ObjTokenID newTokID;
    LSOBasic *lsoFile;
    KFSStat stat;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::createFile() name=%s IN\n", name);

    // Check if the entry for this filename already exists
    tassertMsg(baseLSO != NULL, "?");

    // For performance, we remove this check for an already existing
    // 'name', since Linux and K42 VFS layers perform the lookup for us
    /*
      rc = lso->matchDir(name, namelen, &otokID);
      // it doesn't exist, so create the file
      if (_FAILURE(rc)) {
      } else {
      KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::createFile():");
      KFS_DPRINTF(DebugMask::FS_FILE_KFS, "File already exists!.\n");
      *fileInfo = NULL;
      return _SERROR(2349, 0, EEXIST);
      }
    */

    // FIXME: uid == 0?!
    // create an entry for the file

    rc = lso->createRecord(&newTokID, OT_LSO_BASIC);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem creating record\n");
	return rc;
    }

    *fileInfo = createFSFile(globals, &newTokID, lso->getLocalRecordMap());

    lsoFile = (LSOBasic *)((FSFileKFS *)*fileInfo)->baseLSO;
    tassertMsg(lsoFile != NULL, "?");

    rc = lsoFile->initAttribute(S_IFREG | (mode & ~S_IFMT), 0, 0);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem in initAttribute\n");
	lso->deleteRecord(&newTokID);
	return rc;
    }

    rc = lso->createEntry(name, namelen, &newTokID, S_IFREG | (mode & ~S_IFMT), 0);
    _IF_FAILURE_RET(rc);

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::createFile: Created new entry! "
		"name=%s, id=%llu\n", name, newTokID.id);

    // update the status
    if (status) {
	tassertMsg(lsoFile != NULL, "?");
        lsoFile->getAttribute(&stat);
	stat.copyTo(status);
	tassertMsg(S_ISREG(status->st_mode), "should be a file, mode=%o\n",
		   status->st_mode);
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::createFile() name=%s OUT\n", name);
    return 0;
}

/*
 * getDents()
 *
 *  Retrieve directory entries starting at the specified "cookie"
 */
SysStatusUval
FSFileKFS::getDents(uval &cookie, struct direntk42 *buf, uval len)
{
    LSOBasicDir *lso = (LSOBasicDir *)baseLSO;
    tassertMsg(lso != NULL, "?");
    return lso->getDents(cookie, buf, len);
}

/*
 * readBlockPhys()
 *
 *   Reads in the specified location on disk and copies it into the
 *   given address.
 */
SysStatus
FSFileKFS::readBlockPhys(uval paddr, uval32 offset)
{
    tassertMsg(paddr != 0, "ops");

    // Calculate block number from offset.
    uval32 block = offset / OS_BLOCK_SIZE;
    LSOBasic *lso = (LSOBasic *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS_READ_BLOCK,
		"FSFileKFS::readBlockPhys() IN\n");

    tassertMsg(((block * OS_BLOCK_SIZE) == offset),
	       "FSFileKFS::readBlockPhys(): non block aligned offset\n");

    tassertMsg(lso != NULL, "?");

    sval ret = lso->readBlock(block, (char *)paddr, PSO_EXTERN,
			      UsesPhysAddr);
    tassertMsg(ret >= 0, "?");

    //lso->flush();

    KFS_DPRINTF(DebugMask::FS_FILE_KFS_READ_BLOCK,
		"FSFileKFS::readBlockPhys() OUT\n");
    return (SysStatus) ret;
}

/*
 * writeBlockPhys()
 *
 *   Writes the specified location on disk and with the data at the
 *   given address.
 *
 *  note: length should not cause offset+length to cross a block boundary.
 */
SysStatus
FSFileKFS::writeBlockPhys(uval paddr, uval32 length, uval32 offset)
{
    // Calculate block number from offset.
    uval32 block = offset / OS_BLOCK_SIZE;
    LSOBasic *lso = (LSOBasic *)baseLSO;
    sval rc = 0;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS_WRITE_BLOCK,
		"FSFileKFS::writeBlockPhys() IN this=0x%p\n", this);

    tassert(((block * OS_BLOCK_SIZE) == offset),
	    err_printf("FSFileKFS::writeBlockPhys(): \
                        non block aligned offset\n"));

    tassertMsg(lso != NULL, "?");
    rc = lso->writeBlock(offset + length, block, (char *)paddr, PSO_EXTERN);
    if (_FAILURE(rc)) {
	return rc;
    }
    //lso->flush();

    KFS_DPRINTF(DebugMask::FS_FILE_KFS_WRITE_BLOCK,
		"FSFileKFS::writeBlockPhys() OUT this=0x%p\n", this);
    return rc;
}

SysStatus
FSFileKFS::reValidateFSFile(FileLinux::Stat *status)
{
    KFSStat stat;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::reValidateToken_3() IN\n");

    // locate the correct LSO and call its specific getAttribute()
    if (status) {
	tassertMsg(baseLSO != NULL, "?");
        ((LSOBasic *)baseLSO)->getAttribute(&stat);
	stat.copyTo(status);
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::reValidateToken_3() OUT\n");
    return 0;
}

/*
 * link()
 *
 *   Hard links the given old file to the new name specified.
 *
 * note: Only files can be hard linked. Directories can not.
 */
SysStatus
FSFileKFS::link(FSFile *newDirInfo, char *newname,  uval newlen,
		ServerFileRef fref)
{
    SysStatus rc;
    ObjTokenID otokID, oldTokID;
    LSOBasicDir *lso = (LSOBasicDir *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::link_1() IN\n");

    // Check if hard link needs to be created.
    tassertMsg(lso != NULL, "?");

    rc = lso->matchDir(newname, newlen, &otokID);

    // if there is no such filename, create the hardlink
    if (rc < 0) {
	tassertMsg((fref != NULL), "woops, higher level must pass\n");
	oldTokID = baseLSO->getID();

	// get number of links in oldtok, if==1 put in multilink
	LSOBasic *oldFileLSO;

        // FIXME: set the uid, mode, etc.
	rc = lso->createEntry(newname, newlen, &oldTokID, S_IFREG, 0);
	if (_FAILURE(rc)) {
	    return rc;
	}

	oldFileLSO = (LSOBasic *)lso->getLocalRecordMap()->getObj(&oldTokID);
	tassertMsg(oldFileLSO != NULL, "?");
	if (oldFileLSO->getNumLinks() == 1) {
	    MultiLinkManager::SFHolder *href =
		MultiLinkManager::AllocHolder(fref);
	    // now keep track of this
	    globals->multiLinkMgr.add((uval)this, href);
	}
    } else {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::link_1() File already exists!\n");
        return _SERROR(2342, 0, EEXIST);
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::link_1() OUT\n");
    return 0;
}

/*
 * unlink()
 *
 *   Only removes entry from directory and not actual file.  This
 *   method can only be used for unlinking files, not directories.
 */
SysStatus
FSFileKFS::unlink(char *pathName, uval pathLen, FSFile *finfo /* = NULL */,
		  uval *nlinkRemain /* = NULL */)
{
    sval rc;
    LSOBasicDir *lso = (LSOBasicDir *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::unlink() IN\n");

    // Delete entry from directory and not actual file.
    // Note: this also decreases the link count of the file
    tassertMsg(lso != NULL, "?");

    rc = lso->deleteEntry(pathName, pathLen);
    if (rc < 0) {
	return _SERROR(2343, 0, ENOENT);
    }

    // calculate the link count
    if (nlinkRemain) {
	LSOBasic *lsoFile;
	KFSStat stat;
	ObjTokenID ftokID;

	ftokID = ((FSFileKFS *)finfo)->baseLSO->getID();

	lsoFile = (LSOBasic *)lso->getLocalRecordMap()->getObj(&ftokID);
	tassertMsg(lsoFile != NULL, "?");
        lsoFile->getAttribute(&stat);
        *nlinkRemain = stat.st_nlink;
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::unlink() OUT\n");
    return 0;
}

/*
 * rename()
 *
 */
SysStatus
FSFileKFS::rename(char *oldName, uval oldLen,
		  FSFile *newDirInfo, char *newName, uval newLen,
		  FSFile *renamedFinfo)
{
    sval rc;
    ObjTokenID oldTokID, otokID;
    ObjTokenID oldtokID = baseLSO->getID(),
	newTokID = ((FSFileKFS *)newDirInfo)->baseLSO->getID();
    LSOBasicDir *lsoOld = (LSOBasicDir *)baseLSO, *lsoNew;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::rename(): IN oldName=%s, newName=%s\n", oldName,
		newName);

    // Check that old entry is valid.
    tassertMsg(lsoOld != NULL, "?");
    rc = lsoOld->matchDir(oldName, oldLen, &oldTokID);
    if (rc < 0) {
	return _SERROR(2345, 0, ENOENT);
    }

    KFSStat stat;
    ObjTokenID renamedTokID = ((FSFileKFS *)renamedFinfo)->baseLSO->getID();
    LSOBasic *lsoFile = (LSOBasic *)lsoOld->getLocalRecordMap()->getObj(&renamedTokID);
    tassertMsg(lsoFile != NULL, "?");
    lsoFile->getAttribute(&stat);

    // FIXME: we need to check if the new entry is a directory; if so, it
    // has to be empty

    // Create new link.
    lsoNew = ((FSFileKFS *)newDirInfo)->dirLSO;
    tassertMsg(lsoNew != NULL, "?");
    rc = lsoNew->matchDir(newName, newLen, &otokID);
    if (rc < 0) {
        rc = lsoNew->createEntry(newName, newLen, &renamedTokID, stat.st_mode,
				 stat.st_uid);
	if (_FAILURE(rc)) {
	    return rc;
	}

	// Adding directories should increase parent's link count
	if (S_ISDIR(stat.st_mode)) {
	    lsoNew->link();
	}
    } else {
        lsoNew->updateEntry(newName, newLen, &renamedTokID);
    }
    //lsoNew->flush();

    // If removed entry was a directory, decrease parent't link count
    if (S_ISDIR(stat.st_mode)) {
	lsoOld->unlink();
    }
    // Delete old link
    rc = unlink(oldName, oldLen, renamedFinfo);

    if (_FAILURE(rc)) {
	return rc;
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::rename(): OUT\n");
    return 0;
}

/*
 * mkdir()
 *
 *   Creates a new directory of the given name
 */
SysStatus
FSFileKFS::mkdir(char *compName, uval compLen, mode_t mode, FSFile **newDir)
{
    sval rc;
    ObjTokenID otokID, newTokID;
    LSOBasicDir *lso = (LSOBasicDir *)baseLSO, *lsoDir;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::mkdir() IN\n");

    // Check for duplicate directory name.
    tassertMsg(lso != NULL, "?");
    rc = lso->matchDir(compName, compLen, &otokID);
    if (rc == 0) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::mkdir():");
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "Directory name already exists. Aborting.\n");
	return _SERROR(2346, 0, EEXIST);
    }

    // Create directory.
    // note: updates all the appropriate names, and flushes the new
    //       directory to disk
    // FIXME: set the uid, gid, etc.

#ifdef KFS_USE_GLOBAL_RECORDMAP
    rc = lso->createRecord(&newTokID, OT_LSO_BASIC_DIR);
#else
    rc = lso->createRecord(&newTokID, OT_LSO_DIR_EMB);
#endif

    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem creating record\n");
    }

    *newDir = createFSFile(globals, &newTokID, lso->getLocalRecordMap());

    lsoDir = (LSOBasicDir *)((FSFileKFS *)*newDir)->baseLSO;
    tassertMsg(lsoDir != NULL, "?");

    rc = lsoDir->initAttribute(S_IFDIR | (mode & ~S_IFMT), 0, 0);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::mkdir() problem in initAttribute\n");
	lso->deleteRecord(&newTokID);
	return rc;
    }

    rc = lso->createDir(compName, compLen, S_IFDIR | (mode & ~S_IFMT), 0,
			&newTokID);
    if (_FAILURE(rc)) {
        KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::mkdir():");
        KFS_DPRINTF(DebugMask::FS_FILE_KFS, "Couldn't create entry\n");
	return rc;
    }

    // flush the parent directory
    //lso->flush();

    // set the new directory file token if requested
    tassertMsg(newDir, "woops, not asked for\n");

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::mkdir() OUT\n");
    return 0;
}

/*
 * rmdir()
 *
 *   Removes the name of a directory entry, does not actually free the data.
 */
SysStatus
FSFileKFS::rmdir(char *name, uval namelen)
{
    sval rc;
    LSOBasicDir *lso = (LSOBasicDir *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::rmdir() IN\n");

    tassertMsg(lso != NULL, "?");

    // delete the directory entry for the given directory
    rc = lso->deleteDir(name, namelen);
    _IF_FAILURE_RET(rc);

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::rmdir() OUT\n");
    return 0;
}

/*
 * fsync()
 *
 *  Makes sure all the data and meta-data go to disk *now*
 */
SysStatus
FSFileKFS::fsync()
{
    LSOBasic *lso = (LSOBasic *)baseLSO;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fsync() IN\n");

    if (lso) {
	lso->flush();
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fsync() OUT\n");

    return 0;
}

/*
 * symlink()
 *
 *   Creates a new symlink in this directory, with 'linkValue' inside it.
 *
 *   Important: The code assumes that the required name does _not_ exist.
 *              VFS layers in K42 and Linux call symlink() after making
 *              sure (through lookup()) that the filename is not used.
 */
SysStatus
FSFileKFS::symlink(char *linkName, uval linkLen, char *linkValue)
{
    SysStatus rc;
    ObjTokenID newTokID;
    LSOBasicDir *lso = (LSOBasicDir *)baseLSO;
    LSOBasicSymlink *lsoSymlink;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::symlink() name=%s value=%s IN\n",
		linkName, linkValue);

    // Check if the entry for this filename already exists
    tassertMsg(lso != NULL, "?");

    // For performance, we remove this check for an already existing
    // 'name', since Linux and K42 VFS layers perform the lookup for us
    /*
      rc = lso->matchDir(name, namelen, &otokID);
      // it doesn't exist, so create the file
      if (_FAILURE(rc)) {
      } else {
      KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::createFile():");
      KFS_DPRINTF(DebugMask::FS_FILE_KFS, "File already exists!.\n");
      *fileInfo = NULL;
      return _SERROR(2349, 0, EEXIST);
      }
    */

    // FIXME: uid == 0?!
    // create an entry for the file
    rc = lso->createRecord(&newTokID, OT_LSO_BASIC_LNK);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem creating record\n");
    }

    lsoSymlink = (LSOBasicSymlink *)lso->getLocalRecordMap()->getObj(&newTokID);
    tassertMsg(lsoSymlink != NULL, "?");

    rc = lsoSymlink->initAttribute(linkValue, S_IFLNK | (0777 & ~S_IFMT),
				   0, 0);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::symlink() problem in initAttribute\n");
	lso->deleteRecord(&newTokID);
	return rc;
    }

    rc = lso->createEntry(linkName, linkLen, &newTokID, S_IFLNK, 0);
    if (_FAILURE(rc)) {
	return rc;
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::symlink() name=%s OUT\n", linkName);
    return 0;
}

/*
 * symlink()
 *
 *   Creates a new symlink in this directory, with 'linkValue' inside it.
 *
 *   Important: The code assumes that the required name does _not_ exist.
 *              VFS layers in K42 and Linux call symlink() after making
 *              sure (through lookup()) that the filename is not used.
 */
SysStatus
FSFileKFS::symlink(char *linkName, uval linkLen, char *linkValue,
		   FSFile **newSymlink)
{
    SysStatus rc;
    ObjTokenID newTokID;
    LSOBasicDir *lso = (LSOBasicDir *)baseLSO;
    LSOBasicSymlink *lsoSymlink;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::symlink() name=%s value=%s IN\n",
		linkName, linkValue);

    // Check if the entry for this filename already exists
    tassertMsg(lso != NULL, "?");

    // For performance, we remove this check for an already existing
    // 'name', since Linux and K42 VFS layers perform the lookup for us
    /*
      rc = lso->matchDir(name, namelen, &otokID);
      // it doesn't exist, so create the file
      if (_FAILURE(rc)) {
      } else {
      KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::createFile():");
      KFS_DPRINTF(DebugMask::FS_FILE_KFS, "File already exists!.\n");
      *fileInfo = NULL;
      return _SERROR(2349, 0, EEXIST);
      }
    */

    // FIXME: uid == 0?!
    // create an entry for the file
    rc = lso->createRecord(&newTokID, OT_LSO_BASIC_LNK);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem creating record\n");
    }

    *newSymlink = createFSFile(globals, &newTokID, baseLSO->getRecordMap());

    lsoSymlink = (LSOBasicSymlink *)((FSFileKFS *)*newSymlink)->baseLSO;
    tassertMsg(lsoSymlink != NULL, "?");

    rc = lsoSymlink->initAttribute(linkValue, S_IFLNK | (0777 & ~S_IFMT), 0, 0);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::symlink() problem in initAttribute\n");
	lso->deleteRecord(&newTokID);
	return rc;
    }

    rc = lso->createEntry(linkName, linkLen, &newTokID,
			  S_IFLNK | (0777 & ~S_IFMT), 0);
    if (_FAILURE(rc)) {
	return rc;
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::symlink() name=%s OUT\n", linkName);
    return 0;
}

/*
 * readlink()
 *
 */
SysStatusUval
FSFileKFS::readlink(char *buffer, uval buflen)
{
    LSOBasicSymlink *lso = (LSOBasicSymlink *)baseLSO;
    tassertMsg(lso != NULL, "?");

    return lso->readlink(buffer, buflen);
}

/* virtual */ SysStatus
FSFileKFS::sync()
{
    SysStatus rc;

    // flush inodes
    rc = globals->recordMap->sync();
    _IF_FAILURE_RET(rc);

    // flush the RecordMap
    globals->recordMap->flush();

    // this takes care of super block and BlockCache
    globals->super->sync();

    return 0;
}
