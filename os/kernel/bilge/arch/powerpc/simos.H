#ifndef __SIMOS_H_
#define __SIMOS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simos.H,v 1.24 2005/02/09 18:45:43 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: services built on top of simSupport.
 * **************************************************************************/

#include <init/MemoryMgrPrimitiveKern.H>
#include <misc/arch/powerpc/simSupport.H>
#include <sys/IOChan.H>

// FIXME where does this really belong
/* physmemcpy (destination source length) */
void *physmemcpy(void * t, const void * s, size_t len);

uval bStrLen (char *s);
void bStrCat (char *s, char *t);
void bStrCpy(char *s, char *t);

char *printHex (char *s, unsigned long long v);

void bprintf(char *buf);
void b1printf(char *buf, uval val);

void simosConsoleWrite(char *buf, uval length);
sval simosReadCharStdin();

void simosThinIPInit(MemoryMgrPrimitiveKern *memory);
sval simosThinIPWrite(const char *buf, uval length);
sval simosThinIPRead(char *buf, uval length);

IOChan* getSimConChan(MemoryMgrPrimitive *memory);
IOChan* getSimTWChan(MemoryMgrPrimitive *memory);
#endif /* #ifndef __SIMOS_H_ */
