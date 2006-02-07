#ifndef __SYS_FACTS_H_
#define __SYS_FACTS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysFacts.H,v 1.31 2001/10/16 05:32:07 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Classes that encapsulate basic facts about the system
 * Machine Independent Part
 * **************************************************************************/


class SysFacts {
public:
    SysFacts() { /* empty body */ }
    static void GetRebootImage(uval &imageAddr, uval &imageSize);

#define INCLASS
#include __MINC(SysFacts.H)
};
#undef INCLASS
#include __MINC(SysFacts.H)


#endif /* #ifndef __SYS_FACTS_H_ */
