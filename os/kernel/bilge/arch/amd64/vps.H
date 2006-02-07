#ifndef __VPS_H_
#define __VPS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Constants for interfacing with VisualProbe simulated
 * vpsocket devices.
 * **************************************************************************/

// just placeholder x86 , soon useless XXX

#define VPS_MAGIC (*((uval *) "vps"))

#define VPS_THINWIRE_ID_PORT  0xfff8
#define VPS_THINWIRE_IO_PORT  0xfffc

#endif /* #ifndef __VPS_H_ */
