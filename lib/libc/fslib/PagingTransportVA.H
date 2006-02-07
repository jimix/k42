#ifndef __PAGING_TRANSPORT_VA_H_
#define __PAGING_TRANSPORT_VA_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PagingTransportVA.H,v 1.1 2004/10/03 14:45:02 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Abstract interface for object providing the
 *                     communication between memory manager and
 *                     file system for paging data in/out (specialized for
 *                     VAPageServer)
 * **************************************************************************/

#include "PagingTransport.H"
#include <meta/MetaVAPageServer.H>

class PagingTransportVA : public PagingTransport {
public:

    DEFINE_GLOBAL_NEW(PagingTransportVA);
    DEFINE_ACCESS_SERVER(VAPageServer, MetaObj::controlAccess,
			 MetaObj::none);
    PagingTransportVA() {}
    ~PagingTransportVA() {}

    static SysStatus ClassInit(VPNum vp);
    static SysStatus Create(PagingTransportRef &ref, ObjectHandle &toh);
    void init(ObjectHandle &toh);
};
#endif // #ifndef __PAGING_TRANSPORT_VA_H_
