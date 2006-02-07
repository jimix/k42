/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: crtTrace.C,v 1.2 2004/04/20 18:38:15 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of tracing facility
 * **************************************************************************/

#include <stdarg.h>
#include <string.h>
#include <sys/kinclude.H>
#include "ktypes.h"

#define TRACE_LIB_K42 1
#include <trace/traceLib.H>
