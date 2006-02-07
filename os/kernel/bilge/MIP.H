#ifndef __MIP_H_
#define __MIP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MIP.H,v 1.1 2003/12/31 20:00:27 jappavoo Exp $
 *****************************************************************************/
class MIP {
public:
    static SysStatus ClassInit(VPNum);
    static uval ExtendMemory(uval);
};

#endif /* #ifndef __MIP_H_ */
