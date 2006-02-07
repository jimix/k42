#ifndef __STUB_BASE_OBJ_H_
#define __STUB_BASE_OBJ_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StubBaseObj.H,v 1.10 2001/10/05 21:47:21 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/****************************************************************************
 * Module Description: Superclass from which all system objects are
 * defined.  This is the only stub class that is manually generated, it
 * is used to define the function to initialize the object handle to set the
 * Object handle of the object
 * **************************************************************************/


class StubBaseObj {
    TYPE_DECL(StubBaseObj,<INVALID>);
protected:
    ObjectHandle oh;
public:
    // need a fake type to deal with stub SuperClass constructor bypass
    enum StubObjConstr { UNINITIALIZED };

    inline StubBaseObj(StubObjConstr) { }

    inline void initOHWithPID(ProcessID pid, XHandle xh) {
	oh.initWithPID(pid,xh);
    }

    inline void initOHWithCommID(CommID commID, XHandle xh) {
	oh.initWithCommID(commID, xh);
    }

    inline void setOH(ObjectHandle toh) { oh = toh; }

    inline ObjectHandle getOH() { return oh; }

    inline ProcessID getPid() { return oh.pid(); }

    void* operator new(size_t size);
};

#endif /* #ifndef __STUB_BASE_OBJ_H_ */
