#ifndef __VOLATILE_STATE_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __VOLATILE_STATE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VolatileState.H,v 1.3 2002/04/12 13:23:42 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

struct VolatileState {

    codeAddress codeAddr() { return ((codeAddress)0); }
    codeAddress branchReg() { return ((codeAddress)0); }
};
