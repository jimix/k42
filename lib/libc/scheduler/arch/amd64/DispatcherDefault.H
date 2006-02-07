#ifndef __DISPATCHER_DEFAULT_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __DISPATCHER_DEFAULT_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefault.H,v 1.4 2001/10/05 21:48:14 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    Basic user-level dispatcher functionality.
 * **************************************************************************/

class Thread;

// VVV more to do??? this is unused now pdb XXX

struct DispatcherDefaultArch {
    /* FIXME -- X86-64 */
    uval64 fpscr;	// current default for floating-point status/ctrl reg

    void init() { fpscr = 0; }
};

extern Thread *CurrentThread;
