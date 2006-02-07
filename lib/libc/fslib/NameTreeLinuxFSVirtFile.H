#ifndef __NAME_TREE_LINUX_FS_VIRT_FILE_H_
#define __NAME_TREE_LINUX_FS_VIRT_FILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NameTreeLinuxFSVirtFile.H,v 1.6 2004/08/18 13:50:34 butrico Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Entry for name lookups for Virtual Files
 ****************************************************************************/

#include "NameTreeLinuxFS.H"

class NameTreeLinuxFSVirtFile : public NameTreeLinuxFS {
    uval printFailures;

protected:
    DEFINE_GLOBAL_NEW(NameTreeLinuxFSVirtFile);
public:
    NameTreeLinuxFSVirtFile(uval prf) {printFailures = prf;}

    // exported routines
    virtual SysStatus _getObj(__inbuf(namelen)char *name,
			      __in uval namelen,
			      __in uval oflag, __in uval mode,
			      __out ObjectHandle &oh,
			      __out TypeID &type,
			      __out uval &useType,
			      /* argument for simplifying gathering traces of
			       * file sharing information. This should go
			       * away. */
			      __out ObjRef &fref,
			      __CALLER_PID processID);

    virtual SysStatus _createVirtFile(
	__inbuf(namelen)char *name,
	__in uval namelen,
	__in uval mode,
	__in ObjectHandle& vfoh,
	__CALLER_PID processID);

    // inherited method from NameTreeLinuxFS is specialized
    static void Create(const char *pathToFS, const char *pathToMount,
		       uval isVolatile,
		       DirLinuxFSRef dir) {
	tassertWrn(pathToFS == NULL, "NameTreeLinuxFSVirtFile::Create: pathToFS "
		   "argument %s is being ignored\n", pathToFS);
	tassertWrn(isVolatile == 0, "NameTreeLinuxFSVirtFile::Create: isVolatile "
		   "argument %ld is being ignored\n", isVolatile);
	Create(pathToMount, dir);
    }

    // pathToMount is the path where the file system is being mounted on the
    // name space
    static void Create(const char *pathToMount, DirLinuxFSRef dir,
		       uval printFailures=0);
};
#endif /* #ifndef __NAME_TREE_LINUX_FS_VIRT_FILE_H_ */

