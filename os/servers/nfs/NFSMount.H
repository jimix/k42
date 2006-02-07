#ifndef __NFSMOUNT_H_
#define __NFSMOUNT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NFSMount.H,v 1.14 2001/11/01 12:23:27 mostrows Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include "mount.h"
#include "NFSClient.H"
class NFSHandle;

class NFSMount :protected NFSClient{

    //void null();                   /* Not used */
    fhstatus* mnt(dirpath *dpath);
    mountlist* dump();
    void umnt(dirpath *dpath);
    void umntall();
    exportlist* exports();

    // for results
    union {
	char result;
	mountlist mountList;
	exportlist exportList;
	fhstatus fhStat;
    };

public:
    DEFINE_GLOBAL_NEW(NFSMount);
    NFSMount(char *aHost, char *aProto):
	NFSClient(aHost,aProto, MOUNTPROG, MOUNTVERS) {};

    virtual ~NFSMount();

    SysStatus mount(char *path, NFSHandle &fhandle);
};

#endif /* #ifndef __NFSMOUNT_H_ */
