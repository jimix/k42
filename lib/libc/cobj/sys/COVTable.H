#ifndef __COVTABLE_H_
#define __COVTABLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COVTable.H,v 1.9 2001/11/13 18:29:12 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include "COSconsts.h"

class COVTableEntry {
    friend void genConstants(void);

#if __GNUC__ < 3
    static const uval LOG_SIZE_IN_UVALS = 1;	// 2 uvals, needed in assembler
    uval _offset;
#else
    static const uval LOG_SIZE_IN_UVALS = 0;	// 1 uval, needed in assembler
#endif
    uval _func;

public:
    COVTableEntry()         { _func=(uval)0; }
    void setFunc(uval func) { _func=func; }
    uval getFunc()          { return (_func); }
};

struct  COVTable {
    COVTableEntry vt[NUMCOVFUNCS];
};

#endif /* #ifndef __COVTABLE_H_ */
