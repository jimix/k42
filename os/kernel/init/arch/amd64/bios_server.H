#ifndef __BIOS_SERVER_H_
#define __BIOS_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bios_server.H,v 1.2 2001/10/05 21:49:43 peterson Exp $
 *****************************************************************************/

// probably not needed, staright from x86 XXX

#include __MINC(bios.H)

extern "C" sval bios_service(sval request, va_list ap);

#endif /* #ifndef __BIOS_SERVER_H_ */
