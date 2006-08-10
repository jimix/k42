#ifndef __PERF_MON_H_
#define __PERF_MON_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PerfMon.H,v 1.12 2002/07/10 00:35:17 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent performance monitoring
 * **************************************************************************/

class PerfMon {
#define INCLASS
public:
    enum Events {
	NONE,
	BUS,
	DCACHE,
	INSN,
	TLB
    };

    static void Start();
    static void Stop();
    static void Collect();
    static void Measure(Events);
    static void Print();
    static void Zero();
    virtual SysStatusUval getCounter(uval counter);

    virtual ~PerfMon() { }
#include __MINC(PerfMon.H)
#undef INCLASS
};

#include __MINC(PerfMon.H)

#endif /* #ifndef __PERF_MON_H_ */
