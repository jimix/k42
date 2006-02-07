#ifndef __FR_COMPUTATION_PART_H_
#define __FR_COMPUTATION_PART_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRComputationPart.H,v 1.1 2004/11/01 19:07:56 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Another JA Kludge :-(
 *****************************************************************************/
#include "mem/FRComputation.H"
#include <meta/MetaFRComputationPart.H>

class FRComputationPart : public FRComputation __xbase(FR) {

protected:
    virtual SysStatus init(uval partitionSize);

    static SysStatus InternalCreate(ObjectHandle &frOH, uval partitionSize,
                                    ProcessID toProcID);

    DEFINE_GLOBAL_NEW(FRComputationPart);

#ifndef _IN_STUBGEN
public:
    DEFINE_ACCESS_SERVER(
	FRComputationPart,MetaObj::controlAccess|MetaObj::attach,
        MetaObj::none);
#endif

 __xpublic:
    static SysStatus _Create(__out ObjectHandle &frOH, __in uval partitionSize, 
                             __CALLER_PID caller);
};
#endif /* #ifndef __FR_COMPUTATION_PART_H_ */
