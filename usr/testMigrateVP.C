/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testMigrateVP.C,v 1.3 2005/06/28 19:48:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Tests VP migration.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/ResMgrWrapper.H>
#include <sys/systemAccess.H>

int
main(int argc, char **argv)
{
    NativeProcess();

    VPNum ppCount, newPP;
    SysStatus rc;

    ppCount = _SGETUVAL(DREFGOBJ(TheProcessRef)->ppCount());

    for (uval i = 0; i < 50; i++) {
	newPP = (KernelInfo::PhysProc() + 1) % ppCount;
	err_printf("attempting migration to processor %ld.\n", newPP);
	rc = DREFGOBJ(TheResourceManagerRef)->migrateVP(0, newPP);
	err_printf("migrateVP() returned 0x%lx.\n", rc);
    }

    newPP = (KernelInfo::PhysProc() + 1) % ppCount;
    err_printf("attempting migration while disabled.\n");
    Scheduler::Disable();
    (void) Scheduler::SetAllowPrimitivePPC(1);
    rc = DREFGOBJ(TheResourceManagerRef)->migrateVP(0, newPP);
    (void) Scheduler::SetAllowPrimitivePPC(0);
    Scheduler::Enable();
    err_printf("migrateVP() while disabled returned 0x%lx.\n", rc);

    return 0;
}
