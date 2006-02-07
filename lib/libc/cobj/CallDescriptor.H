/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CallDescriptor.H,v 1.1 2004/01/24 20:58:16 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#ifndef __CALL_DESCRIPTOR_H_
#define __CALL_DESCRIPTOR_H_

// FIXME Don't use these; they don't work yet!
class CallDescriptorGeneric{
protected:
    // these aren't meant to be passed around
    virtual ~CallDescriptorGeneric() {}
    DEFINE_LOCALSTRICT_NEW(CallDescriptorGeneric);
    CallDescriptorGeneric() {}
public:
    virtual uval getIntArg(uval argno);
    virtual void setIntArg(uval argno, uval value);
    virtual double getFPArg(uval argno);
    virtual void setFPArg(uval argno, double value);
};

#include __MINC(CallDescriptor.H)

#endif
