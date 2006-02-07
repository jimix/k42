/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: atexit_kern.C,v 1.7 2000/05/11 11:29:07 rosnbrg Exp $
 *****************************************************************************/

#include "sys/sysIncs.H"
extern "C" int atexit();

int atexit(void (* /*fn*/)())
{
    // atexit is a nop in the kernel, since the kernel never exits,
    // or at leaset never depends on static destructors at exit
    return 0;
}
