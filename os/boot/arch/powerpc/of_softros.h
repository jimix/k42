#ifndef __POWERPC_OF_SOFTROS_H_
#define __POWERPC_OF_SOFTROS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: of_softros.h,v 1.6 2002/09/08 22:53:43 mostrows Exp $
 *****************************************************************************/

int (*rtas_in_handler)(int *, void *);
int (*cl_in_handler)(int *);
struct service_request;
int (*of_entry)(struct service_request *);
int printf(char *fmt, ...);
#endif /* #ifndef __POWERPC_OF_SOFTROS_H_ */
