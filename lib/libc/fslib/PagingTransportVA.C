/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PagingTransportVA.C,v 1.2 2004/10/14 15:14:05 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides the communication between memory manager and
 *                     file system for paging data in/out.         
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <fslib/PagingTransport.H>

#include "PagingTransportVA.H"

class PagingTransportVA;
#define INSTNAME PagingTransportVA
#include <meta/TplMetaVAPageServer.H>
#include <xobj/TplXVAPageServer.H>
#include <tmpl/TplXVAPageServer.I>

typedef TplXVAPageServer<PagingTransportVA> XVAPageServerPT;
typedef TplMetaVAPageServer<PagingTransportVA> MetaVAPageServerPT;

SysStatus
PagingTransportVA::ClassInit(VPNum vp)
{
    if (vp != 0) return 0;
    MetaVAPageServerPT::init();
    return 0;
}

/* static */ SysStatus
PagingTransportVA::Create(PagingTransportRef &ref,
			   ObjectHandle &toh)
{
    PagingTransportVA *obj = new PagingTransportVA();

    CObjRootSingleRep::Create(obj);
    ref = (PagingTransportRef) obj->getRef();

    obj->init(toh);

    return 0;
}

void
PagingTransportVA::init(ObjectHandle &toh)
{
    lock.init();
    // create an object handle for upcall from kernel FR
    giveAccessByServer(toh, _KERNEL_PID, MetaVAPageServerPT::typeID());    
}
