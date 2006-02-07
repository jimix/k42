#ifndef __TRACECOMPILE_H_
#define __TRACECOMPILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceCompile.H,v 1.2 2004/04/21 13:07:23 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: allow tracing to be compiled out
 * **************************************************************************/

// uncomment below to compile tracing out of system
//  doesn't compile out assembly level trace events

// tracing should not have dependencies on k42 infrastructure because
//  it will be used outside the k42 world.  I'll leave this is as a pointer
//  to where the compile out define is

// see traceCore.H

#endif /* #ifndef __TRACECOMPILE_H_ */
