#ifndef __KINCLUDE_H_
#define __KINCLUDE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kinclude.H,v 1.16 2001/10/05 21:48:36 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Defines the basic macroes to include machine specific files, as
 * well personaility specific files, there is a copy of this in each
 * of the personalities (currently BSD) with the personality specific
 * variant of these calls
 * **************************************************************************/

#define _KS1(x) #x
#define _KS(x) _KS1(x)

/* used to include a machine specific file */
#define __MINC(fl)      _KS(arch/TARGET_MACHINE/fl)

/* define standard macroes for defining functions that may be included
   by either C or C++  */
#if defined(__cplusplus)
#define __BEGIN_C_DECLS	extern "C" {
#define __END_C_DECLS	};
#else /* #if defined(__cplusplus) */
#define __BEGIN_C_DECLS
#define __END_C_DECLS
#endif /* #if defined(__cplusplus) */

#endif /* #ifndef __KINCLUDE_H_ */
