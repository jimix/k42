#ifndef __MISC_CALLBACK_H_
#define __MISC_CALLBACK_H_

/******************************************************************************
 * Copyright (C) 2005 Andrew Baumann (andrewb@cse.unsw.edu.au).
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Callback.H,v 1.1 2005/08/09 12:03:23 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple class for doing callbacks, defines a single
 * virtual method for completion notification
 * **************************************************************************/

class Callback {
public:
   virtual void complete(SysStatus success) = 0;
};

#endif /* !defined(__MISC_CALLBACK_H_) */
