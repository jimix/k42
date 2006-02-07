#ifndef __FIVFACCESSOH_H_
#define __FIVFACCESSOH_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/

#include <io/FileLinux.H>
#include <fslib/ServerFile.H>
#include <io/VirtFile.H>
#include <time.h>
#include <cobj/CObjRootSingleRep.H>
#include <sync/FairBLock.H>
#include <fslib/virtfs/FileInfoVirtFS.H>

//
// A derivation of FileInfoVirtFS that is meant for situations where one
// wants a file in a VirtFS to exist for the purpose of being a place-
// holder for an OH.  We perform giveAccess calls on this OH to generate
// new ones to pass to FileLinux::Create

// Base class object that represents entries in a virtFS file system
class FIVFAccessOH : public FileInfoVirtFS{
protected:
    ObjectHandle obj;
    TypeID type;
public:
    DEFINE_GLOBAL_NEW(FIVFAccessOH);
    FIVFAccessOH() { };
    virtual ~FIVFAccessOH() {};
    virtual SysStatus init(ObjectHandle oh, TypeID ohType,
			   mode_t mode,  uval number=0) {
	type = ohType;
	obj = oh;
	FileInfoVirtFS::init(mode, number);
	return 0;
    }
    virtual uval getFSFileType() { return FSFILE_REMOTE; }
    virtual SysStatusUval getServerFileType() {
	// This object doesn't produce a ServerFile object
	return FileInfoVirtFS::None;
    }
    virtual SysStatus deleteFile();

    virtual SysStatus destroy();

    // Provides an OH that is returned to FileLinux::Create
    // May provide a ServerFile ref (but that may not be necessary
    // depending on "type" and the type of the OH.
    virtual SysStatus openCreateServerFile(ServerFileRef &fref,
					   uval oflag,
					   ProcessID pid,
					   ObjectHandle &oh,
					   uval &useType,
					   TypeID &type);

    virtual SysStatus createServerFileBlock(ServerFileRef &fref);
    virtual SysStatus createServerFileChar(ServerFileRef &fref);
};
#endif /* #ifndef __FIVFACCESSOH_H_ */
