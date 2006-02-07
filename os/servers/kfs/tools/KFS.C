/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFS.C,v 1.52 2005/04/14 18:04:21 dilma Exp $
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

#if defined (KFS_TOOLS) && ! defined(PLATFORM_OS_Darwin)
#include <malloc.h>
#endif

void KFSfree(void *ptr) { free(ptr); }
void* KFSalloc(uval size)
{
    void *ptr = malloc(size);
    passertMsg(ptr != NULL, "no more mem\n");
    return ptr;
}


static SuperBlock *superblock = NULL;
static KFSGlobals *globals =  NULL;

void
initFS(Disk *disk, uval format=0)
{
    if (superblock != NULL) {
	printf("initFS has already been called\n");
	return;
    }

    tassertMsg(globals == NULL, "how come?");
    globals = new KFSGlobals();

    superblock = initFS(disk, globals, format);
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
    if (superblock == NULL)  initFS(disk, 1);

    // format the superblock, and bitmaps
    rc = superblock->format("disk");
    passertMsg(_SUCCESS(rc), "superblock format() failed with rc 0x%lx\n", rc);

    // Get record oriented PSO for storing obj data
    KFSFactory factory;
    rc = factory.allocRecordMap(globals->recordMap,
				globals->super->getRecMap(), globals);
    tassertMsg(_SUCCESS(rc), "factory.allocRecordMap problem? rc 0x%lx\n",
	       rc);

    // create root directory
    superblock->createRootDirectory();

    // sync the disk metadata
    globals->recordMap->flush();
    superblock->sync();

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
    ObjTokenID otokID;
    ObjToken rootTok(g), otok(g);

    // locate '/'
    rootTok.setID(sb->getRootLSO());
    root = (LSOBasicDir *)rootTok.getObj(NULL);
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

		rc = g->recordMap->allocRecord(OT_LSO_BASIC_DIR, &otokID);
		otok.setID(otokID);

		lsoNewDir = (LSOBasicDir *)otok.getObj(NULL);
		// FIXME: Pass proper uid, gid
		lsoNewDir->initAttribute(S_IFDIR | (0755 & ~S_IFMT), 0, 0);

		lsoDir->createDir(subPath, strlen(subPath), 0755, 0, &otok);
		lsoDir->flush();
	    } else {
		return _SERROR(2610, 0, ENOENT);
	    }
        } else {
            // set the token to the id we located
            otok.setID(otokID);
        }

        subPath = strtok(NULL, "/");
        lsoDir = (LSOBasicDir *)otok.getObj(NULL);
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
    ObjToken *filetok;
    ObjTokenID fileID;
    char *file;
    SysStatus rc;

    // initialize the in-memory structures
    if (superblock == NULL)  initFS(disk);

    rc = parseName(1, superblock, globals, newPath, lsoDir, file);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    // check if the file already exists
    rc = lsoDir->matchDir(file, strlen(file), &fileID);
    if (rc < 0) {
        // doesn't exist, so create the new file
	rc = globals->recordMap->allocRecord(OT_LSO_BASIC, &fileID);
	filetok = new ObjToken(fileID, globals);

	lso = (LSOBasic *)filetok->getObj(NULL);
	lso->initAttribute(S_IFREG | (mode & ~S_IFMT), uid, gid);

        lsoDir->createEntry(file, strlen(file), filetok,
			    S_IFREG | (mode & ~S_IFMT), uid);
        lsoDir->flush();
    } else {
	filetok = new ObjToken(fileID, globals);
	lso = (LSOBasic *)filetok->getObj(NULL);
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
	    lso->writeBlock(offset + count, offset / OS_BLOCK_SIZE, buf);
        offset += count;
    }

    if (count < 0) {
	printf("Error while reading from file input file (returned <0)\n");
	tassertMsg(0, "?");
	return -1;
    }
    lso->flush();

    // sync the disk metadata
    globals->recordMap->flush();
    superblock->sync();

    return 0;
}

/*
 * createDirKFS
 */
SysStatus
createDirKFS(char *prog, Disk *disk, char *dir, uval uid, uval gid)
{
    LSOBasicDir *lsoDir, *lsoNewDir;
    ObjToken *filetok;
    ObjTokenID fileID;
    char *dirname;
    SysStatus rc;

    // initialize the in-memory structures
    if (superblock == NULL) initFS(disk);

    rc = parseName(1, superblock, globals, dir, lsoDir, dirname);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    // check if the file already exists
    rc = lsoDir->matchDir(dirname, strlen(dirname), &fileID);
    if (rc < 0) {
        // doesn't exist, so create the new directory
	rc = globals->recordMap->allocRecord(OT_LSO_BASIC_DIR, &fileID);
	filetok = new ObjToken(fileID, globals);

	lsoNewDir = (LSOBasicDir *)filetok->getObj(NULL);
	lsoNewDir->initAttribute(S_IFDIR | (0755 & ~S_IFMT), uid, gid);

        lsoDir->createDir(dirname, strlen(dirname), 0755, uid, filetok);
        lsoDir->flush();

	lsoNewDir->flush();
    } else {
	err_printf("%s: directory %s already exists, no action taken by "
		   "createDirKFS\n", prog, dir);
	return 0;
    }

    // sync the disk metadata
    globals->recordMap->flush();
    superblock->sync();

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

    // initialize the in-memory structures
    if (superblock == NULL) initFS(disk);

    // find oldPath
    rc = parseName(0, superblock, globals, oldPath, lsoDir, oldfile);
    if (_FAILURE(rc)) {
	tassertMsg(_SGENCD(rc) == ENOENT, "? rc 0x%lx\n", rc);
	return rc;
    }

    // check if the oldfile exists, and it's a regular file
    ObjToken *oldfiletok = new ObjToken(globals);
    ObjTokenID oldfileID = oldfiletok->getID();

    rc = lsoDir->matchDir(oldfile, strlen(oldfile), &oldfileID);
    if (_FAILURE(rc)) {
	return _SERROR(2611, 0, ENOENT);
    }
    oldfiletok->setID(oldfileID);
    // get the stat information for the old file
    KFSStat stat;
    LSOBasic *oldlso = (LSOBasic *)oldfiletok->getObj(NULL);
    tassertMsg(oldlso != NULL, "?");
    oldlso->getAttribute(&stat);
    if (!S_ISREG(stat.st_mode)) {
	// FIXME for now we're only dealing with files and directories.
	// Maybe we'll need to deal with symlinks at some point ...
	return _SERROR(2612, 0, EPERM);
    }

    // find newPath
    char *newfile;
    rc = parseName(1, superblock, globals, newPath, lsoDir, newfile);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    ObjToken *newfiletok = new ObjToken(globals);
    ObjTokenID newfileID = newfiletok->getID();

    // check if the file already exists
    rc = lsoDir->matchDir(newfile, strlen(newfile), &newfileID);
    if (_FAILURE(rc)) {
        // doesn't exist, so create the new file
	// FIXME: uid, gid ?
        lsoDir->createEntry(newfile, strlen(newfile), oldfiletok, 0, 0);
        lsoDir->flush();
    } else {
	// just update the existing entry
        lsoDir->updateEntry(newfile, strlen(newfile), oldfiletok);
    }

    // sync the directory
    lsoDir->flush();

    // sync the disk metadata
    globals->recordMap->flush();
    superblock->sync();

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
    ObjToken *filetok;
    ObjTokenID fileID;
    char *file;
    SysStatus rc;

    // initialize the in-memory structures
    if (superblock == NULL)  initFS(disk);

    rc = parseName(1, superblock, globals, newPath, lsoDir, file);
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

	rc = globals->recordMap->allocRecord(OT_LSO_BASIC_LNK, &fileID);
	if (_FAILURE(rc)) {
	    passertMsg(0, "createSymLinkKFS() problem with allocRecord\n");
// FIXME: kfs tools stuff should have proper SysStatus ...
//		       "rc is (%ld, %ld, %ld)\n", _SERRCD(rc), _SCLSCD(rc),
//		       _SGENCD(rc)
	}
	filetok = new ObjToken(fileID, globals);

	LSOBasicSymlink *lsoSymlink;

	lsoSymlink = (LSOBasicSymlink *)filetok->getObj(NULL);
	tassertMsg(lsoSymlink != NULL, "?");

	rc = lsoSymlink->initAttribute(linkValue, S_IFLNK | (0777 & ~S_IFMT),
				       uid, gid);
	if (_FAILURE(rc)) {
	    passertMsg(0, "creareSymLinkKFS(): problem in initAttribute\n");
	}

	rc = lsoDir->createEntry(file, strlen(file), filetok, S_IFLNK, 0);
	if (_FAILURE(rc)) {
	    passertMsg(0, "creareSymLinkKFS(): problem in createEntry\n");
	}
	lsoSymlink->flush();
	lsoDir->flush();
    } else {
	passertMsg(0, "NIY\n");
    }

    // sync the disk metadata
    globals->recordMap->flush();
    superblock->sync();

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
    ObjToken otok(g);
    ObjTokenID otokID;
    KFSStat stat;
    LSOBasic *lso;
    LSOBasicDir *subDir;
    char subPath[1024];

    // loop through all the entries in the directory
    while (dir->matchIndex(i, &offset, entry) >= 0) {
	otokID.id = entry->getOtokID();
        otok.setID(otokID);
        lso = (LSOBasic *)otok.getObj(NULL);
	tassertMsg(lso != NULL, "?");
        lso->getAttribute(&stat);

        uval tmpID = entry->getOtokID();

	// Temporarily copying entry->name to subPath, because
	// entry->name is not null terminated.
	memcpy(subPath, entry->name, entry->nameLength);
	subPath[entry->nameLength] = '\0';
        printf("(%lu) %lo %s/%s   %ld bytes\n",
               tmpID, (uval)stat.st_mode, pathName, subPath,
	       (uval)stat.st_size);

        if (S_ISDIR(stat.st_mode) &&
	    memcmp(entry->name, ".", entry->nameLength) &&
	    memcmp(entry->name, "..", entry->nameLength)) {
            subDir = (LSOBasicDir *)otok.getObj(NULL);
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
    if (superblock == NULL) initFS(disk);

    ObjToken rootTok(globals);

    // locate '/'
    rootTok.setID(superblock->getRootLSO());
    root = (LSOBasicDir *)rootTok.getObj(NULL);
    tassertMsg(root != NULL, "?");

    // now recursivly list every file
    return recursiveList(root, "", globals);
}
