#ifndef __FACTORY_H_
#define __FACTORY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Factory.H,v 1.8 2005/08/09 12:03:06 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: base class for all type factories.
 *
 * How to convert a class to use a factory:
 *      1. declare a nested factory class with:
 *              public:
 *                  DEFINE_FACTORY_BEGIN(Factory)
 *                      // virtual methods performing object creation here, eg:
 *                      virtual MyClassRef create(args...);
 *                  DEFINE_FACTORY_END(Factory)
 *      2. provide implementations for your virtuals, making sure you call
 *         registerInstance and deregisterInstance as appropriate
 *      3. in a corresponding C file for the class put:
 *              DECLARE_FACTORY_STATICS(MyClass::Factory);
 *      4. change all MyClass::Create() calls to:
 *              DREF_FACTORY_DEFAULT(MyClass)->create(args...)
 *      5. ensure that MyClass::Factory::ClassInit(vp) runs before any creates
 * **************************************************************************/

#include <cobj/CObjRep.H>
#include <cobj/CObjRootMultiRep.H>
#include <misc/Callback.H>
#include <misc/ListSimpleLocked.H>

DEFINE_REFTYPE(Factory);
class Factory : public CObjRep {
public:
    virtual SysStatusUval getVersion() = 0;
    
    /* standard method for updating old instances we're tracking */
    virtual SysStatus updateOldInstances()=0;
    virtual SysStatus createReplacement(CORef ref, CObjRoot *&root);
    virtual SysStatus takeOverFromFac(FactoryRef factoryRef)=0;
    virtual SysStatus printInstances()=0;
    virtual void switchCompleteCallback(CORef instance, SysStatus success)=0;
};

class BaseFactory : public Factory {
protected:
    class UpdateThread;
    class DataTransferObj;
    class Root : public CObjRootMultiRep {
        friend class BaseFactory;
    protected:
        FactoryRef *myStaticRefPtr;
        BaseFactory *repArray[Scheduler::VPLimit];
        Callback *updateCallback;
        uval updateThreadCount;

        void init();
        Root(CORef *refPtr, sval c=1) :
            CObjRootMultiRep(c), 
            myStaticRefPtr((FactoryRef *)refPtr),
	    updateCallback(NULL) { init(); }
        Root(RepRef ref, CORef *refPtr, sval c=1,
             CObjRoot::InstallDirective idir=CObjRoot::Install,
	     Callback *cb = NULL)
            : CObjRootMultiRep(ref, c, idir), 
              myStaticRefPtr((FactoryRef *)refPtr),
	      updateCallback(cb) { init(); }
        void recordRep(BaseFactory *rep, VPNum vp);
    public:
        BaseFactory * lookupRep(VPNum vp);
        virtual SysStatus getDataTransferExportSet(DTTypeSet *set);
        virtual SysStatus getDataTransferImportSet(DTTypeSet *set);
        virtual DataTransferObject *dataTransferExport(DTType dtt,
						       VPSet transferVPSet);
        virtual SysStatus dataTransferImport(::DataTransferObject *dtobj,
					     DTType dtt, VPSet transferVPSet);
    };

    Root *root() { return (Root *)myRoot; }

    // we cannot use local strict here right now since removal may
    // occur on any processor
    typedef ListSimpleLocked<CORef, AllocGlobal> InstanceList;
    InstanceList instanceList, *oldInstanceList;

    DEFINE_LOCALSTRICT_NEW(BaseFactory);
    BaseFactory(): instanceList(), oldInstanceList(NULL) { /* empty body */ }

    // used internally
    class SwitchCallback;
    static SysStatus updateThreadFunc(void *arg);
    virtual BaseFactory *lookupRep(CORef ref);
    virtual void switchCompleteCallback(CORef instance, SysStatus success);

public:
    // must be called from factory by whatever creates instances
    virtual SysStatus registerInstance(CORef ref)
    {
	instanceList.add(ref);
	return 0;
    }
    // must be called on creating factory by destructor
    virtual SysStatus deregisterInstance(CORef ref);

    virtual SysStatus updateOldInstances();
    // FIXME : this is a big hack ... must really think about the
    //         implications of having mulitple refs especially on 
    //         destruction... but for the moment we blindly and boldly
    //         go ahead and install ourselves on a new ref.
    //         Think about how to make this a first class thing...eg.
    virtual SysStatus takeOverFromFac(FactoryRef factoryRef) {
        // FIXME: Sorry I am not worrying about versioning here... Just do it!
        //        ;-)
        return COSMgr::switchCObj((CORef)factoryRef, myRoot, 
                                  COSMgr::REASSIGN_WHEN_DONE);                    
    }
    virtual SysStatus printInstances();
    virtual ~BaseFactory();
};

/* Macros used to define a subclass of Factory for the factory mechanism.
 * Unfortunately we need to define a bunch of static members and methods, that
 * can't be inherited, so these are quite large.
 *
 * FIXME: could this be cleaner using templates?
 */

#define DEFINE_FACTORY_ROOT(FACTORY, ROOT)                              \
    class ROOT : public BaseFactory::Root {                             \
        friend class FACTORY;                                           \
    protected:                                                          \
        DEFINE_GLOBAL_NEW(ROOT);                                        \
        ROOT(CORef *refPtr, sval c=1): BaseFactory::Root(refPtr,c)      \
                                             { /* empty body */ }       \
        ROOT(RepRef ref, CORef *refPtr, sval c=1,                       \
             CObjRoot::InstallDirective idir=CObjRoot::Install,		\
	     Callback *cb = NULL) :       				\
            BaseFactory::Root(ref, refPtr, c, idir, cb) { /* empty */ } \
    public:                                                             \
        virtual CObjRep *createRep(VPNum vp) {                          \
              CObjRep *rep=new FACTORY;                                 \
              recordRep((BaseFactory *)rep,vp);                         \
              return rep;                                               \
        }                                                               \
    }

#define DEFINE_FACTORY_BEGIN_VERSION(FACTORY, VERSION)                  \
    DEFINE_REFTYPE(FACTORY);                                            \
    class FACTORY : public BaseFactory {                                \
    protected:                                                          \
        DEFINE_FACTORY_ROOT(FACTORY, Root);                             \
        static FACTORY##Ref theFactoryRef;                              \
	static const uval version = VERSION;				\
    public:                                                             \
        static FACTORY##Ref getFactoryRef() { return theFactoryRef; }   \
        static void ClassInit(VPNum vp);                                \
        static SysStatus Update(CORef factoryRef, Callback *cb = NULL);	\
	virtual SysStatusUval getVersion() { return _SRETUVAL(version); }

#define DEFINE_FACTORY_BEGIN(FACTORY) DEFINE_FACTORY_BEGIN_VERSION(FACTORY, 0)

#define DEFINE_FACTORY_END(FACTORY)                                     \
    };

/* these macros are used to provide a declaration for the static members,
 * the second version adds the typename qualifier, needed for a templated
 * class */
#define DECLARE_FACTORY_CLASSINIT(FACTORY)                              \
    /* static */ void                                                   \
    FACTORY::ClassInit(VPNum vp)                                        \
    {                                                                   \
        if (vp == 0) {                                                  \
            theFactoryRef = (FACTORY##Ref) (new                         \
                                  Root((CORef *)&theFactoryRef))        \
                                                       ->getRef();      \
        }                                                               \
    }

#define DECLARE_TEMPLATE_FACTORY_CLASSINIT(FACTORY)                     \
    /* static */ void                                                   \
    FACTORY::ClassInit(VPNum vp)                                        \
    {                                                                   \
        if (vp == 0) {                                                  \
            theFactoryRef = (typename FACTORY##Ref)                     \
                (new Root((CORef *)&theFactoryRef))->getRef();          \
        }                                                               \
    }

#define DECLARE_FACTORY_UPDATE(FACTORY)                                 \
    /* static */ SysStatus                                              \
    FACTORY::Update(CORef factoryRef, Callback *cb /* = NULL */)        \
    {                                                                   \
        SysStatusUval rcversion;                                        \
        rcversion = DREF((FactoryRef)factoryRef)->getVersion();         \
        tassertMsg(_SUCCESS(rcversion), "rc 0x%lx\n", rcversion);       \
        _IF_FAILURE_RET(rcversion);                                     \
	if (_SGETUVAL(rcversion) != version - 1) {                      \
	    tassertMsg(0, "attempt to update wrong version factory\n");	\
	    return -1;							\
	}								\
        CObjRoot *root = new Root((RepRef)factoryRef,                   \
                                  (CORef *)&theFactoryRef, 1,           \
                                  CObjRoot::skipInstall, cb);           \
        return COSMgr::switchCObj(factoryRef, root);                    \
    }

#define DECLARE_FACTORY_STATICS(FACTORY)                                \
    FACTORY##Ref FACTORY::theFactoryRef                                 \
            = (FACTORY##Ref) GOBJ(TheBreakpointObjectRef);              \
    DECLARE_FACTORY_CLASSINIT(FACTORY)                                  \
    DECLARE_FACTORY_UPDATE(FACTORY)

#define DECLARE_TEMPLATED_FACTORY_STATICS(TEMPLATES, FACTORY)           \
    template TEMPLATES                                                  \
    typename FACTORY##Ref FACTORY::theFactoryRef(NULL);                 \
    template TEMPLATES                                                  \
    DECLARE_TEMPLATE_FACTORY_CLASSINIT(FACTORY)                         \
    template TEMPLATES                                                  \
    DECLARE_FACTORY_UPDATE(FACTORY)

/* macro for accessing the factory which created an instance */
#define DREF_FACTORY(REPREF)        DREF(DREF(REPREF)->getFactoryRef())

/* macro akin to DREFGOBJ, calls the current factory for a class */
#define DREF_FACTORY_DEFAULT(CLASS) DREF(CLASS::Factory::getFactoryRef())

#endif /* #ifndef __FACTORY_H_ */
