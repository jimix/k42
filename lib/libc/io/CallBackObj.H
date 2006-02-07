#ifndef __CALLBACK_H_
#define __CALLBACK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CallBackObj.H,v 1.3 2004/11/02 19:40:05 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Intermediate object for stub-compiler
 * **************************************************************************/

// Generic async callback object; for client to be able to register callbacks
// to server objects.
class CallBackObj : public Obj __xbase(Obj) {
__xprivate:
    // make this generic
    virtual __async SysStatusUval _callBack(__in uval arg,__XHANDLE xhandle) {
	return 0;
    }
    virtual __async SysStatusUval _processCallBacks(__in uval arg) {
	return 0;
    }
};

#endif // #ifndef __CALLBACK_H_
