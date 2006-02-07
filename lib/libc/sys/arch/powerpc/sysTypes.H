#ifndef __SYS_TYPES_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __SYS_TYPES_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sysTypes.H,v 1.13 2002/10/11 16:35:58 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

struct EntryPointDesc {
    codeAddress iar;
    uval64 toc;

    void nullify() { iar = codeAddress(-1); toc = 0; }
    uval isNull() { return iar == codeAddress(-1); }
    void relocate(uval offset) { iar += offset; toc += offset;};
};

class SysTypesArch {
public:
    static uval const PID_BITS = 32;
    static uval const DSPID_BITS = 32;
    static uval const RD_BITS = 16;
    static uval const VP_BITS = 16;

    static uval const COMMID_PID_SHIFT = 32;
    static uval const COMMID_DSPID_SHIFT = 0;
    static uval const DSPID_RD_SHIFT = 16;
    static uval const DSPID_VP_SHIFT = 0;
};
