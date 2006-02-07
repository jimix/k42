/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A layer which keeps track to certain choices on
		       services enablement and direct services
                       request accordingly.
 * **************************************************************************/

#include "kernIncs.H"
#include "SysEnviron.H"
#include <sys/KernelInfo.H>
#include <bilge/arch/powerpc/BootInfo.H>
#include <bilge/arch/powerpc/simos.H>
#include <bilge/Wire.H>
#include <bilge/ThinIP.H>
#include <bilge/ThinWireMgr.H>
#include <bilge/KBootParms.H>
#include <sys/time.h>

// 1 for TRUE, 0 for FALSE, 2 for undetermined
uval SysEnviron::OnMambo = 2;
uval SysEnviron::Thinwire = 2;
uval SysEnviron::OnHypervisor = 2;
uval SysEnviron::OnHardware = 2;

/* static */ void
SysEnviron::ClassInit(VPNum vp)
{
    // FIXME: implement for this architecture
}

// initialize kernel parameters. 
// initialize thinwire in some cases.
//
// On mambo kernel parameters are read via the mambo cut-through.
// On environments other than mambo kernel parameters are read via thinwire.
// On mambo thinwire is initialized after kernel paramters and only if
//    K42_MAMBO_USE_THINIRE is not set or is set to 1.
// On enviroments other than mambo thinwire is always started and the
//    environment variable K42_MAMBO_USE_THINWIRE is ignored.
/* static */ void
SysEnviron::InitThinwire(VPNum vp)
{
    tassertMsg(0, "Don't know how to initialize thinwire\n");
}

/*static*/ SysStatus
SysEnviron::GetTimeOfDay(struct timeval &tv) {
    tassertMsg(0, "Don't know how to get time of day\n");
    return -1;
}

/*static*/ uval
SysEnviron::SuspendThinWireDaemon() {
    tassertMsg(0, "Don't know how to suspend thinwire\n");
    return 0;
}

/*static*/ uval
SysEnviron::RestartThinWireDaemon() {
    tassertMsg(0, "Don't know how to restart thinwire\n");
    return 0;
}
