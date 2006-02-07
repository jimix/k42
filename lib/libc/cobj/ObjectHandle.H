#ifndef __OBJECT_HANDLE_H_
#define __OBJECT_HANDLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ObjectHandle.H,v 1.19 2002/07/02 18:12:52 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: An object is identified by a processID and an object Id
 * **************************************************************************/

#define XHANDLE_NONE 0

struct ObjectHandle {
    CommID  _commID;
    XHandle _xhandle;

    inline uval invalid() { return (_xhandle == XHANDLE_NONE); }
    inline uval valid()   { return (_xhandle != XHANDLE_NONE); }
    inline uval isSameOH(ObjectHandle otherOH) {
	return(_commID == otherOH._commID && _xhandle == otherOH._xhandle);
    }

    inline void init() {
	_commID  = 0;
	_xhandle = XHANDLE_NONE;
    }

    inline void initWithOH(ObjectHandle oh) {
	_commID  = oh.commID();
	_xhandle = oh.xhandle();
    }

    inline void initWithPID(ProcessID p, XHandle xh) {
	_commID = SysTypes::COMMID(p, SysTypes::DSPID(0, SysTypes::VP_WILD));
	_xhandle = xh;
    }

    inline void initWithCommID(CommID c, XHandle xh) {
	_commID = c; _xhandle = xh;
    }

    void initWithMyPID(XHandle xh);

    inline ProcessID pid()     { return SysTypes::PID_FROM_COMMID(_commID); }
    inline CommID    commID()  { return _commID; }
    inline XHandle   xhandle() { return _xhandle; }
};


/* definition of IDL compiler KeyWords */

#define __in
#define __inout
#define __out
#define __inbuf(spec)
#define __inoutbuf(spec)
#define __outbuf(spec)
#define __async
#define __CALLER_PID ProcessID
#define __XHANDLE XHandle

#define __xadef(arl)
#define __xa(arl)

#define __xbase(cls)
#define __xpublic    public
#ifdef _IN_STUBGEN
#define __xprotected public
#define __xprivate   public
#else /* #ifdef _IN_STUBGEN */
#define __xprotected protected
#define __xprivate   private
#endif /* #ifdef _IN_STUBGEN */

#endif /* #ifndef __OBJECT_HANDLE_H_ */
