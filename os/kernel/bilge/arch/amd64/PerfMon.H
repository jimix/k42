#ifndef __PERF_MON_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __PERF_MON_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PerfMon.H,v 1.3 2001/10/05 21:49:12 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent performance monitoring
 * **************************************************************************/

// staright form x86 XXX


#ifndef INCLASS
__inline void
PerfMon::Start()
{
}

__inline void
PerfMon::Stop()
{
}

__inline void
PerfMon::Collect()
{
}

__inline void
PerfMon::Measure(Events choice)
{
}

__inline void
PerfMon::Print()
{
}

__inline void
PerfMon::Zero()
{
}
#endif /* #ifndef INCLASS */

