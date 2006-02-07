/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskMountInfoList.C,v 1.2 2004/11/23 22:40:58 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Hack of horrible way we get host-specific information
 *                     about availability of disks and intended mounts
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DiskMountInfoList.H"
#include <stub/StubKBootParms.H>

SysStatus
DiskMountInfoList::init()
{
    SysStatus rc;

    list.reinit();

    char buf[256];

    rc = StubKBootParms::_GetParameterValue("K42_FS_DISK", buf, 256);

    if (_SUCCESS(rc) && buf[0]) { // there is a specification
	enum {DEV = 0, MNT = 1, FSTYPE = 2, FLAGS = 3};
	char *pbuf = buf;
	while (pbuf) {
	    char *minfo[4] = {NULL, NULL, NULL, NULL};
	    rc = GetMountInfo(pbuf, minfo);
	    if (_FAILURE(rc)) {
		err_printf("skipping specs on %s\n", pbuf);
		return rc;
	    }
	    DiskMountInfo *dinfo = new DiskMountInfo();
	    rc = dinfo->init(minfo[DEV], minfo[MNT], minfo[FSTYPE],
			     minfo[FLAGS]);
	    _IF_FAILURE_RET(rc);
#if 0
	    err_printf("Adding (%s,%s,%s,%s) to DiskMountInfoList\n",
		       dinfo->getDev(), dinfo->getFSType(),
		       dinfo->getMountPoint(), dinfo->getFlagSpec());
#endif
	    list.add(dinfo);
	}
    } else {
	return rc;
    }
    return 0;
}


/* returns: device in minfo[0]
 *          mount point in minfo[1]
 *          fstype in minfo[2]
 *          flags in minfo[3]
 *
 */
/* static */ SysStatus
DiskMountInfoList::GetMountInfo(char* &buf, char* minfo[4])
{
    minfo[0] = buf;
    uval part = 1;
    uval len = strlen(buf);
    uval i;
    for (i=0; i < len; i++) {
	switch (buf[i]) {
	case ':':
	    if (part < 4) {
		if (i == len -1) {
		    minfo[part++] = NULL;
		} else {
		    minfo[part++] = &buf[i+1];
		}
	    } else {
		// format is wrong than expected, just ignore it
	    }
	    break;
	case ',':
	    // skip , and return
	    i++;
	    goto out;
	};
    }

out:

    for (int j=0; j < 2; j++) { // for now it's ok if minfo[3] is NULL
	if (minfo[j] == NULL) {
	    passertWrn(0, "getMountInfo returning error because minfo[%d] is"
		       " NULL (buf is %s)\n", j, buf);
	    return _SERROR(2821, 0, 0);
	}
    }

    // null terminate the parts
    for (int j = 1; j < 4; j++) {
	if (minfo[j] != NULL) { // the last one could be null
	    char *prev = minfo[j] - 1;
	    *prev = '\0';
	}
    }

    // last one needs to be null terminated if we finished due to finding a
    // separator comma, i.e., we stopped before going through the whole string
    if (minfo[3]) {
	if (i < len) {
	    buf[i-1] = '\0';
	}
    }

    if (i < len) {
	buf = &buf[i];
    } else {
	buf = NULL;
    }

#if 0
    err_printf("Returning from getMountInfo with dev %s, mnt %s, fstype %s,"
	       " flags %s\n", minfo[0], minfo[1], minfo[2], minfo[3]);
#endif

    return 0;
}

SysStatus
DiskMountInfo::init(char *d, char *m, char *t, char *fl)
{
    dev = (char*)AllocGlobal::alloc(strlen(d)+1);
    if (dev == NULL) return _SERROR(2861, 0, ENOMEM);
    memcpy(dev, d, strlen(d)+1);
    mnt = (char*) AllocGlobal::alloc(strlen(m)+1);
    if (mnt == NULL) return _SERROR(2862, 0, ENOMEM);
    memcpy(mnt, m, strlen(m)+1);
    fstype = (char*) AllocGlobal::alloc(strlen(t)+1);
    if (fstype == NULL) return _SERROR(2863, 0, ENOMEM);
    memcpy(fstype, t, strlen(t)+1);
    flags = (char*) AllocGlobal::alloc(strlen(fl)+1);
    if (flags == NULL) return _SERROR(2864, 0, ENOMEM);
    memcpy(flags, fl, strlen(fl)+1);

    return 0;
}
