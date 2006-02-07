#ifndef __BASE_OBJ_H_
#define __BASE_OBJ_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BaseObj.H,v 1.54 2004/10/08 21:40:05 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stddef.h>		/* to provide size_t (for operator new) */
#include <sys/kinclude.H>
#include <sys/types.H>
#include <sys/SysStatus.H>
#include <sys/sysTypes.H>
#include <cobj/ObjectHandle.H>
#include <cobj/ObjectRefs.H>
#include <cobj/CObjRep.H>
#include <sys/errno.h>

typedef uval AccessRights;

class BaseObj : public CObjRep {
public:
    /* this kludge is used by, for example, SegmentHATPrivate.H
     * which needs to make a transEntry that is not in the table
     *
     * we do this to allow us to keep BaseObj::setRef() private
     */
#if 0
    class FakeRef {
	Obj* _transEntry;
    public:
	ObjRef makeRef(Obj* obj) {
	    _transEntry = obj;
	    ((BaseObj*)obj)->setRef(&_transEntry);
	    return &_transEntry;}
    };
#endif /* #if 0 */

protected:
    inline BaseObj()  { }

#ifdef NDEBUG
    inline ObjRef getRef() { return (ObjRef) CObjRep::getRef(); }
#else /* #ifdef NDEBUG */
    ObjRef getRef();
#endif /* #ifdef NDEBUG */

public:
    static void ClassInit(VPNum vp);

    // default cleanup/destroy routines that all inherinit; just does delete
    // destroy without attachment checking, do not redefine in derived class
    // defined in ObjDestrMgr.C
    SysStatus destroyUnchecked();

    // FIXME: garbage collect x objects on destruction
    virtual SysStatus destroy() { return destroyUnchecked(); }

    // make sure all destructors are virtual and called properly
    // let compiler generate the destructors, only declare it here
    // the important consequence of this is that delete gets passed the
    // correct size even if the pointer is of a base class type
    virtual ~BaseObj() { /* empty body */ }

    /*
     * Every clustered object should use DEFINE_???_NEWOBJ to get a valid
     * new operator.  We define the base new operator to fail - it should
     * never be called.
     */
    void* operator new(size_t size);


    virtual SysStatus getType(TypeID &/*id*/) {
	return _SERROR(1057, 0, EINVAL);
    }


    /*
     * This is the normal server definition, called within the server
     * that exports an object.  It is overwridden by servers which
     * need so specify clientData The whole convoluted structure of
     * giveAccess works around the fact that C++ prevents overriding
     * only one of a set of overloaded virtual functions.  Thus, all
     * the server giveAccess functions must be defined in the
     * DEFINE_ACCESS_SERVER macro, rather than just the ones that
     * change.  Thus, a different name here.
     *
     * To set clientData, write your own giveAccessSetClientData
     * and call giveAccessInternal with the value you want,
     * rather than the 0 that appears here.
     *
     * To free clientData, override handleXObjFree(XHandle xhandle)
     * and call XHandleTrans::SetBeingFreed
     *
     * Finally, in callbackroutine, free the storage pointed to by
     * xobj->getClientData().
     *
     * N.B. when callbackroutine is called, the clustered object is probably
     * already completely deleted.  The callbackroutine must be static.
     */
    virtual SysStatus giveAccessSetClientData(
	ObjectHandle &oh, ProcessID toProcID,
	AccessRights match, AccessRights nomatch,
	TypeID type = 0) {
	return giveAccessInternal(oh, toProcID, match, nomatch, type, 0);
    }

    virtual SysStatus giveAccessInternal(
	ObjectHandle &oh, ProcessID toProcID,
	AccessRights match, AccessRights nomatch,
	TypeID type, uval clientData);

    virtual SysStatus giveAccessByServer(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch, TypeID type);

    virtual SysStatus giveAccessByServer(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch);

    virtual SysStatus giveAccessByServer(ObjectHandle &oh, ProcessID toProcID,
					 TypeID type);

    virtual SysStatus giveAccessByServer(ObjectHandle &oh, ProcessID toProcID);

    /*
     * These are the routines called by a client of an object to pass
     * the capabiliby to an object they have access to to another client
     */
    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch,
					 TypeID type);

    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch);

    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID);
};

// used to simplify getting names of clustered objects for debugging
// -2 is hack for DNE
#define OBJNAME(cobj)	((SysStatus)(DREF(cobj)->getName()) == -2	\
			? "(deleted)"					\
			: DREF(cobj)->getName())

#define DEFINE_REFS(CLASS)				\
    inline CLASS##Ref getRef() {			\
	return (CLASS##Ref)BaseObj::getRef();		\
    }




#undef OBJNAME
template <class CObj>
inline char *
OBJNAME(CObj cobj)
{
    char *s;
    // major hack, but I don't want to include ot.H here, at least not sure
    //    s = DREF(cobj)->getName();
    s = (*cobj)->getName();
    return ((SysStatus)s == -2) ? "(deleted)" : s;
}



/* THE NEW TYPE SERVICE INTERFACE */

#define TYPE_DECL(cls,pcls)				\
public:                                                 \
    static SysStatus typeName(char *buf, uval buflen);	\
    static SysStatus isBaseOf(const TypeID ctypeid);	\
    static SysStatus isChildOf(const TypeID ctypeid);	\
    static TypeID    typeID();                          \
private:                                                \
    static  TypeID __typeid;


#define _TYPE_IMPL(cls,pcls,clsname,signature,preface)			     \
    preface								     \
    SysStatus cls::typeName(char *buf, uval buflen) {			     \
	return DREFGOBJ(TheTypeMgrRef)->typeName(typeID(),buf,buflen);    \
    }									     \
    preface								     \
    TypeID cls::typeID() {						     \
	if (__typeid == 0) {						     \
	    DREFGOBJ(TheTypeMgrRef)->registerType(pcls::typeID(),	     \
                                                     #clsname,signature,     \
						     __typeid);		     \
        }								     \
	return __typeid;						     \
    }									     \
    preface								     \
    SysStatus cls::isBaseOf(const TypeID ctypeid) {			     \
	if (__typeid == 0) {						     \
	    DREFGOBJ(TheTypeMgrRef)->registerType(pcls::typeID(),	     \
                                                     #clsname,signature,     \
						     __typeid);		     \
        }								     \
	return DREFGOBJ(TheTypeMgrRef)->isDerived(ctypeid,__typeid);	     \
    }									     \
    preface								     \
    SysStatus cls::isChildOf(const TypeID ctypeid) {			     \
	if (__typeid == 0) {						     \
	    DREFGOBJ(TheTypeMgrRef)->registerType(pcls::typeID(),	     \
                                                     #clsname,signature,     \
						     __typeid);		     \
        }								     \
	return DREFGOBJ(TheTypeMgrRef)->isDerived(__typeid,ctypeid);	     \
    }

#define TYPE_IMPL(cls,pcls,clsname,signature) \
    TypeID  cls::__typeid = 0;          /* uninitialized */		     \
    _TYPE_IMPL(cls,pcls,clsname,signature,)

#define TYPE_IMPL_TEMPLATE(cls,pcls,clsname,signature) \
    template<> TypeID cls<clsname>::__typeid=0;				     \
    _TYPE_IMPL(cls <T> ,pcls,clsname cls,signature, template <class T>)

#define _TYPE_IMPL_X(xcls, mcls, prefix) \
    prefix								\
    SysStatus xcls::XtypeName(char* buf, uval buflen) {			\
	return mcls::typeName(buf,buflen);				\
    }									\
    prefix								\
    TypeID xcls::XtypeID() { return mcls::typeID(); };

#define TYPE_IMPL_X_TEMPLATE(xcls, mcls)				\
	_TYPE_IMPL_X(xcls, mcls, template <class T>)

#define TYPE_IMPL_X(xcls, mcls)						\
	_TYPE_IMPL_X(xcls, mcls,)

#ifndef _IN_STUBGEN
#define DEFINE_ACCESS_METASERVER(CLASS,matchDefault,nomatchDefault)	\
virtual SysStatus giveAccessInternal(ObjectHandle &oh, ProcessID toProcID, \
				AccessRights match,			\
				AccessRights nomatch,			\
				TypeID type,				\
				uval clientData) {			\
    SysStatusUval ptr;							\
    SysStatusXHandle xref;						\
    if (type != 0) {							\
	ptr = DREFGOBJ(TheTypeMgrRef)->getTypeLocalPtr(type);		\
	tassert(ptr,							\
		err_printf("Type not registered, no pointer to Meta obj\n")); \
	xref = ((MetaObj*)ptr)->virtCreateXHandle((ObjRef)getRef(),	\
				    toProcID, match,nomatch,clientData);\
    } else {								\
        xref = CLASS::createXHandle((ObjRef)getRef(), toProcID,		\
		   		    match,nomatch,clientData);		\
    }									\
    if (_FAILURE(xref)) return xref;					\
    oh.initWithMyPID(_SGETXH(xref));					\
    return 0;								\
}									\
virtual SysStatus giveAccessByServer(ObjectHandle &oh, ProcessID toProcID) { \
	return giveAccessSetClientData(oh, toProcID,			\
	           (matchDefault), (nomatchDefault),0);			\
}									\
virtual SysStatus giveAccessByServer(ObjectHandle &oh,			\
				     ProcessID toProcID, TypeID type) { \
	return giveAccessSetClientData(oh, toProcID,			\
	           (matchDefault), (nomatchDefault), type);		\
}									\
virtual SysStatus giveAccessByServer(ObjectHandle &oh, ProcessID toProcID, \
				AccessRights match, AccessRights nomatch) { \
	return giveAccessSetClientData(oh, toProcID,			\
	           match, nomatch,0);					\
}									\
virtual SysStatus giveAccessByServer(ObjectHandle &oh, ProcessID toProcID,\
				AccessRights match, AccessRights nomatch, \
				TypeID type) {				\
	return giveAccessSetClientData(oh, toProcID,			\
	           match, nomatch, type);				\
}
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)	\
	DEFINE_ACCESS_METASERVER(Meta##CLASS, matchDefault, nomatchDefault)
#else /* #ifndef _IN_STUBGEN */
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#define DEFINE_ACCESS_METASERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifndef _IN_STUBGEN */

#endif /* #ifndef __BASE_OBJ_H_ */
