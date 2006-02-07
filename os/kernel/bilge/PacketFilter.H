/****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketFilter.H,v 1.6 2002/10/10 13:08:31 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: kernel packet filter
 ****************************************************************************/

#ifndef __PACKET_FILTER_H_
#define __PACKET_FILTER_H_

#include <cobj/CObjRoot.H>
#include <io/PacketRingServer.H>
#include <meta/MetaPacketFilter.H>

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

#ifndef EXPORT_PACKETFILTER
#define EXPORT_PACKETFILTER
#include <io/PacketFilterCommon.H>
#endif /* #ifndef EXPORT_PACKETFILTER */

class PacketFilter;
typedef PacketFilter** PacketFilterRef;

class PacketFilter : public Obj {

public:

    static void ClassInit(VPNum vp);

    DEFINE_ACCESS_SERVER(
            PacketFilter,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    virtual SysStatus exportedXObjectListEmpty();

protected:

    DEFINE_GLOBAL_NEW(PacketFilter);

    PacketFilter() { /* empty body */ }

    PacketRingServerRef pRef;
    UserFilter filter;
    void *filterPrivate;

    virtual SysStatus init(ObjectHandle &prOH, ProcessID caller,
                           UserFilter &filter);

    virtual SysStatus destroy();

private:

    SysStatus bindFilter(ObjectHandle &prOH, ProcessID caller,
                         UserFilter &filter);

    SysStatus unbindFilter();

__xprivate:

    static SysStatus _Create(__in UserFilter &filter, __in ObjectHandle &prOH,
                             __out ObjectHandle &pfOH, __CALLER_PID caller);

    virtual SysStatus _rebindFilter(__in ObjectHandle &prOH,
                                    __CALLER_PID caller);

    virtual SysStatus _destroy();
};

#endif /* #ifndef __PACKET_FILTER_H_ */






