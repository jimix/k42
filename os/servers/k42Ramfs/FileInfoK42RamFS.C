/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileInfoK42RamFS.C,v 1.15 2005/04/21 04:35:05 okrieg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <sys/ppccore.H>
#include <io/FileLinux.H>
#include "FileInfoK42RamFS.H"


/* static */ uval FileInfoK42RamFS::nextFileNumber = FIRST_INO;

FileInfoK42RamFSDir::DirEntry*
FileInfoK42RamFSDir::lookup(char *name, uval namelen)
{
    void *dnode;    
    SysStatus rc = entries.lookup(name, namelen, dnode);
    if (_SUCCESS(rc)) {
	return (DirEntry*) dnode;
    } else {
	return NULL;
    }
}

uval
FileInfoK42RamFSDir::remove(char *nm, uval len)
{
    SysStatus rc = entries.remove(nm, len);
    if (_SUCCESS(rc)) {
	return 1;
    } else {
	return 0;
    }
}

SysStatus
FileInfoK42RamFSDir::add(char *nm, uval len, FileInfoK42RamFS *finfo)
{
    NameHolderInfo nhi;
    nhi.flags = NameHolderInfo::IS_DIR | NameHolderInfo::IS_FSFILE;
    nhi.fsFile = (FSFile *)finfo;
    return entries.updateOrAdd(nm, len, NULL, &nhi);
}

inline uval
getRecLen(uval lengthString) {
    uval recLen = sizeof(struct direntk42);
    recLen += lengthString - sizeof(((direntk42 *)0)->d_name) + 1;
    recLen = ALIGN_UP(recLen, sizeof(uval64));
    return recLen;
}

SysStatus
FileInfoK42RamFSDir::getDents(uval &cookie, struct direntk42 *buf, uval len)
{
    void *curr = NULL;
    DentryListHash::HashEntry *dnode;
    struct direntk42 *dp;
    struct direntk42 *nextdp;
    uval dpend;
    uval i;

    curr = entries.getNext(curr, &dnode);
    for (i=0; i < cookie && curr != NULL; i++) {
#if 0
	err_printf("skipping element i %ld with  dnode->name[0](%c),"
		   " dnode->namelen %ld\n", i, dnode->name[0],
		   dnode->namelen);
#endif
	curr = entries.getNext(curr, &dnode);
    }

    tassert(len >= sizeof(struct direntk42),
	    err_printf("buf not large enough for struct direntk42\n"));

    dpend = (uval)buf + len;
    dp = nextdp = buf;

    while (curr != NULL) {
	cookie++;
	dp->d_ino	  = ((FileInfoK42RamFS*)dnode->_obj)->status.st_ino;
	dp->d_reclen      = getRecLen(dnode->strLen);
	// err_printf("computed reclen as %ld\n", (uval) dp->d_reclen);
#if defined DT_UNKNOWN && defined _DIRENT_HAVE_D_TYPE
	dp->d_type	  = DT_UNKNOWN;
#endif /* #if defined DT_UNKNOWN && defined ... */
#if defined _DIRENT_HAVE_D_NAMLEN
	dp->d_namlen      = dnode->namelen;
#endif /* #if defined _DIRENT_HAVE_D_NAMLEN */

#if 0
	err_printf("cookie is %ld, packing dnode->name[0](%c),"
		   " dnode->namelen %ld\n", cookie, dnode->name[0],
		   dnode->namelen);
#endif

	memcpy(dp->d_name, (char *)dnode->str, dnode->strLen);
	*(dp->d_name + dnode->strLen) = '\0';

	nextdp = (struct direntk42 *)((uval)dp + dp->d_reclen);

	curr = entries.getNext(curr, &dnode);

	// Make sure we can fit another
	if ((curr != NULL) &&
	    (((uval)nextdp + getRecLen(dnode->strLen)) < dpend)) {
	    dp->d_off = (uval)nextdp - (uval)buf;
	    dp = nextdp;
	} else {
	    dp->d_off = 0;
	    break;
	}
    }

#if 0
    err_printf("Leaving with cookie %ld, returning %ld\n", cookie,
	       ((uval)nextdp - (uval)buf));
#endif

    return _SRETUVAL((uval)nextdp - (uval)buf);
}

uval
FileInfoK42RamFSDir::makeEmpty()
{
    passertMsg(0, "why do we need this?\n");
    return 0;
}


uval
FileInfoK42RamFSDir::isEmpty()
{
    uval numEntries = entries.getNumEntries();
    tassertMsg(numEntries >= 2, "numEntries %ld\n", numEntries);
    void *dummy; 
    if (numEntries == 2) {
	SysStatus rc;
	rc = entries.lookup(".", 1, dummy);
	if (_SUCCESS(rc)) {
	    rc = entries.lookup("..", 2, dummy);
	    if (_SUCCESS(rc)) {
		return 1;
	    }
	}
    }

    return 0;
}


uval
FileInfoK42RamFSDir::prepareForRemoval()
{
    // We assume that the directory has been already checked for emptyness
    tassertMsg(isEmpty(), "ops");

    SysStatus rc = entries.remove(".", 1);
    if (_SUCCESS(rc)) {
	rc = remove("..", 2);
	if (_SUCCESS(rc)) {
	    status.st_nlink = 0;
	    return 1;
	}
    }
    tassertMsg(0, "problem with . or ..\n");
    return 0;
}

