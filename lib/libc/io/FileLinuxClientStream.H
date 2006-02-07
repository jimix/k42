#ifndef __FILE_LINUX_CLIENT_STREAM_H_
#define __FILE_LINUX_CLIENT_STREAM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxClientStream.H,v 1.4 2001/10/05 21:47:36 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Intermediate object for stub-compiler
 * **************************************************************************/

#ifndef EXPORT_FILELINUXCLIENTSTREAM
#define EXPORT_FILELINUXCLIENTSTREAM
#include <io/GenState.H>
#endif /* #ifndef EXPORT_FILELINUXCLIENTSTREAM */

#include <io/FileLinux.H>

// This is a base class for Packet and Stream objects.
class FileLinuxClientStream : public FileLinux __xbase(Obj) {
__xprivate:
    // change in status, typically data has become available
    // the state passed is ored into availability in client, as
    // described in:
    //      "lib/libc/io/README.html#Blocking in application"
    virtual __async SysStatus _signalDataAvailable(GenState avail)=0;
};

#endif /* #ifndef __FILE_LINUX_CLIENT_STREAM_H_ */
