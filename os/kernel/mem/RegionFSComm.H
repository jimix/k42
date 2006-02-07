#ifndef __REGION_FSCOMM_H_
#define __REGION_FSCOMM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionFSComm.H,v 1.10 2004/10/29 16:30:33 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Special region for kernel/File system
 * communication.  The FR in the kernel plugs individual pages into
 * this region to get them in the file systems address space when
 * sending physical pages to the file system to handle faults.
 * **************************************************************************/

#include "mem/RegionDefault.H"

/* the following object is passed to potentially blocking code paths
   to provide a structure that can be used to block I/O requests */
class IORestartRequests;		

class RegionFSComm;
typedef RegionFSComm **RegionFSCommRef;


class RegionFSComm : public RegionDefault {
    typedef BLock LockType;
    LockType       commLock;		// lock for bitvector
    uval pagesUsed;			// bitvector of pages used
    IORestartRequests *head;		// head of queue of waiting FCMs/threads
    

    DEFINE_PINNEDGLOBAL_NEW(RegionFSComm);
#ifndef _IN_STUBGEN
    static const uval RegionPageCount = sizeof(uval)*8;
    void init(void *memBuf) {pagesUsed = 0; buf = memBuf;}
#else /* #ifndef _IN_STUBGEN */
    void init(void *memBuf);
#endif /* #ifndef _IN_STUBGEN */
    uval *index;  //keep track of physical addrs for pages
		  //in "buf"
    void *buf;	  //pool of pages
public:
    // pushes data from paddr to vaddr, making it valid
    virtual SysStatus attachPagePush(uval paddr, uval &vaddr, 
				     IORestartRequests *rr=0);

    // maps paddr to vaddr
    virtual SysStatus attachPage(uval paddr, uval &vaddr, 
				 IORestartRequests *rr=0);

    // if a read, copies date to final target first
    virtual SysStatus detachPage(uval vaddr);

    virtual SysStatus destroy();
    virtual SysStatus getType(TypeID &id);

    // initializes meta information for creating regions of this type
    static void ClassInit(VPNum vp);
__xprivate:
    static SysStatus _Create(__out uval &vaddr, __CALLER_PID caller);
};

#endif /* #ifndef __REGION_FSCOMM_H_ */
