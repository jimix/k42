/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br).
 *
 * $Id: FSFileKFS.C,v 1.53 2005/08/01 18:09:26 dilma Exp $
 *****************************************************************************/

#include <kfsIncs.H>

#include "FSFileKFS.H"
#include "LSOBasic.H"
#include "LSOBasicFile.H"
#include "LSOBasicDir.H"
#include "LSOBasicSymlink.H"
#include "KFSGlobals.H"

/* virtual */ SysStatus
FSFileKFS::getStatus(FileLinux::Stat *status)
{
    LSOBasic *lso;
    KFSStat stat;

    lso = (LSOBasic *)token->getObj(this);
    tassertMsg(lso != NULL, "?");
    lso->getAttribute(&stat);
    stat.copyTo(status);

    return 0;
}

/*
 * createRecord()
 *
 *   Auxiliary method for file creation. This creates a new record of
 *   type 'type' in the RecordMap and stores the ObjToken in 'newTok'
 */
SysStatus
FSFileKFS::createRecord(ObjToken **newTok, PsoType type)
{
    ObjTokenID newTokID;
    SysStatus rc;

    *newTok = new ObjToken(globals);

    rc = globals->recordMap->allocRecord(type, &newTokID);
    if (_FAILURE(rc)) {
	tassertMsg(0, "FSFileKFS::createRecord() problem with record\n");
	return rc;
    }
    (*newTok)->setID(newTokID);

    if ((*newTok)->gobj()) {
	passertMsg(0, "createRecord() this=0x%p, newTok=0x%p, "
		   "obj=0x%p\n", this, *newTok, (*newTok)->gobj());
    }

    return rc;
}

/*
 * fchown()
 *
 *   Changes the ownership of the file referenced by the passed fileInfo
 */
SysStatus
FSFileKFS::fchown(uid_t uid, gid_t gid)
{
    LSOBasic *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fchown() IN\n");

    // use the token to retrieve the proper LSO, and then the attributes
    lso = (LSOBasic *)token->getObj(this);
    tassertMsg(lso != NULL, "?");

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
    LSOBasic *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fchmod() IN\n");

    // get the "object related state"
    lso = (LSOBasic *)token->getObj(this);
    tassertMsg(lso != NULL, "?");

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
    LSOBasicFile *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::ftruncate() length %ld IN\n", length);

    // we can't truncate to negative sizes
    if (length < 0) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::ftruncate() returning error\n");
	return _SERROR(2341, 0, EINVAL);
    }

    // get the "object related state"
    lso = (LSOBasicFile *)token->getObj(this);
    tassertMsg(lso != NULL, "?");

    // call the proper truncate
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
    LSOBasic *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::deleteFile() IN this=0x%p\n", this);

    lso = (LSOBasic *)token->getObj(this);
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
    LSOBasic *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::utime_1() IN\n");

    lso = (LSOBasic *)token->getObj(this);
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
 *   mode specififes permissions and file type.
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
    ObjToken *newTok = NULL;
    LSOBasicDir *lso;
    LSOBasic *lsoFile;
    KFSStat stat;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::createFile() name=%s IN\n", name);

    passertMsg(!S_ISSOCK(mode), "Not implemented yet, coming soon\n");
    tassertMsg(S_ISREG(mode) || S_ISFIFO(mode), "mode=%o\n", mode);

    // Check if the entry for this filename already exists
    lso = (LSOBasicDir *)token->getObj(this);
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

    rc = createRecord(&newTok, OT_LSO_BASIC);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem creating record\n");
    }

    lsoFile = (LSOBasic *)newTok->getObj(this);
    tassertMsg(lsoFile != NULL, "?");

    rc = lsoFile->initAttribute(mode, 0, 0);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem in initAttribute\n");
	ObjTokenID otokid = newTok->getID();
	globals->recordMap->freeRecord(&otokid);
	return rc;
    }

    rc = lso->createEntry(name, namelen, newTok, mode, 0/*uid*/);
    if (_FAILURE(rc)) {
	delete newTok;
	return rc;
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::createFile: Created new entry! "
		"name=%s, id=%u\n", name, newTok->getID().id);

    *fileInfo = createFSFile(globals, newTok);

    // update the status
    if (status) {
	tassertMsg(lsoFile != NULL, "?");
        lsoFile->getAttribute(&stat);
	stat.copyTo(status);
	tassertMsg(S_ISREG(status->st_mode) || S_ISFIFO(status->st_mode),
		   "should be a file or pipe, mode=%o\n", status->st_mode);
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
    LSOBasicDir *lso = (LSOBasicDir *)token->getObj(this);
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
FSFileKFS::readBlockPhys(uval paddr, uval32 offset,
			 ServerFileRef fileRef /* = NULL */)
{
    tassertMsg(paddr != 0, "ops");

    // Calculate block number from offset.
    uval32 block = offset / OS_BLOCK_SIZE;
    LSOBasic *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS_READ_BLOCK,
		"FSFileKFS::readBlockPhys() IN\n");

    tassertMsg(((block * OS_BLOCK_SIZE) == offset),
	       "FSFileKFS::readBlockPhys(): non block aligned offset\n");

    lso = (LSOBasic *)token->getObj(this);
    tassertMsg(lso != NULL, "?");

    PSOBase::AsyncOpInfo *continuation 
	    = new PSOBase::AsyncOpInfo(fileRef, offset, paddr);

    //err_printf("FSFileKFS::readBlock addr 0x%lx\n", paddr);

    sval ret = lso->readBlock(block, (char *)paddr, continuation,
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
FSFileKFS::writeBlockPhys(uval paddr, uval32 length, uval32 offset,
			  ServerFileRef fileRef /* = NULL*/)
{
    // Calculate block number from offset.
    uval32 block = offset / OS_BLOCK_SIZE;
    LSOBasic *lso;
    sval rc = 0;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS_WRITE_BLOCK,
		"FSFileKFS::writeBlockPhys() IN this=0x%p\n", this);

    tassert(((block * OS_BLOCK_SIZE) == offset),
	    err_printf("FSFileKFS::writeBlockPhys(): \
                        non block aligned offset\n"));

    lso = (LSOBasic *)token->getObj(this);
    tassertMsg(lso != NULL, "?");
    PSOBase::AsyncOpInfo *continuation 
	    = new PSOBase::AsyncOpInfo(fileRef, offset, paddr);
    rc = lso->writeBlock(offset + length, block, (char *)paddr,
			 continuation);
    if (_FAILURE(rc)) {
	return rc;
    }
    //lso->flush();

    KFS_DPRINTF(DebugMask::FS_FILE_KFS_WRITE_BLOCK,
		"FSFileKFS::writeBlockPhys() OUT this=0x%p\n", this);
    return rc;
}

SysStatus
FSFileKFS::reValidateToken(ObjToken *fileInfo, FileLinux::Stat *status)
{
    ObjToken *otok = (ObjToken *)fileInfo;
    LSOBasic *lso;
    KFSStat stat;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::reValidateToken_3() IN fileInfo %p\n", fileInfo);

    // locate the correct LSO and call its specific getAttribute()
    if (status) {
        lso = (LSOBasic *)otok->getObj(this);
	tassertMsg(lso != NULL, "?");
        lso->getAttribute(&stat);
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
    ObjToken *oldtok = token;		// operation called on file
    ObjToken *dirtok = ((FSFileKFS *)newDirInfo)->token;
    ObjTokenID otokID, oldTokID;
    LSOBasicDir *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::link_1() IN\n");

    // Check if hard link needs to be created.
    lso = (LSOBasicDir *)dirtok->getObj(this);
    tassertMsg(lso != NULL, "?");

    rc = lso->matchDir(newname, newlen, &otokID);

    // if there is no such filename, create the hardlink
    if (rc < 0) {
	tassertMsg((fref != NULL), "woops, higher level must pass\n");
        oldTokID = oldtok->getID();

	// get number of links in oldtok, if==1 put in multilink
	LSOBasic *oldFileLSO;

        // FIXME: set the uid, mode, etc.
	rc = lso->createEntry(newname, newlen, oldtok, S_IFREG, 0);
	if (_FAILURE(rc)) {
	    return rc;
	}

	oldFileLSO = (LSOBasic *)oldtok->getObj(NULL);
	tassertMsg(oldFileLSO != NULL, "?");
	if (oldFileLSO->getNumLinks() == 1) {
	    MultiLinkManager::SFHolder *href =
		MultiLinkManager::AllocHolder(fref);
	    // now keep track of this
	    globals->multiLinkMgr.add((uval)token, href);
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
    LSOBasicDir *lso;
    ObjToken *ftok;

    if (finfo == NULL) {
	ftok = NULL;
    } else {
	ftok = ((FSFileKFS *)finfo)->token;
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::unlink() IN\n");

    // Delete entry from directory and not actual file.
    // Note: this also decreases the link count of the file
    lso = (LSOBasicDir *)token->getObj(this);
    tassertMsg(lso != NULL, "?");

    rc = lso->deleteEntry(pathName, pathLen, ftok);
    if (rc < 0) {
	return _SERROR(2343, 0, ENOENT);
    }

    // calculate the link count
    if (nlinkRemain) {
	LSOBasic *lsoFile;
	KFSStat stat;

        lsoFile = (LSOBasic *)ftok->getObj(NULL);
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
    ObjToken *oldtok = token;		// called on old dir
    ObjToken *newtok = ((FSFileKFS *)newDirInfo)->token;
    LSOBasicDir *lsoOld, *lsoNew;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::rename(): IN oldName=%s, newName=%s\n", oldName,
		newName);

    // Check that old entry is valid.
    lsoOld = (LSOBasicDir *)oldtok->getObj(this);
    tassertMsg(lsoOld != NULL, "?");
    rc = lsoOld->matchDir(oldName, oldLen, &oldTokID);
    if (rc < 0) {
	return _SERROR(2345, 0, ENOENT);
    }

    KFSStat stat;
    LSOBasic *lsoFile = (LSOBasic *)((FSFileKFS *)renamedFinfo)->token->getObj(this);
    tassertMsg(lsoFile != NULL, "?");
    lsoFile->getAttribute(&stat);

    // FIXME: we need to check if the new entry is a directory; if so, it
    // has to be empty

    // Create new link.
    lsoNew = (LSOBasicDir *)newtok->getObj(this);
    tassertMsg(lsoNew != NULL, "?");
    rc = lsoNew->matchDir(newName, newLen, &otokID);
    if (rc < 0) {
        rc = lsoNew->createEntry(newName, newLen,
				 ((FSFileKFS *)renamedFinfo)->token,
				 stat.st_mode, stat.st_uid);
	if (_FAILURE(rc)) {
	    return rc;
	}

	// Adding directories should increase parent's link count
	if (S_ISDIR(stat.st_mode)) {
	    lsoNew->link();
	}
    } else {
        lsoNew->updateEntry(newName, newLen,
			    ((FSFileKFS *)renamedFinfo)->token);
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
    ObjTokenID otokID;
    ObjToken *newTok;
    LSOBasicDir *lso, *lsoDir;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::mkdir() IN\n");

    // Check for duplicate directory name.
    lso = (LSOBasicDir *)token->getObj(this);
    tassertMsg(lso != NULL, "?");
    rc = lso->matchDir(compName, compLen, &otokID);
    if (rc == 0) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::mkdir():");
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "Directory name already exists. Aborting.\n");
	delete newTok;
	return _SERROR(2346, 0, EEXIST);
    }

    // Create directory.
    // note: updates all the appropriate names, and flushes the new
    //       directory to disk
    // FIXME: set the uid, gid, etc.

    rc = createRecord(&newTok, OT_LSO_BASIC_DIR);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem creating record\n");
    }

    lsoDir = (LSOBasicDir *)newTok->getObj(this);
    tassertMsg(lsoDir != NULL, "?");

    rc = lsoDir->initAttribute(S_IFDIR | (mode & ~S_IFMT), 0, 0);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::mkdir() problem in initAttribute\n");
	ObjTokenID otokid = newTok->getID();
	globals->recordMap->freeRecord(&otokid);
	return rc;
    }

    rc = lso->createDir(compName, compLen, S_IFDIR | (mode & ~S_IFMT), 0,
			newTok);
    if (_FAILURE(rc)) {
        KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::mkdir():");
        KFS_DPRINTF(DebugMask::FS_FILE_KFS, "Couldn't create entry\n");
	delete newTok;
	return rc;
    }

    // flush the parent directory
    //lso->flush();

    // set the new directory file token if requested
    tassertMsg(newDir, "woops, not asked for\n");

    *newDir = new FSFileKFS(globals, newTok);

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
    LSOBasicDir *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::rmdir() IN\n");

    lso = (LSOBasicDir *)token->getObj(this);
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
    LSOBasic *lso;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FSFileKFS::fsync() IN\n");

    lso = (LSOBasic *)token->gobj();
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
    ObjToken *newTok;
    LSOBasicDir *lso;
    LSOBasicSymlink *lsoSymlink;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::symlink() name=%s value=%s IN\n",
		linkName, linkValue);

    // Check if the entry for this filename already exists
    lso = (LSOBasicDir *)token->getObj(this);
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
    rc = createRecord(&newTok, OT_LSO_BASIC_LNK);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem creating record\n");
    }

    lsoSymlink = (LSOBasicSymlink *)newTok->getObj(NULL);
    tassertMsg(lsoSymlink != NULL, "?");

    rc = lsoSymlink->initAttribute(linkValue, S_IFLNK | (0777 & ~S_IFMT),
				   0, 0);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::symlink() problem in initAttribute\n");
	ObjTokenID otokid = newTok->getID();
	globals->recordMap->freeRecord(&otokid);
	return rc;
    }

    rc = lso->createEntry(linkName, linkLen, newTok, S_IFLNK, 0);
    if (_FAILURE(rc)) {
	delete newTok;
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
    ObjToken *newTok;
    LSOBasicDir *lso;
    LSOBasicSymlink *lsoSymlink;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"FSFileKFS::symlink() name=%s value=%s IN\n",
		linkName, linkValue);

    // Check if the entry for this filename already exists
    lso = (LSOBasicDir *)token->getObj(this);
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
    rc = createRecord(&newTok, OT_LSO_BASIC_LNK);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::createFile() problem creating record\n");
    }

    lsoSymlink = (LSOBasicSymlink *)newTok->getObj(this);
    tassertMsg(lsoSymlink != NULL, "?");

    rc = lsoSymlink->initAttribute(linkValue, S_IFLNK | (0777 & ~S_IFMT), 0, 0);
    if (_FAILURE(rc)) {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::symlink() problem in initAttribute\n");
	ObjTokenID otokid = newTok->getID();
	globals->recordMap->freeRecord(&otokid);
	return rc;
    }

    rc = lso->createEntry(linkName, linkLen, newTok,
			  S_IFLNK | (0777 & ~S_IFMT), 0);
    if (_FAILURE(rc)) {
	delete newTok;
	return rc;
    }

    *newSymlink = createFSFile(globals, newTok);

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
    LSOBasicSymlink *lso = (LSOBasicSymlink *)token->getObj(this);
    tassertMsg(lso != NULL, "?");

    return lso->readlink(buffer, buflen);
}

/* virtual */ SysStatus
FSFileKFS::sync()
{
    globals->super->sync();

    return 0;
}
