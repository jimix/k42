#ifndef __FREELIST_H_
#define __FREELIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000-2003
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FreeList.H,v 1.1 2003/01/29 13:26:49 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Maintains a per-file system free list
 * **************************************************************************/
#include <fslib/FSFile.H>
class TimerEventFS;

class FreeList {
    /*
     * A global list of free server files to be collected.  Obviously
     * going to be a bottleneck.  Eventually, make this a distributed
     * structure, with a list per processor, coallesc if not used into
     * a global list, and then free.
     */
    LockedAutoList freeSF;

    // timer event for the file destruction deamon
    TimerEventFS *te;
public:
    SysStatus freeServerFile(FSFile::FreeServerFileNode *n);
	
    SysStatus unFreeServerFile(FSFile::FreeServerFileNode *n);

    /*
     * tryDestroyFree files is a routine that can be called from outside
     * due to memory pressure to destroy freed files (i.e., files in the
     * freeSF list.)
     *      ticksecs: specifies minimum amount of time that a ServerFile
     *            should be in the list before we try to destroy it.
     */
    SysStatus tryDestroyFreeFiles(uval ticksecs);

    /* setupFileDestruction does work to set up how often the destruction
     * daemon should be activate and minimum time a file should be in the freeSF
     * list before the daemon tries to destroy it.
     * setupFileDestruction should be called by file system initialization code.
     *
     * Arguments:
     *       secsWake: time interval between calls to tryDestroyFreeFiles
     *       secsCall: specifies argument to be given to tryDestroyFreeFiles
     *
     * If arguments are not specified, default values are used.
     */
    SysStatus setupFileDestruction(uval secsWake=0, uval secsCall = 0);
    
    void init() { te = NULL; freeSF.init(); }
};

#endif //__FREELIST_H_
