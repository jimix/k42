/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * Some corrections by Livio Soares (livio@ime.usp.br)
 *
 * $Id: PSOPreallocExtent.C,v 1.13 2004/11/01 19:37:35 dilma Exp $
 *****************************************************************************/

#include <kfsIncs.H>

#ifndef KFS_TOOLS
#include "ServerFileBlockKFS.H"
#endif // #ifndef KFS_TOOLS

#include "SuperBlock.H"
#include "ObjToken.H"
#include "PSOBase.H"
#include "KFSDebug.H"

#include "PSOPreallocExtent.H"
#include "PSOTypes.H"
#include "KFSGlobals.H"
#include "RecordMap.H"

#define EXTENT_SIZE 8

/*
 * writeBlock()
 *
 *   Writes the logical block specified to the disk. This operation is
 *   the only difference between PSOBasicRW and PSOPreallocExtent.
 */
sval
PSOPreallocExtent::writeBlock(uval32 lblkno, char *buffer,
			      PSOBase::AsyncOpInfo *cont /* = NULL */)
{
    sval rc;
    uval32 dblkno;
    PSOBase *pso;

    // lock this PSO
    AutoLock<BLock> al(lock);

    if (lblkno < RW_MAXBLK) {
	dblkno = dblk[lblkno];

        // If there isn't a block already there, allocate one
        if (dblkno == 0) {
	    uval32 extentLen = 1;

	    // Here is where we do extent allocation!
	    // First, try to see how many blocks we can sequentially allocate
	    while (extentLen < EXTENT_SIZE
		   && lblkno + extentLen < RW_MAXBLK
		   && !dblk[lblkno + extentLen]) {
		extentLen++;
	    }

	    KFS_DPRINTF(DebugMask::PSO_REALLOC_EXTENT,
			"PSOPreallocExtent::writeBlock() trying to allocate "
			"%u blocks\n", extentLen);
            rc = dblkno = globals->super->allocExtent(extentLen);
	    KFS_DPRINTF(DebugMask::PSO_REALLOC_EXTENT,
			"PSOPreallocExtent::writeBlock() successfully "
			"allocated %u blocks\n", extentLen);

	    if (_FAILURE(rc)) {
		tassertMsg(0, "PSOPreallocExtent::writeBlock() failed to "
			   "alloc extent\n");
#ifndef KFS_TOOLS // so we don't include ServerFileBlockKFS for tools ...
		// trigger continuation
		ServerFileBlockKFS::CompleteIORequest(cont->obj,
						      cont->addr, cont->offset,
						      rc);
#endif // #ifndef KFS_TOOLS
		return rc;
	    }
	    for (uval i = 0; i < extentLen; i++) {
		dblk[lblkno + i] = dblkno + i;
	    }

            locked_markDirty(lblkno);
        }

        KFS_DPRINTF(DebugMask::PSO_REALLOC_EXTENT_RW,
		    "PSOPreallocExtent::writeBlock: writing block %u %u\n",
                    dblkno, lblkno);
	passertMsg(dblkno, "PSOPreallocExtent::writeBlock() bad disk block "
		   "for writing d=%u, l=%u, this=0x%p\n",
		   dblkno, lblkno, this);

        // FIXME: change disk routines to handle correct token
	rc = llpso->writeBlock(dblkno, buffer, cont);

        return rc;
    }

    /* pass on to sub-object */
    KFS_DPRINTF(DebugMask::PSO_REALLOC_EXTENT_RW,
		"PSOPreallocExtent::write_page: forwarding subobj\n");

    pso = (PSOBase *)subObj.getObj(fsfile);
    if (pso == NULL) {
        // need to get a new PSO to extend the file! Since allocRecord writes
	// the entry to subObjID it will be written out to the data buffer,
	// because subObjID is a pointer to the correct place in the data buffer.
	rc = globals->recordMap->allocRecord(OT_BASIC_EXTENT, subObjID);
	if (_FAILURE(rc)) {
	    tassertMsg(0, "PSOPreallocExtent::writeBlock() problem creating "
		       "ORSMap entry\n");
#ifndef KFS_TOOLS // so we don't include ServerFileBlockKFS for tools ...
	    // trigger continuation
	    ServerFileBlockKFS::CompleteIORequest(cont->obj,
						  cont->addr, cont->offset,
						  rc);
#endif // #ifndef KFS_TOOLS
	    return rc;
	}
        subObj.setID(*subObjID);

	// We have to make sure the subObjID goes to disk. It'll hopefully get
	// correctly flushed later. Warning: lockedMarkDirty() won't work here.
        flags |= PSO_BASICRW_DIRTY;

        pso = (PSOBase *)subObj.getObj(fsfile);
        if(pso == NULL) {
            // this is a problem...
	    passertMsg(0, "? this is a problem");
	    // FIXME return proper SERROR
	    // FIXME deal with trigerring continuation
            return -1;
        }
    }
    rc = pso->writeBlock(lblkno - RW_MAXBLK, buffer, cont);

    return rc;
}

/*
 * clone()
 *
 *   Creates a new PSOPreallocExtent from the given "object related state" map entry.
 */
/* virtual */ ServerObject *
PSOPreallocExtent::clone(ObjTokenID otokID, FSFileKFS *f)
{
    ServerObject *pso = new PSOPreallocExtent(otokID, f, llpso, globals);
    return pso;
}
