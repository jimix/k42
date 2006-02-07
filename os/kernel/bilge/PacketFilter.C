/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketFilter.C,v 1.3 2000/12/22 21:32:43 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for network packet filters
 ****************************************************************************/

#include "kernIncs.H"
#include <bilge/PacketFilter.H>
#include <meta/MetaPacketFilter.H>
#include <stub/StubPacketFilter.H>
#include <io/PacketRingServer.H>
#include <io/PacketFilterCommon.H>
#include <cobj/XHandleTrans.H>
#include <meta/MetaPacketRingServer.H>

extern SysStatus xio_bsd_filter_add(DeviceFilter *filter);
extern SysStatus xio_bsd_filter_remove(DeviceFilter *filter);

void
PacketFilter::ClassInit(VPNum vp)
{
    if (vp != 0) return;
    MetaPacketFilter::init();
}


/* static */ SysStatus
PacketFilter::_Create(__in UserFilter &filter, __in ObjectHandle &prOH, 
                      __out ObjectHandle &pfOH, __CALLER_PID caller)
{
    PacketFilter *pf = new PacketFilter;
    SysStatus rc;

    if (pf == NULL) {
        return _SERROR(1651, 0, EINVAL);
    }
    
    rc = pf->init(prOH, caller, filter);
    tassert(rc == 0, err_printf("PacketFilter::_Create failed\n"));

    rc = pf->giveAccessByServer(pfOH, caller);

    return rc;
}


/* virtual */ SysStatus
PacketFilter::init(ObjectHandle &prOH, ProcessID caller, UserFilter &ufilter)
{
    SysStatus rc;

    if (CObjRootSingleRep::Create(this) == NULL) {
        return _SERROR(1652, 0, EINVAL);
    }

    rc = bindFilter(prOH, caller, ufilter);

    return rc;
}

/* private */ SysStatus
PacketFilter::bindFilter(ObjectHandle &prOH, ProcessID caller, 
                         UserFilter &ufilter)
{
    SysStatus rc;
    ObjRef objRef;
    TypeID type;

    DeviceFilter devFilter;

    // Get generic objRef from object handle
    rc = XHandleTrans::XHToInternal(prOH.xhandle(), caller,
                                    MetaObj::attach, objRef, type);
    if (_FAILURE(rc)) return rc;

    // Check type of object handle
    if (!MetaPacketRingServer::isBaseOf(type)) {
        tassert(0, err_printf("PacketFilter::init - XHToInternal Failed\n"));
        return _SERROR(1653, 0, EINVAL);
    }

    pRef = (PacketRingServerRef)objRef;

    // Increments ref count on packet ring
    rc = DREF(pRef)->bindFilter();
    if (_FAILURE(rc)) return rc;

    filter = ufilter;
    
    devFilter.filter = &ufilter;
    devFilter.callback_arg = (uval)pRef;
    devFilter.callback = NULL;

    // Download filter
    rc = xio_bsd_filter_add(&devFilter);
    if (_FAILURE(rc)) {
      DREF(pRef)->unbindFilter();
      return _SERROR(1654, 0, EINVAL);
    }

    filterPrivate = devFilter.dev_private;

    return 0;
}


/* private */ SysStatus
PacketFilter::unbindFilter()
{
    DeviceFilter devFilter;

    devFilter.filter = &filter;
    devFilter.callback_arg = (uval)pRef;
    devFilter.callback = NULL;
    devFilter.dev_private = filterPrivate;

    xio_bsd_filter_remove(&devFilter);
    DREF(pRef)->unbindFilter();
    
    return 0;
}


/* virtual */ SysStatus
PacketFilter::destroy()
{
    // remove all ObjRefs to this object
    SysStatus rc=closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if(_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    
    unbindFilter();

    destroyUnchecked();

    return 0;
}


// Called when last xobject released see Obj.H
/* virtual */ SysStatus 
PacketFilter::exportedXObjectListEmpty()
{
    return destroy();
}

/* virtual */ SysStatus
PacketFilter::_rebindFilter(__in ObjectHandle &prOH, __CALLER_PID caller)
{
    cprintf("*** Warning: _rebindFilter not implemented\n");
    return _SERROR(1655, 0, EINVAL);
}


/* virtual */ SysStatus
PacketFilter::_destroy()
{
    return destroy();
}


