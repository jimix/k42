/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFS.C,v 1.7 2004/05/06 19:52:50 lbsoares Exp $
 *****************************************************************************/

#include "fs.H"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ServerObject.H"
#include "Disk.H"
#include "FileDisk.H"
#include "SuperBlock.H"
#include "LSOBasic.H"
#include "PSOTypes.H"
#include "LSOBasicDir.H"
#include "PSOBasicRW.H"
#include "RecordMap.H"
#include "PSODiskBlock.H"
#include "PSOPreallocExtent.H"
#include "PSOSmall.H"
#include "PSOSmallMeta.H"
#include "KFSGlobals.H"
#include "BlockCacheTools.H"
#include "KFSFactory.H"

#if defined (KFS_TOOLS) && ! defined(PLATFORM_Darwin)
#include <malloc.h>
#endif

void KFSfree(void *ptr) { free(ptr); }
void* KFSalloc(uval size)
{
    void *ptr = malloc(size);
    passertMsg(ptr != NULL, "no more mem\n");
    return ptr;
}


static SuperBlock *sb = NULL;
static KFSGlobals *g =  NULL;

void
initFS(Disk *disk, uval format=0)
{
    if (sb != NULL) {
	printf("initFS has already been called\n");
	return;
    }

    tassertMsg(g == NULL, "how come?");
    g = new KFSGlobals();

    sb = initFS(disk, g, format);
}

/*
 * initFS()
 *
 *   Allocate the necessary in-memory items to begin KFS operation
 */
SuperBlock *
initFS(Disk *disk, KFSGlobals *g, uval format /* =0 */)
{
    SuperBlock *spb;
    sval rc;

    // create a new BlockCache for KFS
    g->blkCache = new BlockCacheTools(disk);

    g->llpso = new PSODiskBlock(g, disk);

    KFSFactory factory;
    rc = factory.allocSuperBlock(spb, g, 0);
    tassertMsg(_SUCCESS(rc), "factory.allocSuperBlock problem? rc 0x%lx\n",
	       rc);

    // initialize the superblock
    spb->init();
    if (!format) {
	rc = spb->checkVersion();
	// FIXME: when things get more stable, return error instead of panicing
	passertMsg(_SUCCESS(rc), "checkVersion() failed\n");
    }

    // set the superblock
    g->super = spb;

    // fake out the KFS code
    g->disk_ar = disk;

    // register all the base ServerObject types
    rc = factory.registerServerObjectTypes(g);
    tassertMsg(_SUCCESS(rc), "factory.registerServerObjectTypes problem? "
	       "rc 0x%lx\n", rc);

    // If we're formatting, forget about trying to initialize the RecordMap
    if (!format) {
	// Get record oriented PSO for storing obj data
	rc = factory.allocRecordMap(g->recordMap, g->super->getRecMap(), g);
	tassertMsg(_SUCCESS(rc), "factory.allocRecordMap problem? rc "
		   "0x%lx\n", rc);
    }

    return spb;
}

/*
 * formatKFS()
 *
 *   Creates a fresh superblock on the disk.  Also initializes the
 *   RecordMap entries and the block allocation bitmap.  Then it creates
 *   the root directory with entries . & ..
 */
SysStatus
formatKFS(Disk *disk)
{
    SysStatus rc;

    // initialize the memory structures for KFS
    if (sb == NULL)  initFS(disk, 1);

    // format the superblock, and bitmaps
    rc = sb->format("disk");
    passertMsg(_SUCCESS(rc), "superblock format() failed with rc 0x%lx\n", rc);

    // Get record oriented PSO for storing obj data
    KFSFactory factory;
    rc = factory.allocRecordMap(g->recordMap, g->super->getRecMap(), g);
    tassertMsg(_SUCCESS(rc), "factory.allocRecordMap problem? rc 0x%lx\n",
	       rc);

    // create root directory
    sb->createRootDirectory();

    // sync the disk metadata
    g->recordMap->flush();
    sb->sync();

    return 0;
}

/*
 * parseName
 *
 * Given a path (representing a file or directory 'ent'), it returns
 * the LSOBasicDir for the directory containing 'ent', and the string
 * representing the name for 'ent'.
 * If argument createDir is set, intermmediate directories in the path
 * are created as needed.
 */

SysStatus
parseName(uval createDir, SuperBlock *sb, KFSGlobals *g, char *path,
	  LSOBasicDir* &lsoDir, char* &file)
{
    LSOBasicDir *root;
    char *subPath;
    SysStatus rc;
    ObjTokenID otokID, rootTokID = sb->getRootLSO();

    // locate '/'
    //    rootTok.setID(sb->getRootLSO());
    //    root = (LSOBasicDir *)rootTok.getObj(NULL);
    root = (LSOBasicDir *)g->recordMap->getObj(&rootTokID);
    tassertMsg(root != NULL, "?");

    // first chop off the filename
    file = path + strlen(path);
    while (*file != '/') file--;
    *file = 0;
    file++;

    // now traverse the directory path creating directories if necessary
    subPath = strtok(path, "/");
    lsoDir = root;
    while (subPath) {
        rc = lsoDir->matchDir(subPath, strlen(subPath), &otokID);
        if (rc < 0) { // no such directory
	    if (createDir) {
		// doesn't exist, so create the new directory
		LSOBasicDir *lsoNewDir;

#ifdef KFS_USE_GLOBAL_RECORDMAP
		rc = lsoDir->createRecord(&otokID, OT_LSO_BASIC_DIR);
#else
		rc = lsoDir->createRecord(&otokID, OT_LSO_DIR_EMB);
#endif
		_IF_FAILURE_RET(rc);

		lsoNewDir = (LSOBasicDir *)lsoDir->getLocalRecordMap()->getObj(&otokID);
		// FIXME: Pass proper uid, gid
		lsoNewDir->initAttribute(S_IFDIR | (0755 & ~S_IFMT), 0, 0);

		lsoDir->createDir(subPath, strlen(subPath), 0755, 0, &otokID);
		lsoDir->flush();
	    } else {
		return _SERROR(2610, 0, ENOENT);
	    }
        }

        subPath = strtok(NULL, "/");
        lsoDir = (LSOBasicDir *)lsoDir->getLocalRecordMap()->getObj(&otokID);
	tassertMsg(lsoDir != NULL, "?");
    }

    tassertMsg(lsoDir != NULL, "?");

    return 0;
}

/*
 * createFileKFS()
 *
 *   Creates a new file on the disk, creating any requested directory
 *   structure along the way.  If the file already exists, then it is
 *   overwritten.
 */
SysStatus
createFileKFS(char *prog, Disk *disk, int fd, char *newPath,
              uval mode, uval uid, uval gid)
{
    LSOBasicDir *lsoDir;
    LSOBasic *lso;
    ObjTokenID fileID;
    char *file;
    SysStatus rc;

    // initialize the in-memory structures
    if (sb == NULL)  initFS(disk);

    rc = parseName(1, sb, g, newPath, lsoDir, file);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    // check if the file already exists
    rc = lsoDir->matchDir(file, strlen(file), &fileID);
    if (rc < 0) {
        // doesn't exist, so create the new file
	rc = lsoDir->createRecord(&fileID, OT_LSO_BASIC);
	_IF_FAILURE_RET(rc);

	lso = (LSOBasic *)lsoDir->getLocalRecordMap()->getObj(&fileID);
	lso->initAttribute(S_IFREG | (mode & ~S_IFMT), uid, gid);

        lsoDir->createEntry(file, strlen(file), &fileID,
			    S_IFREG | (mode & ~S_IFMT), uid);
        lsoDir->flush();
    } else {
	lso = (LSOBasic *)lsoDir->getLocalRecordMap()->getObj(&fileID);
    }

    // now write the contents of the file
    tassertMsg(lso != NULL, "?");
    KFSStat stat;
    // ensure that this is a file
    lso->getAttribute(&stat);
    if (!S_ISREG(stat.st_mode)) {         // error!
	if (S_ISDIR(stat.st_mode)) {
	    printf("%s: Error for newfile %s: Can't overwrite a directory "
		   "with a file!\n", prog, file);
	    // FIXME: for now let's allow things proceed
	    return 0;
	} else {
	    printf("%s: Error for newfile %s: stat.st_mode is %lo\n",
		   prog, file, (uval) stat.st_mode);
	    // FIXME: for now let's allow things proceed
	    return 0;
	}
    }

    // [over]write the file (the program fscp using this method has already
    // verified fd represents an existing, regular file
    uval64 offset;
    sval64 count;
    char buf[OS_BLOCK_SIZE];
    offset = 0;
    while ((count = read(fd, buf, OS_BLOCK_SIZE)) > 0) {
        lso->writeBlock(offset + count,
                        offset / OS_BLOCK_SIZE, buf, PSO_EXTERN);
        offset += count;
    }

    if (count < 0) {
	printf("Error while reading from file input file (returned <0)\n");
	tassertMsg(0, "?");
 	return -1;
    }

    // sync the disk metadata
    lso->flush();
    lsoDir->flush();
    g->recordMap->flush();
    sb->sync();

    return 0;
}

/*
 * createDirKFS
 */
SysStatus
createDirKFS(char *prog, Disk *disk, char *dir, uval uid, uval gid)
{
    LSOBasicDir *lsoDir, *lsoNewDir;
    ObjTokenID fileID;
    char *dirname;
    SysStatus rc;

    // initialize the in-memory structures
    if (sb == NULL) initFS(disk);

    rc = parseName(1, sb, g, dir, lsoDir, dirname);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    // check if the file already exists
    rc = lsoDir->matchDir(dirname, strlen(dirname), &fileID);
    if (rc < 0) {
        // doesn't exist, so create the new directory
#ifdef KFS_USE_GLOBAL_RECORDMAP
	rc = lsoDir->createRecord(&fileID, OT_LSO_BASIC_DIR);
#else
	rc = lsoDir->createRecord(&fileID, OT_LSO_DIR_EMB);
#endif
	_IF_FAILURE_RET(rc);

	lsoNewDir = (LSOBasicDir *)lsoDir->getLocalRecordMap()->getObj(&fileID);
	lsoNewDir->initAttribute(S_IFDIR | (0755 & ~S_IFMT), uid, gid);

        lsoDir->createDir(dirname, strlen(dirname), 0755, uid, &fileID);
        lsoDir->flush();

	lsoNewDir->flush();
    } else {
	err_printf("%s: directory %s already exists, no action taken by "
		   "createDirKFS\n", prog, dir);
	return 0;
    }

    // sync the disk metadata
    g->recordMap->flush();
    sb->sync();
    
    return 0;
}

/*
 * linkFileKFS()
 *
 *   Creates a hard link from the old path to the new path, creating
 *   any requested directory structure along the way.
 */
sval
linkFileKFS(char *prog, Disk *disk, char *oldPath, char *newPath)
{
    LSOBasicDir *lsoDir;
    char *oldfile;
    SysStatus rc;
    ObjTokenID oldfileID, newfileID;

    // initialize the in-memory structures
    if (sb == NULL) initFS(disk);

    // find oldPath
    rc = parseName(0, sb, g, oldPath, lsoDir, oldfile);
    if (_FAILURE(rc)) {
	tassertMsg(_SGENCD(rc) == ENOENT, "? rc 0x%lx\n", rc);
	return rc;
    }

    // check if the oldfile exists, and it's a regular file
    rc = lsoDir->matchDir(oldfile, strlen(oldfile), &oldfileID);
    if (_FAILURE(rc)) {
	return _SERROR(2611, 0, ENOENT);
    }
    // get the stat information for the old file
    KFSStat stat;
    LSOBasic *oldlso = (LSOBasic *)lsoDir->getLocalRecordMap()->getObj(&oldfileID);
    tassertMsg(oldlso != NULL, "?");
    oldlso->getAttribute(&stat);
    if (!S_ISREG(stat.st_mode)) {
	// FIXME for now we're only dealing with files and directories.
	// Maybe we'll need to deal with symlinks at some point ...
	return _SERROR(2612, 0, EPERM);
    }

    // find newPath
    char *newfile;
    rc = parseName(1, sb, g, newPath, lsoDir, newfile);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    // check if the file already exists
    rc = lsoDir->matchDir(newfile, strlen(newfile), &newfileID);
    if (_FAILURE(rc)) {
        // doesn't exist, so create the new file
	// FIXME: uid, gid ?
        lsoDir->createEntry(newfile, strlen(newfile), &oldfileID, 0, 0);
        lsoDir->flush();
    } else {
	// just update the existing entry
        lsoDir->updateEntry(newfile, strlen(newfile), &oldfileID);
    }

    // sync the directory
    lsoDir->flush();

    // sync the disk metadata
    g->recordMap->flush();
    sb->sync();

    return 0;
}

/*
 * createSymLinkKFS()
 *
 *   Creates a new symlink on the disk, creating any requested directory
 *   structure along the way.
 */
SysStatus
createSymLinkKFS(char *prog, Disk *disk, char *oldPath, char *newPath,
		 uval uid, uval gid)
{
    LSOBasicDir *lsoDir;
    ObjTokenID fileID;
    char *file;
    SysStatus rc;

    // initialize the in-memory structures
    if (sb == NULL)  initFS(disk);

    rc = parseName(1, sb, g, newPath, lsoDir, file);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    // check if the file already exists
    rc = lsoDir->matchDir(file, strlen(file), &fileID);
    if (rc < 0) {
        // doesn't exist, so create the new file

	char linkValue[512];
	int sz;
	if ((sz = readlink(oldPath, linkValue, sizeof(linkValue))) == -1) {
	    passertMsg(0, "readlink for %s failed\n", oldPath);
	    // FIXME: implement cleanup after error
	}

	passertMsg((uval)sz < sizeof(linkValue), "sz %ld, size of buffer %ld",
		   (uval)sz, (uval)(sizeof(linkValue)));
	linkValue[sz] = '\0';
	//err_printf("linkValue is %s\n", linkValue);

	rc = lsoDir->createRecord(&fileID, OT_LSO_BASIC_LNK);
	if (_FAILURE(rc)) {
	    passertMsg(0, "createSymLinkKFS() problem with allocRecord\n");
	    // FIXME: kfs tools stuff should have proper SysStatus ...
	    //	       "rc is (%ld, %ld, %ld)\n", _SERRCD(rc), _SCLSCD(rc),
	    //	       _SGENCD(rc)
	}

	LSOBasicSymlink *lsoSymlink;

	lsoSymlink = (LSOBasicSymlink *)lsoDir->getLocalRecordMap()->getObj(&fileID);
	tassertMsg(lsoSymlink != NULL, "?");

	rc = lsoSymlink->initAttribute(linkValue, S_IFLNK | (0777 & ~S_IFMT),
				       uid, gid);
	if (_FAILURE(rc)) {
	    passertMsg(0, "creareSymLinkKFS(): problem in initAttribute\n");
	}

	rc = lsoDir->createEntry(file, strlen(file), &fileID, S_IFLNK, 0);
	if (_FAILURE(rc)) {
	    passertMsg(0, "creareSymLinkKFS(): problem in createEntry\n");
	}
	lsoSymlink->flush();
	lsoDir->flush();
    } else {
	passertMsg(0, "NIY\n");
    }

    // sync the disk metadata
    g->recordMap->flush();
    sb->sync();

    return 0;
}

/*
 * recursiveList()
 *
 *   List all of the files and directories under this one
 */
static sval
recursiveList(LSOBasicDir *dir, char *pathName, KFSGlobals *g)
{
    int i = 0;
    uval offset;
    char dirEntryBuf[300];
    LSOBasicDir::DirEntry *entry = (LSOBasicDir::DirEntry *)dirEntryBuf;
    //    ObjToken otok;
    ObjTokenID otokID;
    KFSStat stat;
    LSOBasic *lso;
    LSOBasicDir *subDir;
    char subPath[1024];

    // loop through all the entries in the directory
    while (dir->matchIndex(i, &offset, entry) >= 0) {
	RecordMapBase *recordMap;
	if (!memcmp(entry->name, "..", 2)) {
	    stat.st_mode = S_IFDIR;
	    stat.st_size = 0;
	} else {
	    if (memcmp(entry->name, ".", entry->nameLength)) {
		recordMap = dir->getLocalRecordMap();
	    } else {
		recordMap = dir->getRecordMap();
	    }

	    otokID.id = entry->getOtokID();
	    lso = (LSOBasic *)recordMap->getObj(&otokID);
	    tassertMsg(lso != NULL, "?");
	    lso->getAttribute(&stat);
	}

	uval tmpID = entry->getOtokID();

	// Temporarily copying entry->name to subPath, because
	// entry->name is not null terminated.
	memcpy(subPath, entry->name, entry->nameLength);
	subPath[entry->nameLength] = '\0';
        printf("(%lu,%lu) %lo %s/%s   %ld bytes\n",
               tmpID, (uval)stat.st_nlink, (uval)stat.st_mode, pathName,
	       subPath, (uval)stat.st_size);

        if (S_ISDIR(stat.st_mode) &&
           memcmp(entry->name, ".", entry->nameLength) && 
	    memcmp(entry->name, "..", entry->nameLength)) {
	    //            subDir = (LSOBasicDir *)otok.getObj(NULL);
            subDir = (LSOBasicDir *)lso;
	    tassertMsg(subDir != NULL, "?");
            strcpy(subPath, pathName);
            strcat(subPath, "/");
            strncat(subPath, entry->name, entry->nameLength);

            recursiveList(subDir, subPath, g);
        }

        i = offset;
    }

    return 0;
}

/*
 * validateDiskKFS
 *
 *   A simple listing of all the files on the disk to help validate
 *   the current disk.
 */
sval
validateDiskKFS(Disk *disk)
{
    LSOBasicDir *root;

    // initialize the in-memory structures
    if (sb == NULL) initFS(disk);

    ObjTokenID rootTokID = sb->getRootLSO();

    // locate '/'
    root = (LSOBasicDir *)g->recordMap->getObj(&rootTokID);
    tassertMsg(root != NULL, "?");

    // now recursivly list every file
    return recursiveList(root, "", g);
}
