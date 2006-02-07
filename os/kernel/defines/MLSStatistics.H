#ifndef __MLSSTATISTICS_H_
#define __MLSSTATISTICS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MLSStatistics.H,v 1.10 2001/10/16 05:32:05 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Hardware dependant statistics
 * **************************************************************************/

// #define DO_MLS_STATS
struct MLSStatistics {
    uval stime[20];
    uval time[20];

#ifdef DO_MLS_STATS
    static void StartTimer(uval i);
    static void DoneTimer(uval i);
    static void PrintAndZeroTimers(uval doPrint);
#else /* #ifdef DO_MLS_STATS */
    static void StartTimer(uval) { /* empty body */ }
    static void DoneTimer(uval) { /* empty body */ }
    static void PrintAndZeroTimers(uval) { /* empty body */ }
#endif /* #ifdef DO_MLS_STATS */
};




#endif /* #ifndef __MLSSTATISTICS_H_ */
