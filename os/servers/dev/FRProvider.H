#ifndef __FRPROVIDER_H_
#define __FRPROVIDER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRProvider.H,v 1.3 2004/02/27 17:14:20 mostrows Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Interface that creates FR's for devfs files
 ****************************************************************************/
class FRProvider : public Obj {
__xpublic:

    //we can't make a sync call from kernel to devfs
    //_getRegistration will pull the arguments, and _registerComplete
    // push back the results
    virtual SysStatus _registerComplete(SysStatus rc, ObjectHandle oh,
					__XHANDLE xhandle);
    virtual SysStatus _getRegistration(__outbuf(*:buflen) char* name,
				       __in uval buflen,
				       __out uval &devID, // should be dev_t
				       __out uval &mode,  // should be mode_t
				       __out ObjectHandle &parent,
				       __out uval &token,
				       __XHANDLE xhandle);
    virtual SysStatus _open(__out uval &devSize, __out uval &blkSize);

    virtual SysStatus _getFROH(__out ObjectHandle &oh,
			       __in ProcessID pid, __XHANDLE xhandle) = 0;
    virtual SysStatusUval _ioctl(__in uval req,
				 __inoutbuf(size:size:size) char* buf,
				 __inout uval &size);

};

#endif /* #ifndef __FRPROVIDER_H_ */
