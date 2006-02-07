#ifndef __LSO_BASIC_FILE_H__
#define __LSO_BASIC_FILE_H__
/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LSOBasicFile.H,v 1.2 2004/05/05 19:57:58 lbsoares Exp $
 *****************************************************************************/
#include "LSOBasic.H"

#ifndef LONG_LONG_MAX
#define LONG_LONG_MAX ((long long)(~0ULL>>1))
#endif // #ifndef LONG_LONG_MAX
/*
 * The LSOBasicFile class maintains all of the interfaces to logical
 * server objects that are specifically used by files.
 */
class LSOBasicFile : public LSOBasic {
public:
    LSOBasicFile(ObjTokenID *otokID, RecordMapBase *r, KFSGlobals *g) :
	LSOBasic(otokID, r, g) { }

    sval truncate(uval32);
};

#endif /* !__LSO_BASIC_FILE_H__ */
