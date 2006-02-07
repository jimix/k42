/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: BlockCache.C,v 1.9 2003/09/23 15:22:55 dilma Exp $
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
