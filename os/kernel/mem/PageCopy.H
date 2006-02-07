#ifndef __PAGE_COPY_H_
#define __PAGE_COPY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageCopy.H,v 1.3 2002/04/08 22:22:59 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
class PageCopy {
public:
    static void Memset(void * t, int cin, size_t count);
    static void Memset0(void * t, size_t count);
    static void Memcpy(void * t, const void * s, size_t count);

};

#endif /* #ifndef __PAGE_COPY_H_ */
