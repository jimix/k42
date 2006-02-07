#ifndef __FCMDATA_XFER_OBJ_H_
#define __FCMDATA_XFER_OBJ_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDataXferObj.H,v 1.5 2001/10/16 05:32:08 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic FCM for mapping a file representative.
 * **************************************************************************/

#include "mem/FCMDefault.H"
#include <cobj/DTType.H>

class DataTransferObjectFCMDefault : public DataTransferObject {
    FCMDefault *fcmrep;
    DataTransferObjectFCMDefault() { tassert(0, ;); /* should not be used */ }
public:
    DEFINE_LOCALSTRICT_NEW(DataTransferObjectFCMDefault);
    DataTransferObjectFCMDefault(DTType dtt, FCMDefault *v) : fcmrep(v) {
	dtt = dtt;
	tassert(dtt == DTT_FCM_DEFAULT, err_printf("DTType mismatch!\n"));
    }
    virtual ~DataTransferObjectFCMDefault() { /* empty body */ }

    FCMDefault *fcm() { return fcmrep; }
};

#endif /* #ifndef __FCMDATA_XFER_OBJ_H_ */
