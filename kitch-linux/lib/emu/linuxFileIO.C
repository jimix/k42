/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linuxFileIO.C,v 1.42 2004/06/14 20:32:54 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: emulates file IO calls
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <defines/experimental.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <io/IOForkManager.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

void
__k42_linux_fileFini (void)
{
    _FD::CloseAll();
}

class IOForkManagerLinux : public IOForkManager {
    DEFINE_GLOBAL_NEW(IOForkManagerLinux);
public:
    virtual SysStatusUval copyEntries(uval buf, uval size);
    virtual SysStatus preFork(XHandle target);
    static SysStatus ClassInit();
};

/* static */ SysStatus
IOForkManagerLinux::ClassInit()
{
    IOForkManager::obj =  new IOForkManagerLinux();
    tassertMsg((obj != NULL), "alloction of IOForkManagerLinux failed\n");
    return 0;
}
/* virtual */ SysStatus
IOForkManagerLinux::preFork(XHandle target)
{
    return DREFGOBJ(TheProcessRef)->lazyCopyState(target);
}

/* virtual */ SysStatusUval
IOForkManagerLinux::copyEntries(uval buf, uval size) {
    uval sz = sizeof(_FD::FDSet);
    if (size < sz) {
	return _SERROR(1463, 0, ENOMEM);
    }
    _FD::CopyReadyForLazy((_FD::FDSet *)buf);
    return _SRETUVAL(sz);
}

void
LinuxFileInit(uval iofmBufSize, uval iofmBufPtr)
{
    _FD::ClassInit((_FD::FDSet *)iofmBufPtr);
    IOForkManagerLinux::ClassInit();
}
