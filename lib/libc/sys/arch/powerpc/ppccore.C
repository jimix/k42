/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppccore.C,v 1.32 2001/07/10 20:45:39 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    Test implementation of the PPC
 *    TypeMgr  / temporary implementation only
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <stub/StubBaseObj.H>
#include <meta/MetaBaseObj.H>
#include <sys/ppccore.H>
#include <cobj/TypeMgr.H>
#include <cobj/XHandleTrans.H>
#include <scheduler/Thread.H>
#include <sys/Dispatcher.H>

MetaBaseObj::MetaBaseObj()
{
//    This is now valid --- need to make one to build a virtual table
//    tassert(0, err_printf("attempt to construct a meta object\n"));
}

void*
StubBaseObj::operator new(size_t size)
{
    return allocGlobal(size);
}

TypeID
StubBaseObj::__typeid = 0; /* uninitialized */

TypeID
StubBaseObj::typeID() {
    if (__typeid == 0)
	DREFGOBJ(TheTypeMgrRef)->registerType(TYPEID_NONE,"Obj",1,__typeid);
    return __typeid;
}

SysStatus
StubBaseObj::isBaseOf(const TypeID ctypeid) {
    if (__typeid == 0)
	DREFGOBJ(TheTypeMgrRef)->registerType(TYPEID_NONE,"Obj",1,__typeid);
    return DREFGOBJ(TheTypeMgrRef)->isDerived(ctypeid,__typeid);
}

TypeID
MetaBaseObj::__typeid = 0; /* uninitialized */

TypeID
MetaBaseObj::typeID() {
    if (__typeid == 0)
	DREFGOBJ(TheTypeMgrRef)->registerType(TYPEID_NONE,"Obj",1,__typeid);
    return __typeid;
}

SysStatus
MetaBaseObj::isBaseOf(const TypeID ctypeid) {
    if (__typeid == 0)
	DREFGOBJ(TheTypeMgrRef)->registerType(TYPEID_NONE,"Obj",1,__typeid);
    return DREFGOBJ(TheTypeMgrRef)->isDerived(ctypeid,__typeid);
}
