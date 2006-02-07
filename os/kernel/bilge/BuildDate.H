#ifndef __BUILD_DATE_H_
#define __BUILD_DATE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A file whose purpose is to be recompiled before
 *		       echo kernel link to provide a link date.
 * **************************************************************************/


class BuildDate : public Obj {
#define BUILDDATESTRSIZE 256
protected:
    static char LinkDate[BUILDDATESTRSIZE];
    static char CVSCheckoutDate[BUILDDATESTRSIZE];
    static char BuiltBy[BUILDDATESTRSIZE];
    static char DebugLevel[BUILDDATESTRSIZE];

public:
    static void ClassInit(VPNum vp);

    static SysStatus getLinkDate(char*& linkDate) {
	linkDate = LinkDate;
	return 0;
    }
    static SysStatus getCVSCheckoutDate(char*& cvsCheckoutDate) {
	cvsCheckoutDate = CVSCheckoutDate;
	return 0;
    }
    static SysStatus getBuiltBy(char*& builtBy) {
	builtBy = BuiltBy;
	return 0;
    }
    static SysStatus getDebugLevel(char*& debugLevel) {
	debugLevel = DebugLevel;
	return 0;
    }

__xpublic:
    static SysStatus _getLinkDate(__outbuf(__rc:len) char* buf,
				      __in uval len);

    static SysStatus _getBuiltBy(__outbuf(__rc:len) char* buf,
				     __in uval len);
};
#endif /* #ifndef __BUILD_DATE_H */
