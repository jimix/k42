#ifndef __DTTYPE_H_
#define __DTTYPE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DTType.H,v 1.7 2005/03/02 05:27:56 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic FCM for mapping a file representative.
 * **************************************************************************/

#include <cobj/DataTransfer.H>

enum DTTypeRange { DTTYPE_MIN = 0, DTTYPE_MAX = DTTYPE_NUMBITS-1 };

// The smaller the value, the higher the priority in doing matchBest
enum DTTypeDefn {
    DTT_UNUSED =				DTTypeRange(0),

    // counters: 1-3
    DTT_COUNTER_PARTITIONED_VAL = 		DTTypeRange(1),
    DTT_COUNTER_PARTITIONED_REF = 		DTTypeRange(2),
    DTT_COUNTER_CANONICAL_REF = 		DTTypeRange(3),
    DTT_COUNTER_CANONICAL_VAL = 		DTTypeRange(4),

    // FCM: 5-16
    DTT_FCM_DEFAULTMRROOT = 			DTTypeRange(11),
    //DTT_FCM_COMMONMRROOT = 			DTTypeRange(12),
    DTT_FCM_DEFAULT = 				DTTypeRange(13),
    //DTT_FCM_COMMON = 				DTTypeRange(14),
    DTT_FCM_CANONICAL = 			DTTypeRange(15),

    // used for testing null hot swap times
    DTT_FCM_NULL_SWAP =				DTTypeRange(16),

    DTT_FACTORY_DEFAULT =                       DTTypeRange(17),
    DTT_PROCESS_DEFAULT =                       DTTypeRange(18),

    // available: 19-63
    DTT_TEST            =                       DTTypeRange(19),
    
    // THE END
    DTT_ERROR = DTTYPE_NUMBITS	// currently supporting 64 DTTs (IDs 0-63)
};

#endif /* #ifndef __DTTYPE_H_ */
