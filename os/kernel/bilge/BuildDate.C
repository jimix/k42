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

#include "kernIncs.H"
#include "BuildDate.H"
#include <meta/MetaBuildDate.H>

char BuildDate::LinkDate[BUILDDATESTRSIZE];
char BuildDate::CVSCheckoutDate[BUILDDATESTRSIZE];
char BuildDate::BuiltBy[BUILDDATESTRSIZE];
char BuildDate::DebugLevel[BUILDDATESTRSIZE];
/* static */ void
BuildDate::ClassInit(VPNum vp)
{
    if (vp!=0) return;

    MetaBuildDate::init();

    strncpy(LinkDate, LINKDATE, BUILDDATESTRSIZE); 
    strncpy(CVSCheckoutDate, REGRESSTS, BUILDDATESTRSIZE); 
    strncpy(BuiltBy, MAKEUSER, BUILDDATESTRSIZE); 
    strncpy(DebugLevel, DBGLVL, BUILDDATESTRSIZE); 
}

/* static */ SysStatus
BuildDate::_getLinkDate(__outbuf(__rc:len) char* buf, __in uval len)
{
    uval size = strlen(LinkDate) + 1;
    if (size > len) size = len - 1;
    memcpy(buf, LinkDate, size);
    buf[size] = '\0';
    return size;
}

/* static */ SysStatus
BuildDate::_getBuiltBy(__outbuf(__rc:len) char* buf, __in uval len)
{
    uval size = strlen(LinkDate) + 1;
    if (size > len) size = len - 1;
    memcpy(buf, BuiltBy, size);
    buf[size] = '\0';
    return size;
}
