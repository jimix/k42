#ifndef __TEMPLATE_BUGS_H_
#define __TEMPLATE_BUGS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: template_bugs.H,v 1.5 2001/10/05 21:49:24 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines various flags which drive workarounds
 * for current compiler bugs involving templates
 * **************************************************************************/

/*
 * GCC currently chokes on the following test program:
 *	---------------------------------------------
 *	class A {};
 *
 *	template<class B>
 *	class C {
 *	    class D;	// forward declaration
 *	};
 *
 *	// implementation of internal class D.
 *	template<class B>
 *	class C<B>::D : A {};
 *	---------------------------------------------
 * The workaround is to include the internal class D implementation in the
 * outer class declaration.
 */
#define TEMPLATE_INTERNAL_CLASS_BUG

#undef EXPLICIT_TEMPLATE_INSTANTIATION

#endif /* #ifndef __TEMPLATE_BUGS_H_ */
