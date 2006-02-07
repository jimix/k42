#ifndef __DISK_CLIENT_RO_H_
#define __DISK_CLIENT_RO_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskClientRO.H,v 1.1 2003/03/09 21:03:47 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Read Only Interface to Disk Objects
 *                     This is useful for mounting read only file systems in
 *                     a transparent way to the filesystem (Livio)
 * **************************************************************************/

#include "DiskClient.H"
#include <cobj/CObjRootSingleRep.H>

class DiskClientRO: public DiskClient
{
public:
    DEFINE_GLOBAL_NEW(DiskClientRO);
    DiskClientRO() { }

    // Need to redefine the Create because we allocate a DiskClienRO
    // and not a regular DiskClient
    static SysStatus Create(DiskClientRef &dcr, char* blockDev) {
	DiskClient *dc = new DiskClientRO;
	
	SysStatus rc = dc->init(blockDev);

	_IF_FAILURE_RET(rc);

	CObjRootSingleRep::Create(dc);
	dcr = dc->getRef();
	
	return 0;
    }

    SysStatus writeBlock(uval blockNumber, void *block) {
	return 0;
    }

    SysStatus writeBlockPhys(uval blockNumber, uval paddr) {
	return 0;
    }
};
#endif /* #ifndef __DISK_CLIENT_H_ */
