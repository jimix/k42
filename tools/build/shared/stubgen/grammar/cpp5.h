#ifndef __CPP5_H_
#define __CPP5_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: cpp5.h,v 1.11 2001/10/05 21:51:43 peterson Exp $
 *****************************************************************************/

#define CONSTRUCTOR	"construct"

/* define the special stuff we are looking for in a single mask */

#define TP_IN          (0x000)
#define TP_INOUT       (0x001)
#define TP_OUT         (0x002)
#define TP_IO_MASK     (0x00f)

#define TP_PTR         (0x010)    /* only: <tp>*  */
#define TP_REF         (0x020)    /* only: <tp>&  */
#define TP_PTRREF      (0x040)    /* only: <tp>*& */
#define TP_REFPTR      (0x080)    /* only: <tp>&* */
#define TP_PR_MASK     (0x0f0)

#define TP_NOARR_MASK  (0x0FF)

#define TP_ARRAY       (0x100)   /* special type */
#define TP_CONST       (0x200)
#define TP_CALLER      (0x400)
#define TP_XHANDLE     (0x800)

#define NO_CALL          'X'
#define VIRTUAL_CALL     'V'
#define STATIC_CALL      'S'
#define CONSTRUCTOR_CALL 'C'

extern char *class_name;
extern char sig[];
extern int  save_sig;
extern int  save_ftype;
extern char in_ppc_call;
extern int  lineno;
extern int  assume_type;
extern char last_id[];
extern int  expect_type;
extern int  stypemask;

extern void yyerror(const char *fmt, ... );
extern void yywarn (const char *fmt, ... );
#endif /* #ifndef __CPP5_H_ */
