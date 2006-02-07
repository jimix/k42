/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testPageOut.C,v 1.31 2005/06/28 19:48:47 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A real simple implementation of cat.
 * **************************************************************************/

// FIXME: get rid of this:
#include <sys/sysIncs.H>
#include <io/NameTreeLinux.H>
#include <sys/BaseProcess.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>
#include <io/FileLinux.H>

#include <fcntl.h>
#include <sys/systemAccess.H>

int
main()
{
    NativeProcess();

    SysStatus rc;
    char *bufp, *s, *t;
    FileLinuxRef fileRef;

    rc = FileLinux::Create(fileRef, "testPageOut.tmp",
			   O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (_FAILURE(rc)) {
	err_printf("open of file testPageOut.tmp failed\n");
	return (rc);
    }

    while (1) {
	ThreadWait *tw = NULL;
	rc = (DREF(fileRef))->writeAlloc(4, bufp, &tw);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }

    if (_FAILURE(rc)) {
	tassertWrn(0, "writeAlloc failed\n");
	return rc;
    }

    for (s="foo\n",t = bufp; *s; *(t++) = *(s++));

    rc = DREF(fileRef)->writeFree(bufp);

    if (_FAILURE(rc)) {
	tassertWrn(0, "writeFree failed\n");
	return rc;
    }

    rc = DREF(fileRef)->destroy();

    if (_FAILURE(rc)) {
	tassertWrn(0, "destroy failed\n");
	return rc;
    }

    rc = FileLinux::Create(fileRef, "testPageOut.tmp",
			    O_RDONLY, 0544);
    if (_FAILURE(rc)) {
	err_printf("open of file testPageOut.tmp failed\n");
	return (rc);
    }

    /* purposely try to read too much - check below makes sure its correct */
    while (1) {
	ThreadWait *tw = NULL;
	rc = DREF(fileRef)->readAlloc(5, bufp, &tw);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }

    if (_FAILURE(rc)) {
	tassertWrn(0, "readAlloc failed\n");
	return rc;
    }

    if (_SGETUVAL(rc) != 4) {
	tassertWrn(0, "readAlloc length %ld wrong\n", _SGETUVAL(rc));
	return rc;
    }

    char test = 0;

    for (s="foo\n",t = bufp; *s; test |= (*(t++) ^ *(s++)));

    if (test) {
	tassertWrn(0, "compare failed\n");
	return 1;
    }

    rc = DREF(fileRef)->readFree(bufp);

    rc = DREF(fileRef)->destroy();

    if (_FAILURE(rc)) {
	tassertWrn(0, "destroy failed\n");
	return rc;
    }

    // test O_CREAT with O_EXCL: the following should fail
    rc = FileLinux::Create(fileRef, "testPageOut.tmp",
			    O_RDWR | O_CREAT| O_EXCL, 0644);
    if (_SGENCD(rc) != EEXIST) {
	err_printf("open(O_CREAT | O_EXCL) of file testPageOut.tmp failed\n");
	return 1;
    }

    // test open with O_TRUNC flag
    rc = FileLinux::Create(fileRef, "testPageOut.tmp", O_RDWR | O_TRUNC, 0644);
    if (_FAILURE(rc)) {
	err_printf("open (O_TRUNC) of file testPageOut.tmp failed\n");
	return (rc);
    }

    // purposely try to read from empty file
    while (1) {
	GenState moreAvail;
	ThreadWait *tw = NULL;
	char buf[128];
	rc = DREF(fileRef)->read(buf, 5, &tw, moreAvail);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }

    if (_FAILURE(rc)) {
	DREF(fileRef)->destroy();
	err_printf("read testPageOut.tmp after open O_TRUNC failed\n");
	return rc;
    }

    if (_SGETUVAL(rc) != 0) {
	tassertWrn(0,
		   "after open O_TRUNC readAlloc length "
		   "%ld wrong\n", _SGETUVAL(rc));
	return rc;
    }

    err_printf("testPageOut success\n");
    rc = DREF(fileRef)->destroy();

    return 0;
}
