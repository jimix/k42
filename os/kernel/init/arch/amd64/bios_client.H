#ifndef __BIOS_CLIENT_H_
#define __BIOS_CLIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bios_client.H,v 1.3 2001/10/05 21:49:43 peterson Exp $
 *****************************************************************************/

// probably not needed, staright from x86 XXX


// #include  __MINC(bios.H)

typedef sval (*BiosServiceRoutine)(sval request, va_list ap);

void bios_client_init(BiosServiceRoutine);

#endif /* #ifndef __BIOS_CLIENT_H_ */
