#ifndef __FCMDEFAULT_ROOT_H_
#define __FCMDEFAULT_ROOT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDefaultRoot.H,v 1.7 2002/05/15 19:13:14 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic FCM for mapping a file representative.
 * **************************************************************************/

#include <cobj/CObjRootSingleRep.H>
#include "defines/experimental.H"

class FCMDefault;

class FCMDefaultRoot : public CObjRootSingleRep {

    friend class FCMDefault;

#ifdef ENABLE_FCM_SWITCHING
    SysStatus switchImplToMultiRep(uval csize = 1);
    SysStatus switchImplToSameRep(FCMDefaultRoot *newRoot);
#endif /* #ifdef ENABLE_FCM_SWITCHING */

    // These are made private as these are impl specific
    virtual SysStatus handleMiss(COSTransObject * &co, CORef ref,
			         uval methodNum);
    // ---------- Data-transfer-related methods ----------
    virtual SysStatus getDataTransferExportSet(DTTypeSet *set);
    virtual SysStatus getDataTransferImportSet(DTTypeSet *set);
    virtual DataTransferObject *dataTransferExport(DTType dtt, VPSet dtVPSet);
    virtual SysStatus dataTransferImport(DataTransferObject *dtobj,
					 DTType dtt, VPSet dtVPSet);

    SysStatus doTransferFromDefault(FCMDefault *fcmdef);
    // ---------------------------------------------------

public:
    DEFINE_GLOBAL_NEW(FCMDefaultRoot);

    FCMDefaultRoot(FCMDefault *co) : CObjRootSingleRep((CObjRep *)co) {
      /* empty body */
    }
    FCMDefaultRoot(FCMDefault *co, RepRef ref,
		   CObjRoot::InstallDirective idir=CObjRoot::Install) :
	CObjRootSingleRep((CObjRep *)co, ref, idir) {
	/* empty body */
    }

    static FCMRef Create(FCMDefault *co, void *ref = NULL, uval install = 1) {
	CObjRoot *root;

	if (ref == NULL) {
	    root = new FCMDefaultRoot(co);
	} else {
	    root = new FCMDefaultRoot(co, (RepRef)ref,
		    install ? CObjRoot::Install : CObjRoot::skipInstall);
	}

	return (FCMRef)root->getRef();
    }
};

#endif /* #ifndef __FCMDEFAULT_ROOT_H_ */
