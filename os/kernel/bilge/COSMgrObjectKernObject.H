#ifndef __COSMGR_OBJECT_KERN_OBJECT_H_
#define __COSMGR_OBJECT_KERN_OBJECT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COSMgrObjectKernObject.H,v 1.2 2005/01/26 03:21:51 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The Clustered Object System Manager (which itself is a
 *    Clustered Object.  There is one rep per vp.  which manges the local trans
 *    cache and the vp's portion of the main trans table.
 * **************************************************************************/
#include <cobj/sys/COSMgrObject.H>

class MemoryMgrPrimitive;

class COSMgrObjectKernObject : public COSMgrObject {
    class COSMgrObjectKernObjectRoot : public COSMgrObjectRoot {
    public:
	COSMgrObjectKernObjectRoot() { /* empty body */ }
	// Define a new which does not rely on standard allocators
	DEFINE_PRIMITIVE_NEW(COSMgrObjectKernObject::
				COSMgrObjectKernObjectRoot);

    };
    COSMgrObjectKernObjectRoot *root() { 
	return (COSMgrObjectKernObjectRoot *)myRoot; 
    }

    COSMgrObjectKernObject() { /* empty body */ }
    DEFINE_PRIMITIVE_NEW(COSMgrObjectKernObject);
public:
    static  SysStatus ClassInit(VPNum vp, MemoryMgrPrimitive *pa);
    static  SysStatusUval getKnownTypeList(COTypeDesc *typeDesc, uval numDescs);
    virtual SysStatus vpMaplTransTablePaged(VPNum vp,uval useExistingAddr=0);
};
#endif /* #ifndef __COSMGR_OBJECT_KERN_OBJECT_H_ */
