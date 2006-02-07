#ifndef __COSTRANS_OBJECT_H_
#define __COSTRANS_OBJECT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COSTransObject.H,v 1.7 2001/10/16 05:31:52 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

// Class that defines what is required to act as an externally
// visible entity within the COS. In the case of K42 this
// or inother words it defines the type of object which can be
// installed into a localtranslation table
// Should also be in its own header file
class COSTransObject {
public:
    DEFINE_NOOP_NEW(COSTransObject);
    virtual ~COSTransObject() { /* empty body */ }
};

typedef COSTransObject **CORef;

#endif /* #ifndef __COSTRANS_OBJECT_H_ */
