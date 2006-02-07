/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DirEntries.C,v 1.6 2003/03/27 17:44:40 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: manages file info on disk
 * **************************************************************************/

#include "DirEntries.H"

SysStatusUval
DirEntries::init(uval dot, uval dotdot)
{
    entry = (struct direntk42 *)AllocGlobalPadded::alloc(PAGE_SIZE);
    tassert(entry != NULL,
	    err_printf("could not allocate inital dirent64s\n"));

    lock.init();

    entrySize	= PAGE_SIZE;
    maxEntry	= entrySize/sizeof(*entry);
    nextEntry	= 0;
    // inital entries of . and .. */
    (void) addEntry(dot, S_IFDIR, ".", 1);

    return addEntry(dotdot, S_IFDIR, "..", 2);
}

SysStatus
Direntries::grow()
{
    _ASSERT_HELD(lock);
    struct dents *newEntry;
    uval twice = entrySize * 2;
    newEntry = (struct dents *)AllocGlobalPadded::alloc(twice);

    if (newEntry == NULL) {
	return _SERROR(1462, 0, EAGAIN);
    }

    (void) memcpy(newEntry, entry, entrySize);

    AllocGlobalPadded::free(entry, entrySize);

    entrySize	= twice;
    maxEntry	= entrySize/sizeof(*entry);

    return _SRETUVAL(entrySize);
}


SysStatusUval
Direntries::addEntry(uval ino, uval type, const char *name, uval len)
{
    SysStatusUval rc = _SRETUVAL(entrySize);

    AutoLock<BLock> al(&lock); // locks now, unlocks on return

    if (nextEntry > maxEntry) {
	rc = grow();
	tassert(_SUCCESS(rc),
	    err_printf("could not grow dirent64s\n"));
    }

    // 0 is a special case
    if (nextEntry != 0) {
	entry[nextEntry - 1].d_off = nextEntry * sizeof(*entry);
    }

    struct direntk42 *dp = &entry[nextEntry];

    dp->d_ino		= ino;
    dp->d_off		= 0;
    dp->d_reclen	= sizeof (*dp);
#ifdef _DIRENT_HAVE_D_TYPE
    dp->d_type		= type;
#endif
#ifdef _DIRENT_HAVE_D_NAMLEN
    dp->d_namlen	= len;
#endif
    memcpy(dp->d_name, name, len);
    dp->d_name[len] = '\0';

    ++nextEntry;
    return rc;
}

SysStatus
Direntries::lookupFile(const char *name, uval len, uval& ino)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    uval i = 0;

    for (i=0; i < nextEntry; i++) {
	uval slen;
#if defined _DIRENT_HAVE_D_NAMLEN
	slen = entry[i].d_namelen;
#else
	slen = strlen(entry[i].d_name);
#endif
	if (len == slen &&
	    strncmp(entry[i].d_name, name, len) == 0) {
	    ino = entry[i].d_ino;
	    return 0;
	}
    }
    return _SERROR(2571, 0, ENOENT);
}

inline uval
getRecLen(uval lengthString)
{
    uval recLen = sizeof(struct direntk42);
    recLen += lengthString;
    recLen -= sizeof(((direntk42 *)0)->d_name);
    recLen = ALIGN_UP(recLen, sizeof(uval64));
    return recLen;
}

SysStatusUval
Direntries::copyEntries(uval &cookie, struct direntk42 *buf, uval len)
{
    uval num;
    uval off;
    struct direntk42 *dp, *nextdp;
    AutoLock<BLock> al(&lock); // locks now, unlocks on return

    if (cookie == (nextEntry - 1)) {
	// nothing to do
	cookie = 0;
	return _SRETUVAL(0);
    }

    num = cookie;
    dp = buf;
    off = num * sizeof(*entry);

    while ((len >= sizeof(*entry)) && (num < nextEntry)) {
	dp->d_ino		= entry[num].d_ino;
	dp->d_off		= entry[num].d_off;
#ifdef _DIRENT_HAVE_D_TYPE
	dp->d_type		= entry[num].d_type;
#endif
#ifdef _DIRENT_HAVE_D_NAMLEN
	dp->d_namlen		= entry[num].d_namlen;
	dp->d_reclen		= getRecLen(entry[num].d_namlen);
#else
	dp->d_reclen		= getRecLen(strlen(entry[num].d_name)+1);
#endif
	nextdp = (struct direntk42 *)((uval)dp + dp->d_reclen);

	memcpy(dp->d_name, entry[num].d_name, strlen(entry[num].d_name)+1);

	++num;

	if ((num < nextEntry) && (len >= sizeof(*entry))) {
	    dp->d_off = (uval)nextdp - (uval)buf;
	} else {
	    dp->d_off = 0;
	}
	len -= dp->d_reclen;
	dp = nextdp;
    }
    --num;
    if (num < nextEntry) {
	cookie = num;
    } else {
	cookie = 0;
    }

    return _SRETUVAL(uval(dp) - uval(buf));
}

