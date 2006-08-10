#ifndef _K42_IOCHAN_H
#define _K42_IOCHAN_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IOChan.H,v 1.1 2005/02/09 18:45:41 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Logical stream IO channel
 * **************************************************************************/

struct IOChan {
    uval id;
    IOChan(uval num): id(num) {};
    virtual uval isReadable() = 0;
    virtual uval read(char* buf, uval length, uval block = 1) = 0;
    virtual uval write(const char* buf, uval length, uval block = 1) = 0;
    
    DEFINE_NOOP_NEW(IOChan);
    virtual ~IOChan() { }
};

extern IOChan* getSimConChan_Primitive(MemoryMgrPrimitive *mem, uval chan);

#endif /* _K42_IOCHAN_H */
