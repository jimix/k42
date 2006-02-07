/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: LSOBasicFile.C,v 1.20 2005/07/05 16:56:47 dilma Exp $
 *****************************************************************************/

#include "kfsIncs.H"

#include <sys/stat.h>
#include <limits.h>
#include <time.h>
#include "LSOBasicFile.H"

sval
LSOBasicFile::truncate(uval32 length)
{
    PSOBase *pso;
    AutoLock <BLock> al(lock);

    // Make sure it is a file.
    if (S_ISREG(lsoBuf->statGetMode()) == 0) {
        tassertMsg(0, "LSOBasicFile::truncate() can only truncate files "
		   "(st_mode is 0x%lx)\n", (uval) lsoBuf->statGetMode());
	return _SERROR(2355, 0, EISDIR);
    }

    // free the associated blocks, if truncating to smaller size
    if (lsoBuf->statGetSize() > length) {
	pso = (PSOBase *)data.getObj(fsfile);
	tassertMsg(pso != NULL, "?");
	uval lblkno = (length == 0 ? 0 : ((length-1)/OS_BLOCK_SIZE + 1));
	pso->freeBlocks(lblkno, LONG_LONG_MAX);
    }

    // truncate the file to the given size
    lsoBuf->statSetSize(length);
    lsoBuf->statSetBlocks(ALIGN_UP(length, OS_BLOCK_SIZE) / OS_SECTOR_SIZE);

    // update the modification times
    uval64 t =  time(NULL);
    lsoBuf->statSetCtime(t);
    lsoBuf->statSetMtime(t);

    return 0;
}
