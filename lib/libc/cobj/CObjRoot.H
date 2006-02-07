#ifndef __COBJ_ROOT_H_
#define __COBJ_ROOT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRoot.H,v 1.24 2005/03/02 05:27:55 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <cobj/sys/COSTransObject.H>
#include <cobj/sys/COSMissHandler.H>
#include <cobj/DataTransfer.H>
#include "XObjectList.H"

class MemoryMgrPrimitive;
class CObjRep;

/*
 * A typical clustered object is composed of a Root object and
 * possibly one or more Representatives.  The root object serves
 * as the misshandler for the clustered with respect to the COS.
 * The root object is resposible for the management of the representatives
 * of the Clustered Object it is a part of.  All representatives of
 * all Clustered Objects have a pointer to the root of the Clustered Object
 * it is a part of.  A rep can obtain its Clustered Object Id (CORef)
 * from the root.  Of course a rep may choose to cache the value.
 */
class CObjRoot : public COSMissHandler {
    CObjRoot() { /* empty body, no default constructor exported */ }
protected:
    friend class Obj;
    XObjectListObj exported;		// list of x objects exported
    virtual void resetTransSet();
public:
    // make sure all destructors are virtual and called properly
    // let compiler generate the destructors, only declare it here
    // the important consequence of this is that delete gets passed the
    // correct size even if the pointer is of a base class type
    virtual ~CObjRoot() { /* empty body */ }
    RepRef  getRef() { return (RepRef) COSMissHandler::getRef(); }

    //FIXME:  Don't like having this here but will talk to Kevin about this
    virtual CObjRep *getRepOnThisVP() = 0;

    // Data-transfer-related methods
    virtual SysStatus getDataTransferExportSet(DTTypeSet *set) {
	// by default we do not support an export type
	set->reset(); // reset to the null or empty set
	return 0;
    }
    virtual SysStatus getDataTransferImportSet(DTTypeSet *set) {
	// by default we do not support an import type
	set->reset(); // reset to the null or empty set
	return 0;
    }
    virtual DataTransferObject *dataTransferExport(DTType dtt,
						   VPSet transferVPSet);
    virtual SysStatus dataTransferImport(DataTransferObject *dtobj,
					 DTType dtt, VPSet transferVPSet);

    enum InstallDirective { skipInstall, Install };
    virtual SysStatus reAssignRef(CORef &ref);
    virtual SysStatus deRegisterFromFactory();
protected:
    CObjRoot(RepRef ref, InstallDirective idir);
    // pool specified kind of ref to allocate
    CObjRoot(uval8 pool);
    static SysStatus CleanupMsgHandler(uval repUval);
    static SysStatus CleanupRep(VPNum theVP, CObjRep* theRep);
};


#endif /* #ifndef __COBJ_ROOT_H_ */
