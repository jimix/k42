/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MIPKernelMethods.C,v 1.2 2004/01/16 15:59:09 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/RegionDefault.H"
#include "proc/ProcessReplicated.H"
#include "MIP.H"

/* static */ SysStatus
RegionDefault::CreateUnAttachedMIPRegion(RegionRef &mipRef, ProcessRef procRef, 
                                         uval mipVaddr, 
                                         uval mipSize,
                                         uval mipOffset,
                                         AccessMode::mode mipAccessMode,
                                         FRRef mipFrRef)
{
    return -1;
}


/* virtual */ SysStatus 
ProcessReplicated::createMIP(uval &mipVaddr, uval mipSize,
                             uval chunkSize, uval mipOptions)
{
    return -1;
}


/* virtual */ SysStatus 
ProcessReplicated::destroyMIP(uval regionVaddr, uval mipOptions)
{
    return -1;
}

/* virtual */ SysStatus 
ProcessReplicated::flushMIP(uval regionVaddr, uval regionSize,
                            uval mipOptions) 
{
    return -1;
}

/* virtual */ SysStatus 
ProcessReplicated::fetchMIP(uval regionVaddr, uval regionSize,  
                            uval mipOptions) {
    return -1;
}

/* static */ SysStatus 
MIP::ClassInit(VPNum)
{
    return -1;
}

/* static */ uval
MIP::ExtendMemory(uval)
{
    return 0;
}
