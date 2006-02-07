#ifndef __DCO_XFER_OBJ_H_
#define __DCO_XFER_OBJ_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dcoXferObj.H,v 1.5 2001/10/16 05:32:17 okrieg Exp $
 *****************************************************************************/

class DataTransferObjectCounterPtr : public DataTransferObject {
    sval *_ptr;
public:
    DEFINE_LOCALSTRICT_NEW(DataTransferObjectCounterPtr);
    DataTransferObjectCounterPtr(sval *p) : _ptr(p) { /* empty body */ }
    virtual ~DataTransferObjectCounterPtr() { /* empty body */ }

    sval *ptr() {return _ptr;}
};

class DataTransferObjectCounter : public DataTransferObject {
    sval _val;
public:
    DEFINE_LOCALSTRICT_NEW(DataTransferObjectCounter);
    DataTransferObjectCounter(sval v) : _val(v) { /* empty body */ }
    virtual ~DataTransferObjectCounter() { /* empty body */ }

    sval value() {return _val;}
};

#endif /* #ifndef __DCO_XFER_OBJ_H_ */
