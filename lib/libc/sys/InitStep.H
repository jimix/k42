#ifndef __INIT_STEP_H_
#define __INIT_STEP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: InitStep.H,v 1.2 2003/12/03 18:52:49 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: An interface for registering and invoking a step of the
 *		       system initialization process.
 *		       InitStep objects are called by InitServer, when the
 *		       time is right.
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#ifndef _IN_STUBGEN
#include <meta/MetaInitStep.H>
#else /* _IN_STUBGEN */
// FIXME: fix stub-compiler grammar so don't need to do this
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class InitStep: public Obj {

public:
    static void ClassInit();

    DEFINE_GLOBALPADDED_NEW(InitStep);
    DEFINE_ACCESS_SERVER(InitStep, MetaObj::controlAccess|MetaObj::attach,
			 MetaObj::none);
public:
    virtual SysStatus exportedXObjectListEmpty();
    virtual SysStatus init(uval id);
    virtual SysStatus action() = 0;
__xpublic:
    virtual __async SysStatus _doAction(ObjectHandle reply, __XHANDLE xh);
};

#define INIT_OBJECT(name, id)			\
struct name: public InitStep {			\
    DEFINE_GLOBAL_NEW(name);			\
    name() { init(id); };			\
    virtual SysStatus action();			\
};

#define INIT_OBJECT_PTR(type, name, id, fn)	\
struct type: public InitStep {			\
    DEFINE_GLOBAL_NEW(type);			\
    type() { init(id); };			\
    virtual SysStatus action() {		\
	fn();					\
	err_printf("Started " name "\n");	\
	return 0;				\
    }						\
};




#endif /* #ifndef __INIT_STEP_H_ */
