/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysFacts.C,v 1.34 2002/01/18 15:46:18 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FIXME, null implementation to get compiling
 * Fixed the pinned page allocator SysFacts init still needs to be fixed
 * **************************************************************************/

#include "kernIncs.H"
#include "init/SysFacts.H"
#include "bilge/arch/powerpc/BootInfo.H"

/*static*/ void
SysFacts::GetRebootImage(uval &imageAddr, uval &imageSize)
{
    imageAddr = _BootInfo->rebootImage;
    imageSize = _BootInfo->rebootImageSize;
}
