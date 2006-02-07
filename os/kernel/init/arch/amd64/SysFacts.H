#ifndef __AMD64_SYS_FACTS_H_
#define __AMD64_SYS_FACTS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysFacts.H,v 1.3 2001/10/05 21:49:42 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine specific component of SysFacts.H
 * Machine Dependent Part: amd64
 * **************************************************************************/

#ifdef INCLASS				// additions to class
    uval    numProc;
    struct  physProc {
	// FIXME:  id shoud be of type apicId see APIC.H
	//         However it would be a pain in the a%#
	//         to include that here.  Please figure out
	//         the right way to handle this.
        uval8 id;
    }      *phyProc;
    uval    bootProc;
    uval8 procId(const uval proc);
#else /* #ifdef INCLASS				// additions to class */
					// pure methods must be outside of
					// class but in .H so each compile sees
					// them. .H is included twice by

extern SysFacts::physProc *ThePhysProcArray;
#endif /* #ifdef INCLASS				// additions to class */
#endif /* #ifndef __AMD64_SYS_FACTS_H_ */
