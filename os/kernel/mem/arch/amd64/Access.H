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
 * $Id: Access.H,v 1.9 2003/01/17 16:20:15 marc Exp $
 *****************************************************************************/

// VVV

/* I would like to  be able to include mmu.H but ...
 * hence the value below are hard coded as enum instead of relying on PTE info.
 * #include <mmu.H>
 */

public:
    enum mode {
//	noUserWriteSup = PTE::RW_bit,
	noUserWriteSup = 2,
//	readUserReadSup = PTE::US_bit,
	readUserReadSup = 4,
//	writeUserWriteSup = PTE::US_bit+PTE::RW_bit,
	writeUserWriteSup = 6,
//	noUserReadSup = 0
	// must be separate bit
	execute = 8
    };

    enum {
	readFault = 0,
//	writeFault = PTE::writeFault
	writeFault = 2
    };


    static inline uval isSegmentFault(pageFaultInfo pfinfo)
    {
      return 1;
    }

    static inline uval isWrite(mode access)
//    {return access & PTE::RW_bit;}
    {
      return access & 2;
    }

    static inline uval isWriteUser(mode access)
    {return RWPerms(access) == writeUserWriteSup;}

    static inline uval isWriteFault(pageFaultInfo pfinfo)
//    {return (pfinfo & writeFault);}
    {
      return (pfinfo & 2);
    }

    static inline uval makeReadOnly(mode &access)
//    {access = (mode) (access & ~PTE::RW_bit); return 1;}
    {access = (mode) (access & ~2); return 1|(access&8);}

    static inline uval verifyAccess(
	uval user, mode access, pageFaultInfo pfinfo)
    {
        uval retvalue;
/*
	retvalue = (((!(access & PTE::US_bit)) && user) ||
	    ((!(access & PTE::RW_bit)) && (pfinfo & PTE::writeFault))) ?
	    0:1;
 */
	retvalue = (((!(access & 0x4)) && user) ||
	    ((!(access & 2)) && (pfinfo & 2))) ?
	    0:1;
	return(retvalue);
    }











