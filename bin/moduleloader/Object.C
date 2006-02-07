/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Object.C,v 1.1 2005/06/07 03:46:39 jk Exp $
 *****************************************************************************/

/**
 * @file Object.C
 * A sample dynamically-loadable k42 module.
 */

/**
 * @todo fix Makefile so this compiles in a kernel environment
 */
#include <sys/sysIncs.H>

/**
 * Specify an initialisation function for this module. This is a temporary
 * solution to the 'specify an init function' idea.
 * @param fn the module's init function
 */ 
#define module_init(fn) extern "C" { void _init(void) { fn(); } }

static void test_module(void)
{
    err_printf("module loaded!\n");
}

module_init(test_module);

