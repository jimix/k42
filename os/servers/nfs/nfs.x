/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: nfs.x,v 1.5 2000/05/11 11:30:09 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  
 * **************************************************************************/

const MAXDATA = 8192;
const MAXPATHLENGTH = 1024;
const MAXNAMELENGTH = 255;
const COOKIESIZE  = 4;
const FHSIZE = 32;     

typedef string filename<MAXNAMELENGTH>;
typedef string path<MAXPATHLENGTH>;        
typedef opaque nfsfhandle[FHSIZE]; 
typedef unsigned int nfscookie;
typedef opaque nfsdata<MAXDATA>;

enum nfsstat {
  NFS_OK = 0,
  NFSERR_PERM=1,
  NFSERR_NOENT=2,
  NFSERR_IO=5,
  NFSERR_NXIO=6,
  NFSERR_ACCES=13,
  NFSERR_EXIST=17,
  NFSERR_NODEV=19,
  NFSERR_NOTDIR=20,
  NFSERR_ISDIR=21,
  NFSERR_FBIG=27,
  NFSERR_NOSPC=28,
  NFSERR_ROFS=30,
  NFSERR_NAMETOOLONG=63,
  NFSERR_NOTEMPTY=66,
  NFSERR_DQUOT=69,
  NFSERR_STALE=70,
  NFSERR_WFLUSH=99
};        

enum ftype {
  NFNON = 0,
  NFREG = 1,
  NFDIR = 2,
  NFBLK = 3,
  NFCHR = 4,
  NFLNK = 5
};     

struct timevalue {
  unsigned int seconds;
  unsigned int useconds;
}; 

struct sattr {
  unsigned int mode;
  unsigned int uid;
  unsigned int gid;
  unsigned int size;
  timevalue    atime;
  timevalue    mtime;
}; 

struct fattr {
  ftype        type;
  unsigned int mode;
  unsigned int nlink;
  unsigned int uid;
  unsigned int gid;
  unsigned int size;
  unsigned int blocksize;
  unsigned int rdev;
  unsigned int blocks;
  unsigned int fsid;
  unsigned int fileid;
  timevalue    atime;
  timevalue    mtime;
  timevalue    ctime;
};      

struct sattrargs {
  nfsfhandle file;
  sattr attributes;
};

struct diropargs {
  nfsfhandle  dir;
  filename name;
}; 

struct readargs {
  nfsfhandle file;
  unsigned offset;
  unsigned count;
  unsigned totalcount;
};

struct writeargs {
  nfsfhandle file;
  unsigned beginoffset;
  unsigned offset;
  unsigned totalcount;
  nfsdata data;
};

struct createargs {
  diropargs where;
  sattr attributes;
};

struct renameargs {
  diropargs from;
  diropargs to;
};

struct linkargs {
  nfsfhandle from;
  diropargs to;
};

struct symlinkargs {
  diropargs from;
  path to;
  sattr attributes;
};              

struct readdirargs {
  nfsfhandle dir;
  nfscookie cookie;
  unsigned count;
};

/*------------------------------------------*/
union attrstat switch (nfsstat status) {
case NFS_OK:
  fattr attributes;
default:
  void;
};
/*------------------------------------------*/

/*------------------------------------------*/
struct diropok_tag {
  nfsfhandle file;
  fattr   attributes;
};

union diropres switch (nfsstat status) {
case NFS_OK:
  diropok_tag diropok;
default:
  void;
};
/*------------------------------------------*/

/*------------------------------------------*/
union readlinkres switch (nfsstat status) {
case NFS_OK:
  path data;
default:
  void;
};
/*------------------------------------------*/

/*------------------------------------------*/
struct readok_tag {
  fattr attributes;
  nfsdata data;  
};

union readres switch (nfsstat status) {
case NFS_OK:
  readok_tag readok;
default:
  void;
};
/*------------------------------------------*/

/*------------------------------------------*/
struct entry {
  unsigned fileid;
  filename name;
  nfscookie cookie;
  entry *nextentry;
};  

struct readdirok_tag {
  entry *entries;
  bool eof;
};

union readdirres switch (nfsstat status) {
case NFS_OK:
  readdirok_tag readdirok;  
default:
  void;
};
/*------------------------------------------*/

/*------------------------------------------*/
struct info_tag {
  unsigned tsize;
  unsigned bsize;
  unsigned blocks;
  unsigned bfree;
  unsigned bavail;
};   

union statfsres switch (nfsstat status) {
case NFS_OK:
  info_tag info;
default:
  void;
};
/*------------------------------------------*/

/*
 * Remote file service routines
 */
program NFS_PROGRAM {
  version NFS_VERSION {
    void NFSPROC_NULL(void) = 0;                     /* Not used */
    attrstat NFSPROC_GETATTR(nfsfhandle) = 1;
    attrstat NFSPROC_SETATTR(sattrargs) = 2;
    void NFSPROC_ROOT(void) = 3;                     /* Not used */ 
    diropres NFSPROC_LOOKUP(diropargs) = 4;
    readlinkres NFSPROC_READLINK(nfsfhandle) = 5;
    readres NFSPROC_READ(readargs) = 6;
    void NFSPROC_WRITECACHE(void) = 7;               /* Not used */
    attrstat NFSPROC_WRITE(writeargs) = 8;
    diropres NFSPROC_CREATE(createargs) = 9;
    nfsstat NFSPROC_REMOVE(diropargs) = 10;
    nfsstat NFSPROC_RENAME(renameargs) = 11;
    nfsstat NFSPROC_LINK(linkargs)  = 12;
    nfsstat NFSPROC_SYMLINK(symlinkargs) = 13;
    diropres NFSPROC_MKDIR(createargs) = 14;
    nfsstat NFSPROC_RMDIR(diropargs) = 15;
    readdirres NFSPROC_READDIR(readdirargs) = 16;
    statfsres NFSPROC_STATFS(nfsfhandle) = 17;
  } = 2;
} = 100003;

