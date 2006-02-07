#ifndef __MULTI_LINK_MANAGER_H_
#define __MULTI_LINK_MANAGER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MultiLinkManager.H,v 1.1 2004/02/11 23:04:00 lbsoares Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * Embedded object used to keep track of files with multiple links.
 * parameter unique is file system specific
 * FIXME: should just change this to using HashSimpleLocked
 * **************************************************************************/
#include <fslib/FSFile.H>
#include <sync/Lock.H>

class MultiLinkManager {
  
public: 
    typedef struct {
	ServerFileRef fref;
	BLock lock;
    } SFHolder; 
    static SFHolder* AllocHolder(ServerFileRef fref) {
	return NULL; 
    }
    void add(uval unique, SFHolder *ref) { }
    void init() { }
};
#endif /* __MULTI_LINK_MANAGER_H_ */
