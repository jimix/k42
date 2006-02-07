#ifndef __DIR_BUF_H_
#define __DIR_BUF_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DirBuf.H,v 1.7 2001/10/05 21:47:35 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Describes contents of a directory buffer
 * **************************************************************************/
#include <misc/StringTable.H>

class DirBufEntry {
public:
    uval8 strLen;
    char str[1];
};

typedef _StrTable<DirBufEntry>  DirBuf;
#endif /* #ifndef __DIR_BUF_H_ */
