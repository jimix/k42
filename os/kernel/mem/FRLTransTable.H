#ifndef __FRLTRANSTABLE_H_
#define __FRLTRANSTABLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRLTransTable.H,v 1.3 2003/03/30 18:30:55 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to applications for files in memory manager
 * **************************************************************************/

#include "mem/FRPlaceHolder.H"
#include <meta/MetaFRLTransTable.H>

class FRLTransTable;
typedef FRLTransTable **FRLTransTableRef;

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class FRLTransTable : public FRPlaceHolder __xbase(FR){
protected:
    DEFINE_GLOBAL_NEW(FRLTransTable);
    DEFINE_REFS(FRLTransTable);

public:
    DEFINE_ACCESS_SERVER(
	FRLTransTable,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    static void ClassInit(VPNum vp);

    virtual SysStatus getType(TypeID &id);

    SysStatus destroy() { 
        return FRCommon::destroy();
    }

    virtual SysStatus locked_getFCM(FCMRef &r);

    //FIXME - for now anyway FRPlaceHolder doesn't support destroy logic
    virtual SysStatus fcmNotInUse() {
	return FRCommon::fcmNotInUse();
    }
    
__xpublic:
    static SysStatus _Create(__out ObjectHandle &frOH, __in uval defaultObject,
                             __CALLER_PID caller);

};

#ifndef _IN_STUBGEN
class FRLTransTablePinned : public FRLTransTable{
protected:
    DEFINE_PINNEDGLOBAL_NEW(FRLTransTablePinned);
public:
    static SysStatus Create(FRRef &frRef, uval defaultObject);
};
#endif

#endif /* #ifndef __FRLTRANSTABLE_H_ */
