#ifndef __DEV_OPENER_H_
#define __DEV_OPENER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevOpener.H,v 1.2 2004/02/27 17:14:20 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Stub interface used by DevFSRemote to perform open()
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#ifndef EXPORT_DEVOPENER
#define EXPORT_DEVOPENER
#include <sys/types.h>
#endif //EXPORT_DEVOPENER

#include <cobj/Obj.H>
#include <cobj/ObjectHandle.H>

class DevOpener: public Obj {
__xpublic:
    virtual SysStatus _open(__out ObjectHandle &oh,
			    __out TypeID &type,
			    __in  ProcessID pid,
			    __in  uval oflag,
			    __in  uval token);
};


#endif /* #ifndef __DEV_OPENER_H_ */
