/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceLib.C,v 1.38 2004/04/12 13:09:16 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of tracing facility
 * **************************************************************************/

#include "sys/sysIncs.H"
#include <scheduler/Scheduler.H>

#define TRACE_INFO &kernelInfoLocal.traceInfo

#include <trace/traceLib.H>
