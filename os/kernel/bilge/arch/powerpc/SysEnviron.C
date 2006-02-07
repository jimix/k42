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
#include <bilge/LocalConsole.H>
#include <sys/time.h>

// 1 for TRUE, 0 for FALSE, 2 for undetermined
uval SysEnviron::OnMambo = 2;
uval SysEnviron::Thinwire = 2;
uval SysEnviron::OnHypervisor = 2;
uval SysEnviron::OnHardware = 2;

/* static */ void
SysEnviron::ClassInit(VPNum vp)
{
    if (0==vp) {
	if (SIM_MAMBO==KernelInfo::OnSim())	OnMambo = 1;
	else					OnMambo = 0;
	if (KernelInfo::OnHV())	OnHypervisor = 1;
	else			OnHypervisor = 0;
    }
}

// initialize kernel parameters.
// initialize thinwire in some cases.
//
// The kernel parameters can be read through thinwire, the mambo cut-through
// or from the boot image bootData section.  Currently we read from the
// bootData sections.
// On mambo thinwire is initialized after kernel paramters and only if
//    K42_MAMBO_USE_THINIRE is not set or is set to 1.
// On enviroments other than mambo thinwire is always started and the
//    environment variable K42_MAMBO_USE_THINWIRE is ignored.
/* static */ void
SysEnviron::InitThinwire(VPNum vp)
{
    tassertMsg(OnMambo!=2, "Class not initialized");

    SysStatus rc;

    if (0==vp) {
	if (1==OnHypervisor) {
	    Thinwire = 0;
	    turnOffThinwire();
	}
	else if (1==OnMambo) {
            // start thinwire conditionally
	    const char *envName = "K42_MAMBO_USE_THINWIRE";
	    char retVal[64];
	    rc = KBootParms::_GetParameterValue(envName, retVal, 64);
	    if (_SUCCESS(rc) && (strcmp(retVal, "0") == 0)) {
		err_printf("K42_MAMBO_USE_THINWIRE == %s; "
			   "KernelInit not starting thinwire\n", retVal);
		Thinwire = 0;
		turnOffThinwire();
	    } else {
		err_printf("K42_MAMBO_USE_THINWIRE == %s or is not set; "
			   "KernelInit starting thinwire\n", retVal);

		Thinwire = 1;
	    }
	} else {
	    // alway start thinwire if we are not under mambo
	    Thinwire = 1;
	}
    }

    if (Thinwire == 1) {
	ThinWireMgr::ClassInit(vp);
	Wire::ClassInit(vp);	// initialize thinwire

	// initialize the IP cutthrough
	ThinIP::ClassInit(vp,
			  new TWMgrChan(IPCUT_CHANNEL),
			  new TWMgrChan(IPSEL_CHANNEL));

	// reread paramaters from thinwire.  This make our debug env.
	// a little easier
	if (vp == 0 && OnMambo != 1) {
	    SysConsole->setConsole(new TWMgrChan(CONSOLE_CHANNEL));
	    const char *envName = "K42_REREAD_PARAM_FROM_THINWIRE";
	    char retVal[64];
	    rc = KBootParms::_GetParameterValue(envName, retVal, 64);
	    if (_SUCCESS(rc) && strncmp("true", retVal, 4)==0) {
		void * mykparms;
		err_printf("Rereading parameters from thinwire ...");
		rc = ThinIP::GetKParms(&mykparms);
		if (_SUCCESS(rc)) {
		    rc = KBootParms::_UpdateParameters(
			(uval)mykparms+sizeof(uval32));
		    if (_FAILURE(rc))
			err_printf("Failed to update parameter area\n");
		} else {
		    err_printf("Failed to get parameters from thinwire\n");
		}
	    }
	}
    }
}

/*static*/ SysStatus
SysEnviron::GetTimeOfDay(struct timeval &tv) {
    SysStatus rc;

    if (OnHypervisor) {
	/* really should do an RTAS call here, on HV, mambo and HW */
	rc = -1;
    } else if (1==Thinwire) {
	rc = ThinIP::GetThinTimeOfDay(tv);
    } else if (OnMambo) {
	/* similarly here it should ask mambo */
	rc = -1;
    } else {
	tassertWrn(0, "Don't know how to get the Time of Day\n");
	rc = -1;
    }

    return rc;
}

/*static*/ uval
SysEnviron::SuspendThinWireDaemon() {
    if (1==Thinwire)	return ThinWireMgr::SuspendDaemon();
    else		return 0;
}

/*static*/ uval
SysEnviron::RestartThinWireDaemon() {
    if (1==Thinwire)	return ThinWireMgr::RestartDaemon();
    else		return 0;
}
