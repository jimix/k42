#ifndef __FILE_INFO_K42_RAM_FS_H_
#define __FILE_INFO_K42_RAM_FS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileInfoK42RamFS.H,v 1.20 2005/09/07 10:27:45 dilma Exp $
 *****************************************************************************/

#include <io/FileLinux.H>
#include <fslib/DentryList.H>
#include <fslib/FSCreds.H>
#include <fslib/fs_defines.H>

#include <time.h>

class FileInfoK42RamFS {
    static const uval FIRST_INO = 0;
public:
    static uval nextFileNumber;
    static uval GetFileNumber() {
	uval fn = FetchAndAdd(&nextFileNumber, 1);
        return fn;
    }

    FileLinux::Stat status;

    DEFINE_GLOBAL_NEW(FileInfoK42RamFS);

    void init(uval useCredential = 1) {
	status.st_ino = GetFileNumber();
	/* FIXME HACK: when we mount this FS from baseServers we don't
	 * have linux environmnet ready, so for the root directory
	 * let's just fake uid,gid */
	if (useCredential) {
#ifndef NO_CREDENTIAL_CHECKING
	    ProcessLinux::creds_t *creds = FSCreds::Get();
	    status.st_uid = creds->fsuid;
	    status.st_gid = creds->fsgid;
#else
	    status.st_uid = 0;
	    status.st_gid = 0;
#endif // #ifdef NO_CREDENTIAL_CHECKING
	} else {
	    status.st_uid = 0;
	    status.st_gid = 0;
	}
	status.st_nlink = 1;
	status.st_size = 0;
	status.st_blocks = 0;
	status.st_blksize = 0x1000;
	status.st_atime = status.st_mtime = status.st_ctime = time(NULL);
    };
    FileInfoK42RamFS() {
	init();
    };
    FileInfoK42RamFS(uval useCredential) {
	init(useCredential);
    };
    
    FileInfoK42RamFS(mode_t mode) {
	tassertMsg(S_ISREG(mode), "should be reg file, mode is %o\n", mode);
	init();
	status.st_mode = mode;
	tassertMsg(status.st_nlink == 1, "?");
    }
};

class FileInfoK42RamFSDir : public FileInfoK42RamFS {
public:
    struct DirEntry : public DentryListHash::HashEntry {
	// field _obj is used for finfo
	FileInfoK42RamFS* GetFinfo() { return (FileInfoK42RamFS*)_obj; }
    };

    DentryListHash entries;

    DEFINE_GLOBAL_NEW(FileInfoK42RamFSDir);
    FileInfoK42RamFSDir(mode_t mode) : FileInfoK42RamFS() {
	status.st_mode = (mode & ~S_IFMT) | S_IFDIR;
	status.st_nlink = 2;
    }
    FileInfoK42RamFSDir(mode_t mode, uval useCredential) : 
	    FileInfoK42RamFS(useCredential) {
	status.st_mode = (mode & ~S_IFMT) | S_IFDIR;
	status.st_nlink = 2;
    }

    ~FileInfoK42RamFSDir() {
	entries.destroy();
    }    

    uval makeEmpty();

    DirEntry *lookup(char *name, uval namelen);

    SysStatus add(char *nm, uval len, FileInfoK42RamFS *finfo);
    uval remove(char *nm, uval len);
    uval isEmpty();
    uval prepareForRemoval();
    SysStatusUval getDents(uval &cookie, struct direntk42 *buf, uval len);
};

class FileInfoK42RamFSSymLink : public FileInfoK42RamFS {
    char path[PATH_MAX+1];
    uval pathlen;
public:
    DEFINE_GLOBAL_NEW(FileInfoK42RamFSSymLink);
    FileInfoK42RamFSSymLink(char *linkvalue) : FileInfoK42RamFS() {
	pathlen = strlen(linkvalue);
	tassertMsg(pathlen < PATH_MAX + 1, "?");
	memcpy(path, linkvalue, pathlen);
	status.st_mode = (0777 & ~S_IFMT) | S_IFLNK;
	status.st_size = pathlen;
    }
    uval getPath(char* &p) {
	p = path;
	return pathlen;
    }
};

class FileInfoK42RamFSPipe : public FileInfoK42RamFS {
public:
    DEFINE_GLOBAL_NEW(FileInfoK42RamFSPipe);
    FileInfoK42RamFSPipe(mode_t mode) : FileInfoK42RamFS() {
	status.st_mode = (mode & ~S_IFMT) | S_IFIFO;
    }
};

class FileInfoK42RamFSSocket : public FileInfoK42RamFS {
public:
    DEFINE_GLOBAL_NEW(FileInfoK42RamFSSocket);
    FileInfoK42RamFSSocket(mode_t mode) : FileInfoK42RamFS() {
	status.st_mode = (mode & ~S_IFMT) | S_IFSOCK;
    }
};
#endif /* #ifndef __FILE_INFO_K42_RAM_FS_H_ */
