/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testMountPointMgr.C,v 1.5 2005/06/28 19:48:46 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: test mount point server
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <io/FileLinux.H>
#include <io/PathName.H>
#include <sys/MountPointMgrClient.H>
#include <sys/systemAccess.H>

static uval
BuildPathName(char *uname, PathNameDynamic<AllocGlobal>* &pn)
{
    return  PathNameDynamic<AllocGlobal>::Create(uname, strlen(uname), NULL, 0,
						 pn, 0);
}

static SysStatus
TestLookup(const PathName *oldPath, uval oldLen, uval &cmpResolved)
{
    char resolvedPath[PATH_MAX+1];
    ObjectHandle oh;
    PathName *unRes, *path;
    uval unResLen, pathLen;
    SysStatus rc;

    // need to copy path since comes back from lookup changed
    path = ((PathName *)oldPath)->dup(oldLen, resolvedPath, PATH_MAX+1);
    pathLen = oldLen;
    

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(path, pathLen, PATH_MAX+1, 
					       unRes, unResLen, oh);
    _IF_FAILURE_RET(rc);
    
    cmpResolved = 0;

    // will not work when dealing with symbolic links
    while(path != unRes) {
	path = path->getNext(pathLen, pathLen);
	cmpResolved++;
    }
    tassertMsg((pathLen == unResLen), "woops\n");
    return 0;
}

int
main(int argc, char **argv)
{
    NativeProcess();

    SysStatus rc;
    uval pnlen1, pnlen2;
    PathNameDynamic<AllocGlobal> *pn1, *pn2;
    ObjectHandle oh;
    uval numCompResolved;

    printf("this test is broken\n");
    return 1;

    /*
     * This test is easier to visualize if you consider the tree we're
     * building/querying (notation: M: mount point associated
     *                              U: defined as uncoverable)
     *
     * testMPM  (under whatever root we currently have on the system)
     *    |
     *    |_________l1a
     *    |         |______l2c (MU)
     *    |         |______l3d
     *    |                |______l4a (M)
     *    |
     *    |_________l1b (MU)
     *    |         |______l2a
     *    |                |________l3a (MU)
     *    |                         |__________l4b (M)
     *    |                         |__________l4c (M)
     *    |_________l1c
     *              |______l2b
     *                     |________l3b (M)
     *                     |________l3c (M)
     */

    // Step 1: search for /testMPM, should fail
    char *uname1 = "/testMPM";
    pnlen1 = BuildPathName(uname1, pn1);
breakpoint();
    rc = TestLookup(pn1, pnlen1, numCompResolved);
    tassertMsg(_SUCCESS(rc), "?");
    // expected to return numCompResolved == 0
    if (numCompResolved != 0) {
	printf("%s: lookup for %s expected to resolve only /\n", argv[0],
	       uname1);
	return 1;
    }

    // Step 2: add /testMPM/l1b as uncoverable, query for /testMPM and
    // testMPM/l1b
    char *uname2 = "/testMPM/l1b";
    pnlen2 = BuildPathName(uname2, pn2);
    rc = DREFGOBJ(TheMountPointMgrRef)->registerMountPoint(pn2, pnlen2, oh,
							   NULL, 0, "foo", 3,
							   0);
    tassertMsg(_SUCCESS(rc), "?");

    rc = TestLookup(pn1, pnlen1, numCompResolved);
    tassertMsg(_SUCCESS(rc), "?");
    // expected to return numCompResolved == 0
    if (numCompResolved != 0) {
	printf("%s: lookup for %s expected to resolve only /\n", argv[0],
	       uname1);
	return 1;
    }

    rc = TestLookup(pn2, pnlen2, numCompResolved);
    tassertMsg(_SUCCESS(rc), "?");
    // expected to return numCompResolved == 0
    if (numCompResolved != 2) {
	printf("%s: lookup for %s expected to resolve only two components"
	       " got %ld\n", argv[0], uname2, numCompResolved);
	return 1;
    }

    // Step 4: populate the tree and print it for "visual inspection"
    const uval NB_REST_TREE = 7;
    struct {
	char *name;
	char *desc;
	uval isCoverable;
    } restOfTree[NB_REST_TREE] = {{"/testMPM/l1a/l2c", "t0", 0},
				  {"/testMPM/l1a/l2c/l3d/l4a", "t1", 1},
				  {"/testMPM/l1b/l2a/l3a", "t2", 0},
				  {"/testMPM/l1b/l2a/l3a/l4b", "t3", 1},
				  {"/testMPM/l1b/l2a/l3a/l4c", "t4", 1},
				  {"/testMPM/l1c/l2b/l3b", "t5", 1},
				  {"/testMPM/l1c/l2b/l3c", "t6", 1},
    };

    PathNameDynamic<AllocGlobal> *pn;
    uval pnlen;
    for (uval i = 0; i < NB_REST_TREE; i++) {
	pnlen = BuildPathName(restOfTree[i].name, pn);
	rc = DREFGOBJ(TheMountPointMgrRef)->registerMountPoint
	    (pn, pnlen, oh, NULL, 0,
	     restOfTree[i].desc, strlen(restOfTree[i].desc),
	     restOfTree[i].isCoverable);
	tassertMsg(_SUCCESS(rc), "?");

	pn->destroy(pnlen);
    }

    DREFGOBJ(TheMountPointMgrRef)->print();

    // Step 5: tests related to uncoverable nodes
    const uval NB_TESTS = 8;
    struct {
	char *name;
	uval expectedNumCompResolved;
    } tests[] = {{"/testMPM/l1b/l2a", 2},
		 {"/testMPM/l1b/l2a/l3a", 4},
		 {"/testMPM/l1b/l2a/l3a/l4b", 4},
		 {"/testMPM/l1c/l2b/l3b", 4},
		 {"/testMPM/l1a/", 0},
		 {"/testMPM/l1a/l2c", 3},
		 {"/testMPM/l1a/l2c/l3d", 3},
		 {"/testMPM/l1a/l2c/l3d/l4a", 3},
    };
    for (uval i = 0; i < NB_TESTS; i++) {
	pnlen = BuildPathName(tests[i].name, pn);
	rc = TestLookup(pn, pnlen, numCompResolved);
	tassertMsg(_SUCCESS(rc), "?");

	if (numCompResolved != tests[i].expectedNumCompResolved) {
	    printf("%s: testLookup for %s returned numCompResolved %ld; "
		   "expected %ld\n", argv[0], tests[i].name, numCompResolved,
		   tests[i].expectedNumCompResolved);
	}
	pn->destroy(pnlen);
    }

    printf("Test succeeded\n");
    return 0;
}

