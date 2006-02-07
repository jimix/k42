/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockCache.C,v 1.1 2004/02/11 23:03:57 lbsoares Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include "BlockCache.H"
#include "BlockCacheK42.H"

/* static */ BlockCache*
BlockCache::CreateBlockCache(Disk *disk, KFSGlobals *globals)
{
    return new BlockCacheK42(disk);
}
