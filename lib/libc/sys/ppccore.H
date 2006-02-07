#ifndef __PPCCORE_H_
#define __PPCCORE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppccore.H,v 1.61 2004/03/09 19:32:22 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: This file provides the interface (used mainly
 * by stub generated code) for PPC requests.
 * **************************************************************************/

/*FIXME
 *
 *There are missing checks in severside ("X") code, which would allow
 *a malicious client to crash the server or worse.
 *
 *Checks needed: __vtablen must be checked to fit in PPCPAGE.  This one
 *check will prevent crash caused by a crazy ALLOCA and crash caused by
 *a copy which runs off the end of the PPCPAGE.
 *
 *Lengths in sizearr for inoutbuf items need to be checked to avoid trying
 *to copy too many bytes.
 *
 *Both could be combined into a check that the sum of __vtablen and the
 *relevent sizearr entries, along with the inargs, fit in the ppcpage.
 *
 *There may be a server crash if the server fails to store anything in
 *an outbuf(*:len) parameter, because the code takes a strlen of the outbuf.
 *
 *Could be fixed by unconditionally setting the last character of the outbuf
 *to zero after the server method returns and before doing the strlen call
 */

#include <cobj/ObjectRefs.H>
#include <sys/syscalls.H>
#include <misc/alloca.H>
#include <cobj/TypeMgr.H>
#include __MINC(ppccore.H)
#include <sys/extRegs.H>

#define PPCPAGE_LENGTH (extRegsLocal.ppcPageLength)
#define PPCPAGE_DATA   (extRegsLocal.ppcPageData)

// different implementations in kernel and user level
void
PPC_ASYNC(SysStatus &rc, CommID targetID, XHandle xhandle, uval methnum);

/*
 *
 * The first uval of PPC page contains the number of bytes in use, NOT
 * counting itself.
 *
 * The LENGTH macros take or return the length of the PPC args.
 *
 * The length field must be set before any of the content it protects is
 * filled in.  Empty __asm() statements are used to keep the compiler from
 * reordering assignments to the length field with respect to subsequent
 * assignments to PPC page content, and to keep it from eliminating seemingly
 * dead stores to the length field.  No multiprocessor hardware
 * synchronization is necessary because the PPC page is accessed on only
 * one processor.
 */

#define GET_PPC_LENGTH() PPCPAGE_LENGTH

#define SET_PPC_LENGTH(len) \
    tassertSilent(PPCPAGE_LENGTH == 0, BREAKPOINT); \
    PPCPAGE_LENGTH = (len); \
    __asm ("#fence")

#define SET_PPC_LENGTH_CH(len) \
    tassertSilent(PPCPAGE_LENGTH == 0, BREAKPOINT); \
    if ((len) > PPCPAGE_LENGTH_MAX) goto __ppc_overflow; \
    PPCPAGE_LENGTH = (len); \
    __asm ("#fence")

#define INC_PPC_LENGTH(len) \
    PPCPAGE_LENGTH += (len); \
    __asm ("#fence")

#define INC_PPC_LENGTH_CH(len) \
{ \
    uval tmp_len = PPCPAGE_LENGTH + (len); \
    if (tmp_len > PPCPAGE_LENGTH_MAX) goto __ppc_overflow; \
    PPCPAGE_LENGTH = tmp_len; \
    __asm ("#fence"); \
}

#define ASSERT_PPC_EMPTY() \
    tassertSilent(PPCPAGE_LENGTH == 0, BREAKPOINT)

#define RESET_PPC_LENGTH() \
    __asm ("#fence"); \
    PPCPAGE_LENGTH = 0;

#define RESET_PPC() \
{ \
    uval len = PPCPAGE_LENGTH; \
    if (len > PPCPAGE_LENGTH_MAX) len = PPCPAGE_LENGTH_MAX; \
    memset(PPCPAGE_DATA, 0, len); \
    PPCPAGE_LENGTH = 0; \
}

#define RESET_PPC_OVERFLOW() \
{ \
    memset(PPCPAGE_DATA, 0, PPCPAGE_LENGTH_MAX); \
    PPCPAGE_LENGTH = 0; \
}

#define COPY_TO_PPC_PAGE(addr,size) memcpy(PPCPAGE_DATA,(addr),(size))

#define FITS_PPCPAGE(len) if ((len) > PPCPAGE_LENGTH_MAX) goto __ppc_overflow

/* STUB-GENERATOR  STRING FUNCTIONS
 * - Some notations
 *   SGS - StubGenerator-Stub
 *   SGX - StubGenerator-X
 */


#define SGALIGN(x)  (((x)+sizeof(uval)-1)&~(sizeof(uval)-1))
/* #define SGALIGN(x)  (x) */

/* COPY WITHOUT UPDATING __vtablen */
#define SGSU_PUT_FARRAY(name,numelem) 			\
{							\
    __len = sizeof(*(name))*(numelem);			\
    __alen = SGALIGN(__len);				\
    INC_PPC_LENGTH_CH(__alen);                          \
    memcpy((void*)__stab,(void*)name,__len);          	\
    (*(char**)&__inargs->name) = __stab;                \
    __stab += __alen;					\
    __inargs->__vtablen += __alen;                      \
}

#define SGSU_PUT_VARRAY(name,numelem)  SGSU_PUT_FARRAY(name,numelem)

#define SGSU_PUT_STR(name,idx)				\
{							\
    __len = strlen(name) + 1;		        	\
    __alen = SGALIGN(__len);				\
    INC_PPC_LENGTH_CH(__alen);                          \
    memcpy((void*)__stab,(void*)name,__len);          	\
    (*(char**)&__inargs->name) = __stab;                \
    __inargs->__sizearr[idx] = __len;			\
    __stab += __alen;					\
    __inargs->__vtablen += __alen;			\
}

/* COPY WITHOUT UPDATING __vtablen */
#define SGSN_PUT_FARRAY(name,numelem) 			\
{							\
    __len = sizeof(*(name))*(numelem);			\
    __alen = SGALIGN(__len);				\
    INC_PPC_LENGTH_CH(__alen);                          \
    memcpy((void*)__stab,(void*)name,__len);          	\
    (*(char**)&__inargs->name) = __stab;                \
    __stab += __alen;					\
}

#define SGSN_PUT_VARRAY(name,numelem) SGSN_PUT_FARRAY(name,numelem)

#define SGSN_PUT_STR(name,idx)				\
{							\
    __len = strlen(name) + 1;		        	\
    __alen = SGALIGN(__len);				\
    INC_PPC_LENGTH_CH(__alen);                          \
    memcpy((void*)__stab,(void*)name,__len);          	\
    (*(char**)&__inargs->name) = __stab;                \
    __inargs->__sizearr[idx] = __len;			\
    __stab += __alen;					\
}

/* get stuff from the PPC-PAGE */

#define SGS_GET_FARRAY(name,numelem)			\
    memcpy((void*)name,(void*)__outargs->name,		\
	   sizeof(*(__outargs->name))*(numelem))

#define SGS_GET_VARRAY(name,numelem)			\
    memcpy((void*)name,(void*)__outargs->name,		\
	   sizeof(*(__outargs->name))*numelem)

#define SGS_GET_STR(name,idx)				\
    memcpy((void*)name,(void*)__outargs->name,__outargs->__sizearr[idx])


/*******************************************************
 *               MACROS FOR THE XOBJ SIDE
 *******************************************************/

/* get stuff from the PPC-PAGE */
#define CHECK_OVERFLOW(val,vmax,argnum)				\
    if (val > vmax) {						\
	__ppc_rc = _SERROR(argnum,1,E2BIG);	                \
	goto __exit_xobj;					\
    }

#define SGX_GET_FARRAY(name,numelem,maxelem,argnum)		\
{								\
    CHECK_OVERFLOW(numelem,maxelem,argnum);			\
    __len = sizeof(*(__inargs.name))*(numelem);			\
    memcpy((void*)__lstab,(void*)__inargs.name,__len);		\
    (*(char**)&__outargs.name) = __lstab;			\
    __len = SGALIGN(__len);					\
    __stab += __len;						\
    __lstab += SGALIGN(sizeof(*(__inargs.name))*(maxelem));	\
}

#define SGX_GET_VARRAY(name,numelem,maxelem,argnum) 		\
{								\
    CHECK_OVERFLOW(numelem,maxelem,argnum);			\
    __len = sizeof(*(__inargs.name))*(numelem);			\
    memcpy((void*)__lstab,(void*)__inargs.name,__len);		\
    (*(char**)&__outargs.name) = __lstab;			\
    __len = SGALIGN(__len);					\
    __stab += __len;						\
    __lstab += SGALIGN(sizeof(*(__inargs.name))*(maxelem));	\
}

#define SGX_GET_STR(name,idx,maxlen,argnum)			\
{								\
    __len = __inargs.__sizearr[idx];				\
    CHECK_OVERFLOW(__len,maxlen,argnum);                        \
    memcpy((void*)__lstab,(void*)__inargs.name,__len);		\
    (*(char**)&__outargs.name) = __lstab;			\
    __len = SGALIGN(__len);					\
    __stab += __len;						\
    __lstab += SGALIGN(sizeof(*(__inargs.name))*(maxlen));	\
}

/* reserve space on the local variable len stack __lstab */

#define SGX_RES_FARRAY(name,maxelem)			        \
{								\
    (*(char**)&__outargs.name) = __lstab;			\
    __lstab += SGALIGN(sizeof(*(__outargs.name))*(maxelem));	\
}

#define SGX_RES_VARRAY(name,maxelem)			        \
{								\
    (*(char**)&__outargs.name) = __lstab;			\
    __lstab += SGALIGN(sizeof(*(__outargs.name))*(maxelem));	\
}

#define SGX_RES_STR(name,maxlen)				\
{								\
    (*(char**)&__outargs.name) = __lstab;			\
    __lstab += SGALIGN(sizeof(*(__outargs.name))*(maxlen));	\
}



/* Put stuff into the PPC-PAGE */

#define SGX_PUT_FARRAY(name,numelem) 			\
{							\
    __len = sizeof(*(__outargs.name))*(numelem);	\
    __alen = SGALIGN(__len);				\
    INC_PPC_LENGTH(__alen);                             \
    memcpy((void*)__stab,(void*)__outargs.name,__len);  \
    (*(char**)&__outargs.name) = __stab;                \
    __stab += __alen;					\
}

#define SGX_PUT_VARRAY(name,numelem) 			\
{							\
    if (numelem >= 0) {					\
     __len = sizeof(*(__outargs.name))*(numelem);	\
     __alen = SGALIGN(__len);				\
     INC_PPC_LENGTH(__alen);                            \
     memcpy((void*)__stab,(void*)__outargs.name,__len); \
     (*(char**)&__outargs.name) = __stab;               \
     __len = SGALIGN(__len);				\
     __stab += __alen;					\
    }                                                   \
}

#define SGX_PUT_STR(name,idx)				\
{							\
    __len = strlen(__outargs.name) + 1;			\
    __alen = SGALIGN(__len);				\
    INC_PPC_LENGTH(__alen);                             \
    memcpy((void*)__stab,(void*)__outargs.name,__len);  \
    (*(char**)&__outargs.name) = __stab;                \
    __outargs.__sizearr[idx] = __len;			\
    __stab += __alen;					\
}

#define SGX_ADJ_PTR(name)  (*(char**)&__inargs.name += __ptrdiff)

#endif /* #ifndef __PPCCORE_H_ */
