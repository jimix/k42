#ifndef __FRPLACE_HOLDER_H_
#define __FRPLACE_HOLDER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRPlaceHolder.H,v 1.20 2004/10/29 16:30:32 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to applications for files in memory manager
 * **************************************************************************/

#include "mem/FRCommon.H"


class FRPlaceHolder;
typedef FRPlaceHolder **FRPlaceHolderRef;

class FRPlaceHolder : public FRCommon{
protected:
    DEFINE_GLOBAL_NEW(FRPlaceHolder);
    DEFINE_REFS(FRPlaceHolder);

public:
    virtual SysStatus putPage(uval physAddr, uval objOffset);

    virtual SysStatusUval startFillPage(uval physAddr, uval objOffset);

    virtual SysStatus _fsync();

    virtual SysStatus destroy();

    virtual SysStatus locked_getFCM(FCMRef &r);

    virtual SysStatus fcmNotInUse();

    static SysStatus Create(FRRef &frRef, uval useRef=0);

    virtual SysStatus startPutPage(uval physAddr, uval objOffset,
				   IORestartRequests *rr=0);

    virtual SysStatus setFileLength(uval fileLength);
};

class FRPlaceHolderPinned : public FRPlaceHolder{
protected:
    DEFINE_PINNEDGLOBAL_NEW(FRPlaceHolderPinned);
public:
    static SysStatus Create(FRRef &frRef, uval useRef=0);
};
#endif /* #ifndef __FRPLACE_HOLDER_H_ */
