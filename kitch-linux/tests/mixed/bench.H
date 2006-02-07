/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bench.H,v 1.4 2001/10/05 21:46:53 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Multiprocessor microbenchmarks
 * **************************************************************************/
#ifndef __BENCH_H_
#define __BENCH_H_

static inline uval min_time(uval _counts[], int count_size) {
    uval min = _counts[1] - _counts[0];
    int i;
    for(i = 1; i < count_size; i++) {
	if(min > (_counts[i+1] - _counts[i])) {
	    min = (_counts[i+1] - _counts[i]);
	}
    }
    return min;
}

static inline uval max_time(uval _counts[], int count_size) {
    uval max = _counts[1] - _counts[0];
    int i;
    for(i = 1; i < count_size; i++) {
	if(max < (_counts[i+1] - _counts[i])) {
	    max = (_counts[i+1] - _counts[i]);
	}
    }
    return max;
}

static inline uval tot_time(uval _counts[], int count_size) {
    uval tot = 0;
    int i;
    for(i = 0; i < count_size; i++) {
	tot += _counts[i+1] - _counts[i];
    }
    return tot;
}

#define TIMER_DECL(ITERS) uval _counts[ITERS+1]; const int _tot_iters = ITERS; int _i
#define START_TIMER for(_i = 0; _i < _tot_iters; _i++) { \
                        _counts[_i] = Scheduler::SysTimeNow()
#define END_TIMER } _counts[_i] = Scheduler::SysTimeNow()
#define DO_YIELD Scheduler::YieldProcessor()
#define TICKS_TO_US(TIME) ((double(TIME) / double(Scheduler::TicksPerSecond())) * \
			   double(1000000.0))
#define MIN_TIME TICKS_TO_US(min_time(_counts, _tot_iters))
#define MAX_TIME TICKS_TO_US(max_time(_counts, _tot_iters))
#define AVG_TIME TICKS_TO_US((double)(tot_time(_counts, _tot_iters)) / \
                             (double)(_tot_iters))

#endif /* #ifndef __BENCH_H_ */
