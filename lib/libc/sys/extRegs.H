#ifndef __EXT_REGS_H_
#define __EXT_REGS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: extRegs.H,v 1.2 2004/04/06 21:00:32 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: This file defines a logical extension to the register
 *                     set implemented using a processor-specific read-write
 *                     page mapped into every address space.
 * **************************************************************************/

#include __MINC(extRegs.H)

struct Dispatcher;		// to avoid circular dependencies

struct ExtRegsCore : ExtRegsArch {
    uval disabled;
    Dispatcher *dispatcher;
    uval ppcPageLength;
};

const uval EXT_REGS_SIZE = PAGE_SIZE;
const uval PPCPAGE_LENGTH_MAX = EXT_REGS_SIZE - sizeof(ExtRegsCore);

struct ExtRegs : ExtRegsCore {
    char ppcPageData[PPCPAGE_LENGTH_MAX];
};

extern ExtRegs extRegsLocal;

#define PRESERVE_PPC_PAGE() \
    char *savedPPCPage = NULL; \
    if (extRegsLocal.ppcPageLength > 0) { \
	uval chklen = extRegsLocal.ppcPageLength; \
	if (chklen > PPCPAGE_LENGTH_MAX) chklen = PPCPAGE_LENGTH_MAX; \
	savedPPCPage = (char *) alloca(sizeof(uval) + chklen); \
	*((uval *) savedPPCPage) = extRegsLocal.ppcPageLength; \
	memcpy(savedPPCPage+sizeof(uval), extRegsLocal.ppcPageData, chklen); \
	memset(extRegsLocal.ppcPageData, 0, chklen); \
	extRegsLocal.ppcPageLength = 0; \
    }

#define RESTORE_PPC_PAGE() \
    tassertSilent(extRegsLocal.ppcPageLength == 0, BREAKPOINT); \
    if (savedPPCPage != NULL) { \
	extRegsLocal.ppcPageLength = *((uval *) savedPPCPage); \
	uval chklen = extRegsLocal.ppcPageLength; \
	if (chklen > PPCPAGE_LENGTH_MAX) chklen = PPCPAGE_LENGTH_MAX; \
	memcpy(extRegsLocal.ppcPageData, savedPPCPage+sizeof(uval), chklen); \
    }

#endif /* #ifndef __EXT_REGS_H_ */
