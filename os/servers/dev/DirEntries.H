#ifndef __DIR_ENTRIES_H_
#define __DIR_ENTRIES_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DirEntries.H,v 1.4 2003/03/27 17:44:40 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: manages file info on disk
 * **************************************************************************/

#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

class DirEntries {
    BLock lock;
    struct direntk42 *entry;

    uval entrySize;
    uval nextEntry;
    uval maxEntry;

    SysStatus grow(void);
public:
    DEFINE_GLOBALPADDED_NEW(DirEntries);

    SysStatusUval init(uval dot, uval dotdot);
    SysStatusUval addEntry(uval ino, uval type, const char *name, uval len);
    SysStatus lookupFile(const char *name, uval len, uval& ino);
    SysStatus rmEntry() {
	return _SERROR(2572, 0, ENOSYS);
    }
    SysStatusUval copyEntries(uval &cookie, struct direntk42 *buf, uval len);
};
#endif /* #ifndef __DIR_ENTRIES_H_ */
