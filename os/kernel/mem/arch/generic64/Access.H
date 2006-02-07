#ifndef __GENERIC64_ACCESS_H_
#define __GENERIC64_ACCESS_H_
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Access.H,v 1.5 2003/01/17 16:20:18 marc Exp $
 *************************************************************************** */

public:
    enum mode {
      noUserWriteSup,
      readUserReadSup,
      writeUserWriteSup,
      // must be separate bit
      execute = 4
    };

    enum {
      readFault,
      writeFault,
      segmentFault
    };


static inline uval
isSegmentFault(pageFaultInfo pfinfo)
{
  return(0);
}

static inline uval
isWrite(mode access)
{
  return(0);
}

static inline uval
isWriteUser(mode access)
{
    return 0;
}

static inline uval
isWriteFault(AccessMode::pageFaultInfo pfinfo)
{
  return(0);
}

static inline uval
makeReadOnly(mode &access)
{
  if (RWPerms(access) == noUserWriteSup) return 0; // can't reduce to readonly
  access = readUserReadSup|(access&execute);
  return 1;
}

static inline uval
verifyAccess(uval user, mode access, pageFaultInfo pfinfo)
{
  if (user) {
    if (isWriteFault(pfinfo)) {
      return (RWPerms(access) == writeUserWriteSup);
    }
    return RWPerms(access) != noUserWriteSup);
  }

  if (isWriteFault(pfinfo)) {
    return (RWPerms(access) != readUserReadSup);
  }

  return 1;			// sup can always read in pwrPC
}

#endif /* #ifndef __GENERIC64_ACCESS_H_ */
