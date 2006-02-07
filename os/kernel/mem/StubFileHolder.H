#ifndef __STUB_FILE_HOLDER_H_
#define __STUB_FILE_HOLDER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StubFileHolder.H,v 1.3 2005/01/10 15:30:01 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: object being used by FRs and KernelPagingTransport
 * **************************************************************************/

#include <defines/mem_debug.H>
#include <trace/traceMem.h>

#ifdef _IN_STUBGEN
class StubFileHolder;
#else
/*
 * FRPA and FRVA talk to different stubs which export the same interface
 * but FRPA makes calls on the interface as a base of FRVA as well
 * as when it is the object.
 * We insulate the majority of the code by using an adapter object
 */
class StubFileHolder : public Obj __xbase(Obj) {
public:
    virtual ~StubFileHolder() {};	// force virtual destructor
    virtual XHandle getXHandle()=0;
    virtual SysStatus _frIsNotInUse(uval ftoken)=0;
    virtual SysStatus _startFillPage(uval addr, uval offset)=0;
    virtual SysStatus _startWrite(uval addr, uval offset, uval size)=0;
    virtual SysStatus _startIO()=0;
    DEFINE_GLOBAL_NEW(StubFileHolder);
};

template<class PageServerStub> class StubFileHolderImp :
    public StubFileHolder {
protected:
    PageServerStub stub;
public:
    StubFileHolderImp (ObjectHandle oh) : stub(StubObj::UNINITIALIZED)
	{ stub.setOH(oh); };
    DEFINE_GLOBAL_NEW(StubFileHolderImp);

    virtual XHandle getXHandle() { return stub.getOH().xhandle(); }

    virtual SysStatus _frIsNotInUse(uval fileToken) {
	return stub._frIsNotInUse(fileToken);
    }
    virtual SysStatus _startFillPage(uval addr, uval offset) {
	//err_printf("F 0x%lx 0x%lx 0x%lx\n", fileToken, addr, offset);
	TraceOSMemFRStartFillPage(addr, offset);
	return stub._startFillPage(addr, offset);
    }
    virtual SysStatus _startWrite(uval addr, uval offset, uval size) {
	TraceOSMemFRStartWrite(addr, offset, size);
	return stub._startWrite(addr, offset, size);
    }
    virtual SysStatus _startIO() {
	return stub._startIO();
    }
};
#endif

#endif /* #ifndef __STUB_FILE_HOLDER_H_ */
