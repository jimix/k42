#ifndef __REGIONTYPE_H_
#define __REGIONTYPE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionType.H,v 1.2 2003/12/31 20:03:51 jappavoo Exp $
 *****************************************************************************/

class RegionType {
public:
    enum Type {
	/* regions reproduced on fork */
	ForkCopy=1,
	NewCRW=2,
	/* N.B. the more attractive "Shared" breaks the linux part of
	 * the build - linux must define Shared somewhere in its guts
	 * So we settle for UseSame
	 */
	UseSame=3,
        MIP=4,
	/* don't delete on exec */
	KeepOnExec=0x80, FreeOnExec=0,
	/* a region with NO built in fork/exec behavior */
	K42Region=KeepOnExec
    };
    struct RegionElementInfo {
	RegionType::Type type;
	uval start, size;
    };
};
// so we can add the KeepOnExec without then casting result
inline RegionType::Type operator+(RegionType::Type a, RegionType::Type b)
                        {return RegionType::Type(uval(a)+uval(b));}
inline RegionType::Type operator|(RegionType::Type a, RegionType::Type b)
                        {return RegionType::Type(uval(a)|uval(b));}
#endif /* #ifndef __REGIONTYPE_H_ */
