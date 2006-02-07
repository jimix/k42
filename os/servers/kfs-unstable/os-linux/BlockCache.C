/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: BlockCache.C,v 1.1 2004/02/11 23:03:58 lbsoares Exp $
 **************************************************************************/

#include "kfsIncs.H"
#include "BlockCache.H"
#include "BlockCacheLinux.H"

/* static */ BlockCache*
BlockCache::CreateBlockCache(Disk *disk, KFSGlobals *globals)
{
    (void) disk;
    return new BlockCacheLinux(disk);
}
