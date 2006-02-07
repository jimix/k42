/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ObjectHandle.C,v 1.9 2005/04/15 17:39:33 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for ObjectTranslation object
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/ObjectHandle.H>
#include <cobj/XHandleTrans.H>

void
ObjectHandle::initWithMyPID(XHandle xh)
{
    tassert(_SUCCESS(DREFGOBJ(TheXHandleTransRef)->xhandleValidate(xh)),
	    err_printf("Trying to convert invalid XHandle to ObjectHandle\n"));
    ProcessID p = DREFGOBJ(TheProcessRef)->getPID();
    _commID = SysTypes::COMMID(p, SysTypes::DSPID(0, SysTypes::VP_WILD));
    _xhandle = xh;
}
