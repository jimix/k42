/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CRT.C,v 1.1 2000/10/30 18:25:31 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provide dummy CRT calls in the kernel to allow
 *                     for user level compatiblity
 * **************************************************************************/
#include <sys/sysIncs.H>
/********************************************************************/
/* FIXME:  This is here as a kludge talk to someone about the right */
/*         way to handle this                                       */

class CRT {
public:
    static SysStatus CreateVP(VPNum vp);
};
  
SysStatus
CRT::CreateVP(VPNum vp)
{
    return 0;
}

/*******************************************************************/

