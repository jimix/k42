#ifndef __GENERIC64_DISPATCHER_DEFAULT_H_
#define __GENERIC64_DISPATCHER_DEFAULT_H_
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefault.H,v 1.2 2001/10/05 21:48:15 peterson Exp $
 *************************************************************************** */

/*****************************************************************************
 * Module Description:
 *    Basic scheduler functionality.
 * **************************************************************************/

class Thread;

struct DispatcherDefaultArch
{
  void init() { }
};

extern Thread *CurrentThread;


#endif /* #ifndef ... */
