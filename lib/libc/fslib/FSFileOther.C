/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004, 2005
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFileOther.C,v 1.6 2005/04/27 17:35:06 butrico Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: implementation of opening a pipe
 ****************************************************************************/

#include <sys/sysIncs.H>
#include "FSFileOther.H"
#include <stub/StubStreamServerPipe.H>
#include <stub/StubStreamServerSocket.H>

/* virtual */ SysStatus 
FSFileOtherPipe::openCreateServerFile(ServerFileRef &ref, uval oflag, 
				      ProcessID pid, ObjectHandle &oh, 
				      uval &useType, TypeID &type)
{
    SysStatus rc = 0;

    ref = 0;
    err_printf("%s creating half a pipe\n", __PRETTY_FUNCTION__);
    type = FileLinux_PIPE;
    if (otherOH.invalid()) {
	// Create connection to pipe server
	rc = StubStreamServerPipe::_Create(otherOH);
	_IF_FAILURE_RET_VERBOSE(rc);
    }
    
    StubStreamServerPipe stubSSP(StubObj::UNINITIALIZED);
    stubSSP.setOH(otherOH);
    
    uval accmode = O_ACCMODE & oflag;
    if (accmode == O_RDONLY) {
	err_printf("%s - read only\n", __PRETTY_FUNCTION__);
	rc = stubSSP._createReader(oh, pid);
    } else if (accmode == O_WRONLY) {
	err_printf("%s - write only\n", __PRETTY_FUNCTION__);
	rc = stubSSP._createWriter(oh, pid);
    } else {
	rc = _SERROR(2895, 0, ENOTSUP);
    }
    
    return rc;
}

/* virtual */ SysStatus 
FSFileOtherSocket::openCreateServerFile(ServerFileRef &ref, uval oflag, 
				      ProcessID pid, ObjectHandle &oh, 
				      uval &useType, TypeID &type)
{
    // semantics unknown for now 
    return _SERROR(2894,0,ENOTSUP);
}

/* virtual */ SysStatus
FSFileOtherSocket::bind(ObjectHandle serverSocketOH)
{
    SysStatus rc;

    // we are setting the oh 'other' to the socket which is bound to this
    // file.  Those connecting, potentially multiple, require that this 
    // field be already set.

    if (otherOH.invalid()) {
	otherOH.initWithOH(serverSocketOH);
	StubStreamServerSocket stubSSS(StubObj::UNINITIALIZED);
	stubSSS.setOH(otherOH);
	rc = stubSSS._bind();
    } else {
	// already bound return failure
	rc = _SERROR(0,0,EADDRINUSE);
    }
    
    return rc;
}

/* virtual */ SysStatus
FSFileOtherSocket::getSocketObj(ObjectHandle &oh, ProcessID pid)
{
    SysStatus rc;

    if (otherOH.invalid()) {
	rc = _SERROR(0,0, ECONNREFUSED);
    } else {
	StubStreamServerSocket stubSSS(StubObj::UNINITIALIZED);
	stubSSS.setOH(otherOH);
	rc = stubSSS._getSocketObj(oh, pid);
    }

    return rc;
}
