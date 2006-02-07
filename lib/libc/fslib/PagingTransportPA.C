/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PagingTransportPA.C,v 1.2 2004/10/14 15:14:05 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides the communication between memory manager and
 *                     file system for paging data in/out.         
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <fslib/PagingTransport.H>

#include "PagingTransportPA.H"

class PagingTransportPA;
#define INSTNAME PagingTransportPA
#include <meta/TplMetaPAPageServer.H>
#include <xobj/TplXPAPageServer.H>
#include <tmpl/TplXPAPageServer.I>

typedef TplXPAPageServer<PagingTransportPA> XPAPageServerPT;
typedef TplMetaPAPageServer<PagingTransportPA> MetaPAPageServerPT;

SysStatus
PagingTransportPA::ClassInit(VPNum vp)
{
    if (vp != 0) return 0;

    MetaPAPageServerPT::init();
    return 0;
}

/* static */ SysStatus
PagingTransportPA::Create(PagingTransportRef &ref,
			   ObjectHandle &toh)
{
    PagingTransportPA *obj = new PagingTransportPA();

    CObjRootSingleRep::Create(obj);
    ref = (PagingTransportRef) obj->getRef();

    obj->init(toh);

    return 0;
}

void
PagingTransportPA::init(ObjectHandle &toh)
{
    lock.init();
    // create an object handle for upcall from kernel FR
    giveAccessByServer(toh, _KERNEL_PID, MetaPAPageServerPT::typeID());    
}
