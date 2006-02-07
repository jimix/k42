/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: first.S,v 1.2 2005/02/16 00:05:54 mergen Exp $
 *****************************************************************************/

/*****************************************************************************
 *
 *  Jump to the real entry point.  This instruction should be at start of .text
 *
 *****************************************************************************/
#define __ASSEMBLY__
#include "asm32.h"	
#include <sys/kinclude.H>
#include <misc/arch/powerpc/asdef.h>

C_TEXT_ENTRY(__start_of_text)
	/* Do a bl, to put the post-relocation address of __start_link_register
	 * in lr.  We calculate the offset between our link location
	 * and where we've been loaded by taking the difference between lr
	 * and the reported value of __start_link_register
	 */
	bl _start
GLBL_LABEL(__start_link_register)
C_TEXT_END(__start_of_text)
