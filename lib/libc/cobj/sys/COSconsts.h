#ifndef __COSCONSTS_H_
#define __COSCONSTS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COSconsts.h,v 1.19 2003/03/12 04:55:55 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The Clustered Object System Manager (which itself is a
 *    Clustered Object.  There is one rep per vp.  which manges the local trans
 *    cache and the vp's portion of the main trans table.
 * **************************************************************************/

//  FIXME:  Not Sure any of this is really needed. Have tried to centralize
//          all constants in COSMgr.  Note few that are required are those
//          needed in assembly files.

#define GCOTRANSTABLEPAGESIZE PAGE_SIZE
#define GCOLOGTRANSTABEPAGESIZE LOG_PAGE_SIZE

#define GCOLOGPINNEDPAGES 1
#define GCOPINNEDPAGES  (1 << GCOLOGPINNEDPAGES)
#define GCOPINNEDSIZE   (GCOPINNEDPAGES * GCOTRANSTABLEPAGESIZE)
#define GCOLOGPINNEDSIZE (GCOLOGPINNEDSIZE + GCOLOGTRANSTABEPAGESIZE)

#define GCOLOGPAGABLEPAGESPERPART 11
#define GCOPAGABLEPAGESPERPART (1<<GCOLOGPAGABLEPAGESPERPART)
#define GCOPAGABLESIZEPERPART   (GCOPAGABLEPAGESPERPART * GCOTRANSTABLEPAGESIZE)
#define GCOLOGPAGABLESIZEPERPART (GCOLOGPAGABLEPAGESPERPART + \
                                  GCOLOGTRANSTABEPAGESIZE)

//FIXME: BIG KLUDGE would really like it to be this but for the moment...
#if 0
#define COSMAXVPS            (Scheduler::VPLimit)
#else
#define COSMAXVPS            32
#endif

#define GCOTRANSTABLEPAGABLESIZE  (GCOPAGABLESIZEPERPART * COSMAXVPS)
#define GCOTRANSTABLESIZE   (GCOPINNEDSIZE + GCOTRANSTABLEPAGABLESIZE)

#define NUMCOVFUNCS           256

#endif /* #ifndef __COSCONSTS_H_ */
