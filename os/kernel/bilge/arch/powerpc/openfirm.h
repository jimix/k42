#ifndef __OPENFIRM_H_
#define __OPENFIRM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: openfirm.h,v 1.19 2004/02/27 17:14:28 mostrows Exp $
 *****************************************************************************/

__BEGIN_C_DECLS

sval32 rtas_display_character(sval32 value);
sval32 rtas_shutdown(sval32 mask_hi, sval32 mask_lo);
sval32 rtas_system_reboot();
sval32 rtas_get_time_of_day(uval& year, uval& month, uval& day,
			    uval& hour, uval& minute, uval& second,
			    uval& nonoseconds);
sval32 rtas_set_time_of_day(uval year, uval month, uval day,
			    uval hour, uval minute, uval second,
			    uval nonoseconds);

void   rtas_init(uval virtBase);
sval32 rtasBootTrace(sval32 state);

#define SIMOS_RTASCALL .long 0x7C0007CA


/*	$NetBSD: openfirm.h,v 1.1 1996/09/30 16:35:04 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996, 2000 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Prototypes for Openfirmware Interface Routines
 */

#if 0 /* OF real-mode */
extern int ofstdin, ofstdout;
extern uval32 ofBufLen;

extern int *ofVirtArgs;
extern uval32 ofRealArgs;
extern void *ofVirtBuf;
extern uval32 ofRealBuf;

void initOFArena(char *virtFloor, uval virtBase);

sval32 OF_finddevice (uval32 dname);
sval32 OF_instance_to_package (sval32 ihandle);
sval32 OF_getprop    (sval32 phandle, uval32 prop, void *buf, sval32 buflen);
sval32 OF_getproplen (sval32 phandle, uval32 prop);
sval32 OF_open       (uval32 dname);
void   OF_close      (sval32 ihandle);
sval32 OF_read       (sval32 ihandle, void *addr, sval32 len);
sval32 OF_write      (sval32 ihandle, void *addr, sval32 len);
sval32 OF_seek       (sval32 ihandle, uval32 pos);
uval32 OF_claim      (uval32 virt, uval32 size, uval32 align);
void   OF_release    (uval32 virt, uval32 size);
sval32 OF_milliseconds (void);
sval32 OF_start_cpu  (sval32 phandle, uval32 pc);
void   OF_chain      (uval32 addr, uval32 size, uval32 entry,
		      uval32 parm, uval32 parmlen);
void   OF_enter      (void);
void   _quit         (void);

typedef struct {
    uval32 floor;
    uval32 chosen;
    uval32 close;
    uval32 enter;
    uval32 exit;
    uval32 finddevice;
    uval32 getprop;
    uval32 getproplen;
    uval32 loadbase;
    uval32 memory;
    uval32 milliseconds;
    uval32 options;
    uval32 open;
    uval32 read;
    uval32 reg;
    uval32 realbase;
    uval32 realsize;
    uval32 serial;
    uval32 stdin;
    uval32 stdout;
    uval32 startcpu;
    uval32 virtbase;
    uval32 virtsize;
    uval32 write;
    uval32 K;
    uval32 welcome;
    uval32 end;
} ofRD;

extern struct ofRD ofRealDict;
#endif /* #if 0 */

__END_C_DECLS

#endif /* #ifndef __OPENFIRM_H_ */
