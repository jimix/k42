#ifndef __ACCESS_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __ACCESS_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Access.H,v 1.21 2004/10/26 13:52:24 mostrows Exp $
 *****************************************************************************/

/* assume that key for supervisor is set to 0 and for user is set to 1 */
public:
    enum mode {
	noUserWriteSup = 0,
	readUserReadSup = 3,
	writeUserWriteSup = 2,
	// must be separate bit
	execute = 4,
	io_uncached = 8,
    };

    enum {
	readFault = 0,
	writeFault = DSIStatusReg::writeFault,
	segmentFault = DSIStatusReg::segMiss
    };

    static inline uval isSegmentFault(pageFaultInfo pfinfo)
    {return pfinfo&segmentFault;}

    static inline uval isWrite(mode access)
    {return RWPerms(access) != readUserReadSup;}

    static inline uval isWriteUser(mode access)
    {return RWPerms(access) == writeUserWriteSup;}

    static inline uval isWriteFault(AccessMode::pageFaultInfo pfinfo)
    {return pfinfo&writeFault;}

    static inline uval isCacheInhibit(mode access)
    {return access & io_uncached;}

    static inline uval makeReadOnly(mode &access)
    {
        // can't reduce to readonly?
	if (RWPerms(access) == noUserWriteSup) return 0;
	access = mode(readUserReadSup|(access&execute));
	return 1;
    }

    static inline uval verifyAccess(
	uval user, mode access, pageFaultInfo pfinfo)
    {
	// user true if user mode process, false if sup mode
	if (user) {
	    if (pfinfo & DSIStatusReg::writeFault)
		return (RWPerms(access) == writeUserWriteSup);
	    return (RWPerms(access) != noUserWriteSup);
	}

	if (pfinfo & DSIStatusReg::writeFault)
	    return (RWPerms(access) != readUserReadSup);

	return 1;			// sup can always read in pwrPC
    }
