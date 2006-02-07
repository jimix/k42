/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NFSMount.C,v 1.18 2001/11/01 12:23:27 mostrows Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <io/Socket.H>
#include "NFSHandle.H"
#include "NFSClient.H"
#include "NFSMount.H"
#include <arpa/inet.h>

fhstatus*
NFSMount::mnt(dirpath *dpath)
{
    memset((char *)&fhStat, 0, sizeof(fhStat));
    if ( rpcCall(MOUNTPROC_MNT,
		 (uval) xdr_dirpath, (uval) dpath,
		 (uval) xdr_fhstatus, (uval) &fhStat) != RPC_SUCCESS) {
	return (NULL);
    }
    return &fhStat;
}

mountlist*
NFSMount::dump()
{
    memset((char *)&mountList, 0, sizeof(mountList));
    if (rpcCall(MOUNTPROC_DUMP, (uval) xdr_void, (uval) 0,
		(uval) xdr_mountlist, (uval) &mountList) != RPC_SUCCESS) {
	return (NULL);
    }
    return (&mountList);
}

void
NFSMount::umnt(dirpath *dpath)
{
    memset((char *)&result, 0, sizeof(result));
    rpcCall(MOUNTPROC_UMNT, (uval) xdr_void, (uval) dpath,
	    (uval) xdr_void, (uval) &result);
}

void
NFSMount::umntall()
{
    char res=0;
    rpcCall(MOUNTPROC_UMNT, (uval) xdr_void, (uval) 0,
	    (uval) xdr_void, (uval) &res);
}

exportlist*
NFSMount::exports()
{
    memset((char *)&exportList, 0, sizeof(exportList));
    if ( rpcCall(MOUNTPROC_EXPORT, (uval) xdr_void, (uval) 0,
		 (uval) xdr_exportlist, (uval) &exportList) != RPC_SUCCESS) {
	return (NULL);
    }
    return (&exportList);
}

NFSMount::~NFSMount()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    if (client->cl_auth)
	auth_destroy(client->cl_auth);
    clnt_destroy(client);
}

SysStatus
NFSMount::mount(char *path, NFSHandle &fhandle)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    fhstatus *fhstat;

    fhstat = mnt(&path);
    if (fhstat->status != 0) {
	//printf("mnt status:%d\n", fhstat->status);
	return _SERROR(1397, 0, NFSClient::ToLinuxErrorNo(fhstat->status));
    }

    fhandle.init(fhstat->fhstatus_u.directory);

    return 0;
}
