#ifndef __KFS_FACTORY_H_
#define __KFS_FACTORY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSFactory.H,v 1.3 2004/05/06 19:52:49 lbsoares Exp $
 *****************************************************************************/

#include "KFSGlobals.H"
#include "SuperBlock.H"
#include "LSOBasic.H"
#include "LSOBasicDir.H"
#include "LSOBasicSymlink.H"
#include "PSOBasicRW.H"
#include "PSOPreallocExtent.H"
#include "PSOSmall.H"
#include "PSOSmallMeta.H"
#include "GlobalRecordMap.H"
#include "LSODirEmb.H"

class KFSFactory {
public:
    virtual SysStatus allocSuperBlock(SuperBlock* &sb, KFSGlobals *globals,
				      uval flags) {
	sb = new SuperBlock(globals->llpso, flags, globals);
	return 0;
    }
    virtual SysStatus allocRecordMap(RecordMapBase* &obj, uval32 rmapBlkno,
				     KFSGlobals *globals) {
#ifdef KFS_USE_GLOBAL_RECORDMAP
	obj = new RecordMap(rmapBlkno, globals);
#else
	obj = new GlobalRecordMap(rmapBlkno, globals);
#endif
	obj->init();
	return 0;
    }
    // register all the base ServerObject types
    virtual SysStatus registerServerObjectTypes(KFSGlobals* globals) {
	ServerObject *so;

	globals->soAlloc = new ServerObjectAllocator();

	so = new LSOBasic(globals);
	globals->soAlloc->join(so, OT_LSO_BASIC);

	so = new LSOBasicDir(globals);
	globals->soAlloc->join(so, OT_LSO_BASIC_DIR);

	so = new LSODirEmb(globals);
	globals->soAlloc->join(so, OT_LSO_DIR_EMB);

	so = new LSOBasicSymlink(globals);
	globals->soAlloc->join(so, OT_LSO_BASIC_LNK);

#ifdef PSOBASICRW_HAS_BEEN_FIXED_FOR_ENDIAN
	so = new PSOBasicRW(globals->llpso, globals);
	globals->soAlloc->join(so, OT_BASIC_RW);

	so = new PSOPreallocExtent(globals->llpso, globals);
	globals->soAlloc->join(so, OT_BASIC_EXTENT);
#endif // #ifdef PSOBASICRW_HAS_BEEN_FIXED_FOR_ENDIAN

	so = new PSOSmall(globals->llpso, globals);
	globals->soAlloc->join(so, OT_PRIM_UNIX);

	so = new PSOSmallMeta(globals->llpso, globals);
	globals->soAlloc->join(so, OT_PRIM_UNIX_META);

	return 0;
    }
};
#endif /* #ifndef __KFS_FACTORY_H_ */
