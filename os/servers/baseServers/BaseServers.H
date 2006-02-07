#ifndef __BASE_SERVERS_H_
#define __BASE_SERVERS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BaseServers.H,v 1.3 2001/10/05 21:50:33 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines operations on which applications can
 * block to wait for base facilities to be fully initialized.
 * **************************************************************************/

#include <meta/MetaBaseServers.H>

class BaseServers : private Obj {
public:
    static void ClassInit() { MetaBaseServers::init(); }
__xprotected:
    static SysStatus _TestAlive() { return 0; }
};


#endif /* #ifndef __BASE_SERVERS_H_ */
