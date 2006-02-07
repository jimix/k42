#ifndef __COSMISS_HANDLER_H_
#define __COSMISS_HANDLER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COSMissHandler.H,v 1.11 2001/10/16 05:31:52 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/

// This class defines what must be exported by a class to serve
// as a misshandler in the COS. This should be in its own
// header file with the COS stuff.  COSMissHandler.H

#include "COSTransObject.H"

class VPSet;

/*
 * Split out a base class to force the vtable pointer to the front of the
 * object.  This property isn't functionally necessary but makes debugging
 * easier.  See TransPageDesc::printVTablePtrs().
 */
class COSMissHandlerBase {
public:
    virtual ~COSMissHandlerBase() { /* empty body */ }
    DEFINE_NOOP_NEW(COSMissHandlerBase);
};

class COSMissHandler : public COSMissHandlerBase {
protected:
    CORef myRef;
    inline COSMissHandler() { myRef = 0; }
    DEFINE_NOOP_NEW(COSMissHandler);
public:
    virtual SysStatus handleMiss(COSTransObject * &co,
				 CORef ref, uval methodNum) = 0;
    enum  CleanupCmd {STARTCLEANUP, DOVPCLEANUP};
    virtual SysStatus cleanup(CleanupCmd cmd) = 0;
    virtual VPSet   getTransSet() = 0;
    virtual VPSet   getVPCleanupSet() = 0;
    CORef   getRef()  { return myRef; }

};
#endif /* #ifndef __COSMISS_HANDLER_H_ */
