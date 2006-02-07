#ifndef FRCRW_DEFH
#define FRCRW_DEFH
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRCRW.H,v 1.7 2002/11/07 19:05:23 marc Exp $
 *****************************************************************************/
#include "mem/FRComputation.H"
#include "mem/Region.H"

/*
 * FIXME: when we put in unmap from the FCM to the region, then we should
 * do the unmap if we are copyonreference, but not do it if we are copyonwrite
 */

class FRCRW : public FRComputation __xbase(FR) {
protected:
    FRRef  baseFR;			// the base FR
    DEFINE_GLOBAL_NEW(FRCRW);
    void init(FRRef baseFR, uval nnode);

public:
    static uval NoCopyOnReference;
    static void ClassInit(VPNum vp);
    static SysStatus Create(FRRef& newFRRef, FRRef baseFRRef);

__xpublic:
    static SysStatus _Create(
	__out ObjectHandle &frOH, __in ObjectHandle baseFrOH,
	__CALLER_PID caller);
};
#endif /* #ifndef FRCRW_DEFH */
