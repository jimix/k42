/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MountPointMgrCommon.C,v 1.34 2005/08/22 22:35:59 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implements functionality common to server and
 * client mount point objects.  Have chosen to implement this as a
 * straight C++ object invoked by client and server objects, rather than
 * as a common superclass since gives more flexibility in future
 * rearrangement of code.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <io/PathName.H>
#include <io/FileLinux.H>
#include "MountPointMgrCommon.H"
#include <fslib/fs_defines.H>
#include <stub/StubNameTreeLinux.H>

//#define DEBUG_SYMLINKS

inline uval
getRecLen(uval lengthString)
{
    uval recLen = sizeof(struct direntk42);
    recLen += lengthString;
    recLen -= sizeof(((direntk42 *)0)->d_name);
    recLen = ALIGN_UP(recLen, sizeof(uval64));
    return recLen;
}

MountPointMgrCommon::MountComponent::MountPointInfo::
MountPointInfo(ObjectHandle poh, const PathName *pth, uval pthLen,
	       const char *d, uval dLen, uval iB)
{
    oh = poh;
    relLen = ((PathNameDynamic<AllocGlobal> *)pth)->dupCreate(
	pthLen, relPath);
    if (dLen) {
	descLen = dLen+1;
	desc = (char*) allocGlobal(descLen);
	memcpy(desc, d, dLen);
	desc[dLen] = '\0';
    } else {
	descLen = 0;
	desc = NULL;
    }
    isBind = iB;
}

MountPointMgrCommon::MountComponent::MountPointInfo::
~MountPointInfo()
{
    relPath->destroy(relLen);
    if (desc) {
	freeGlobal(desc, descLen+1);
    }
}

MountPointMgrCommon::MountComponent::SymbolicLinkInfo::
SymbolicLinkInfo(const char *d, uval dl)
{
    data = (char *)allocGlobal(dl+1);
    memcpy(data, d, dl);
    dataLen = dl;
}

MountPointMgrCommon::MountComponent::SymbolicLinkInfo::
~SymbolicLinkInfo()
{
    freeGlobal(data, dataLen+1);
}

uval
MountPointMgrCommon::MountComponent::getDents(struct direntk42 *buf, uval len)
{
    MountComponent *pChld;
    void *iter;				// iterator for list
    struct direntk42 *dp;
    struct direntk42 *nextdp=0;
    uval dpend;
    uval ino=5000;			// got better ideas?

    // get the first child (that doesn't correspond to sym link)
    iter = children.next(NULL, pChld);
    while (iter != NULL && pChld->isSymLink()) {
	iter = children.next(iter, pChld);
    }
    if (iter == NULL) return 0;

    tassertMsg(len >= sizeof(struct direntk42),
	       "buf is not large enough to hold struct direntk42\n");

    dpend = (uval)buf + len;
    dp = buf;

    while (iter != NULL) {
	dp->d_ino	= ino++;
#if defined DT_UNKNOWN && defined _DIRENT_HAVE_D_TYPE
	dp->d_type	= DT_UNKNOWN;
#endif
#if defined _DIRENT_HAVE_D_NAMLEN
	dp->d_namlen	= pChld->compLen;
#endif
	dp->d_reclen	= getRecLen(pChld->compLen);
	memcpy(dp->d_name, pChld->comp, pChld->compLen);
	nextdp = (struct direntk42 *)((uval)dp + getRecLen(pChld->compLen));

	iter = children.next(iter, pChld);
	while (iter != NULL && pChld->isSymLink()) {
	    iter = children.next(iter, pChld);
	}

	// Make sure we can fit another
	if ((iter != NULL) &&
	    (((uval)nextdp + getRecLen(pChld->compLen)) < dpend)) {
	    dp->d_off = (uval)nextdp - (uval)buf;
	    dp = nextdp;
	} else {
	    dp->d_off = 0;
	    break;
	}

    }
    return _SRETUVAL((uval)nextdp - (uval)buf);
}

void
MountPointMgrCommon::MountComponent::print(uval indent)
{
    uval i;
    void *iter;
    MountComponent *pChld;

    i = indent;
    while ((i--)>0) {
	err_printf("\t");
    };
    err_printf("%s", comp);

    if (isMountPoint()) {
	char tbuf[256];
	uval len;

	len = getMTRelPath()->getUPath(getMTRelLen(), tbuf, 256);
	err_printf(" -> oh <%ld, %ld >, path %s",
		   getMTOH()->commID(), getMTOH()->xhandle(),
		   tbuf);
    } else {
	if (isSymLink()) {
	    err_printf("-> %s", getSLData());
	}
    }
    err_printf("\n");
    iter = children.next(NULL, pChld);
    while (iter!=NULL) {
	pChld->print(indent+1);
	iter = children.next(iter, pChld);
    };
}

MountPointMgrCommon::MountComponent *
MountPointMgrCommon::MountComponent::findChild(PathName *name, uval pathLen)
{
    MountComponent *pChld;
    void *iter;				// iterator for list
    // get the first child
    iter = children.next(NULL, pChld);
    while (iter != NULL) {
        if (pChld->compare(name, pathLen)) {
	    return pChld;
	}
	// doesn't match, check next
	iter = children.next(iter, pChld);
    }
    return NULL;
}

SysStatus
MountPointMgrCommon::MountComponent::init(char *pth, uval len)
{
    SysStatus rc;
    rc = 0;

    mountPointInfo = NULL;
    compType = EMPTY;
    compLen = len+1; // note, null terminate
    comp = (char *)allocGlobal(compLen);
    tassertMsg(comp!=NULL, "alloc failed\n");
    memcpy(comp, pth, compLen-1);
    comp[compLen-1] = '\0';

    return rc;
}

SysStatus
MountPointMgrCommon::MountComponent::reInit(char *pth, uval len)
{
    SysStatus rc;
    rc = 0;

    if (isMountPoint()) {
	delete mountPointInfo;
    }
    freeGlobal(comp, compLen);

    mountPointInfo = NULL;
    compType = EMPTY;
    compLen = len+1; // note, null terminate
    comp = (char *)allocGlobal(compLen);
    tassertMsg(comp!=NULL, "alloc failed\n");
    memcpy(comp, pth, compLen-1);
    comp[compLen-1] = '\0';

    return rc;
}

SysStatus
MountPointMgrCommon::MountComponent::makeMountPoint(
    ObjectHandle poh, const PathName *pth, uval pthLen,
    const char *desc, uval descLen, uval isCov, uval isBind)
{
    tassert(isEmpty(), err_printf("already allocated\n"));
    if (isCov) {
	compType = COVMOUNT;
    } else {
	compType = MOUNT;
    }
    mountPointInfo = new MountPointInfo(poh, pth, pthLen, desc, descLen,
					isBind);
    return 0;
}

SysStatus
MountPointMgrCommon::MountComponent::overwriteMountPoint(
    ObjectHandle poh, const PathName *pth, uval pthLen,
    const char *desc, uval descLen, uval isCov, uval isBind)
{
    if (isMountPoint()) {
	delete mountPointInfo;
	compType = EMPTY;
    }
    return makeMountPoint(poh, pth, pthLen, desc, descLen, isCov, isBind);
}

SysStatus
MountPointMgrCommon::MountComponent::makeSymLink(const char *str, uval len)
{
    tassert(isEmpty(), err_printf("already allocated\n"));
    compType = SYMLINK;
    symbolicLinkInfo = new SymbolicLinkInfo(str, len);
    return 0;
}

/* static */ MountPointMgrCommon::MountComponent *
MountPointMgrCommon::MountComponent::Create(PathName *pth,  uval pthlen,
					    SysStatus &rc)
{
    MountComponent *mc = new MountComponent();
    tassertMsg(mc!=NULL, "alloc failed\n");
    rc = mc->init(pth->getCompName(pthlen), pth->getCompLen(pthlen));
    return mc;
}

MountPointMgrCommon::MountComponent *
MountPointMgrCommon::addMountComponent(const PathName *mountPath, uval lenMP)
{
    SysStatus rc;
    MountComponent *ptrPar;		// current parent
    PathName *currPth = (PathName *)mountPath; // current path component
    uval remainder = lenMP;
    MountComponent *ptrChld;

    // cludge to allow mounting at root
    tassertMsg(!(PathName::IsEmpty(currPth, remainder)), "woops, empty MP");
    ptrPar = &top;	// get top element of tree
    while (1) {
	ptrChld = ptrPar->findChild(currPth, remainder);
	if (ptrChld == NULL) {
	    break;			// didn't find child, new stuff to add
	}
	currPth = currPth->getNext(remainder, remainder);
	if (PathName::IsEmpty(currPth, remainder)) {
	    return ptrChld;
	}
	ptrPar = ptrChld;
    }

    /*
     * At this point, currPth points to the unresolved part of the path,
     * and ptrPar points to the parent the unresolved part should be
     * mounted under
     */
    while (currPth) {
	PathName *next;
	uval nextLen;
	// get the next part of path, to see if this is the last component
	next = currPth->getNext(remainder, nextLen);

	ptrChld = MountComponent::Create(currPth, remainder, rc);
	tassertMsg( _SUCCESS(rc), "woops\n");
	ptrPar->addChild(ptrChld);
	if (PathName::IsEmpty(next, nextLen)) {
	    return ptrChld;
	} else {
	    currPth = next;
	    remainder = nextLen;
	    ptrPar = ptrChld;
	}
    }
    tassertMsg(0, "should never get here\n");
    return 0;
}

SysStatus
MountPointMgrCommon::registerMountPoint(const PathName *mountPath, uval lenMP,
					ObjectHandle oh,
					const PathName *relPath, uval lenRP,
					const char *desc, uval lenDesc,
					uval isCoverable,
					uval isBind /* = 0 */)
{
    MountComponent *mc;
    tassertMsg(isCoverable == 0 || isCoverable == 1, "?");
    // this code assumes that relPath is not NULL for empty paths
    tassertMsg(relPath, "Assumption about relPath not valid\n");
    tassertMsg(lenDesc < MAX_MP_DESC_LEN, "no space for desc\n");

    // cludge to allow mounting at root
    if (PathName::IsEmpty((PathName *)mountPath, lenMP)) {
	tassertWrn(top.isEmpty(), "already mounted something at root\n");
#if 1   // FIXME: hack hiding desc to save space (dilma)
	top.overwriteMountPoint(oh, relPath, lenRP, desc, lenDesc,
				isCoverable, isBind);
#else
	top.overwriteMountPoint(oh, relPath, lenRP, NULL, 0,
				isCoverable, isBind);
#endif
	return 0;
    }

    mc = addMountComponent(mountPath, lenMP);
    tassertMsg((mc !=NULL), "woops");

#if 1   // FIXME: hack hiding desc to save space (dilma)
    mc->overwriteMountPoint(oh, relPath, lenRP, desc, lenDesc, isCoverable,
			    isBind);
#else
    mc->overwriteMountPoint(oh, relPath, lenRP, NULL, 0, isCoverable,
			    isBind);
#endif
    return 0;
}

SysStatus
MountPointMgrCommon::registerSymLink(const PathName *symPath, uval lenSP,
				     char *buf, uval lenBuf)
{
    MountComponent *mc;
    buf[lenBuf] = '\0';			// readlink doesn't come in null term
#ifdef DEBUG_SYMLINKS
    {
	char tmp[PATH_MAX];
	((PathName *)symPath)->getUPath(lenSP, tmp, sizeof(tmp));
	err_printf("MountPointMgrCommon:registering sympath %s to %s\n",
		   tmp, buf);
    }
#endif

    mc = addMountComponent(symPath, lenSP);
    tassertMsg((mc !=NULL), "woops");

    mc->makeSymLink(buf, lenBuf+1);
    return 0;
}

// resolve and cache any symbolic links in this path
SysStatus
MountPointMgrCommon::resolveSymbolicLink(const PathName *apth, uval apthLen)
{
    SysStatusUval rc;
    char buf[PATH_MAX+1], pthBuf[PATH_MAX+1];
    PathName *unRes, *cPth;
    uval unResLen, cPthLen, found = 0, lenTo;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    MountComponent *mc;

    PathName *pth = (PathName*) apth;
    uval pthLen = apthLen;

    // FIXME: pass correct maxlen
    rc = lookupMP(pth, pthLen, 256, unRes, unResLen, mc);
    _IF_FAILURE_RET(rc);
	
    tassertMsg( (mc->isMountPoint()), "in resolve path got non mount point\n");

    stubNT.setOH(*mc->getMTOH());

    // move relative path to start of buffer
    cPthLen = mc->getMTRelLen();
    cPth = mc->getMTRelPath()->dup(cPthLen, pthBuf, PATH_MAX+1);

    do {
	// add to path sent to server the next component unresolved
	cPthLen = cPth->appendComp(cPthLen, PATH_MAX+1, unRes);
	unRes = unRes->getNext(unResLen, unResLen); // bump components resolved
	rc = stubNT._readlink(cPth->getBuf(), cPthLen, buf, PATH_MAX+1);
	if (_SUCCESS(rc)) found = 1;
    } while ( (unResLen > 0) && (found == 0));

    tassertMsg( (found==1), "resolveSymbolicLink didn't find symlink\n");
    lenTo = _SGETUVAL(rc);		// length of string in file

    // now get the part of the original path up to the component
    // and register it
    if (unRes != NULL) {
	cPth = ((PathName *)pth)->getStart(pthLen, unRes, cPthLen);
    } else {
	cPth = (PathName *)pth;
	cPthLen = pthLen;
    }

    // okay, now have path in simLinkPath, and data in buffer
    // add link to mount client
    rc = registerSymLink(cPth, cPthLen, buf, lenTo);
    return rc;
}

void
MountPointMgrCommon::print()
{
    top.print(0);
}

struct MountPointMgrCommon::History {
    MountComponent *comp;		// parent component
    History        *prev;		// previous element
    void           *chldTok;	// token for traversal to next child
    uval            pthLen;		// length of path
    PathNameDynamic<AllocLocalStrict> *pth;	// full path of parent

    // just temporary storage so allocate locally
    DEFINE_LOCALSTRICT_NEW(History);
};

SysStatus
MountPointMgrCommon::marshalToBuf(MarshBuf *marshBuf)
{
    marshBuf->reset();
    History *prev = 0;
    void *chldTok = NULL;		// if null, looking for first child

    PathNameDynamic<AllocLocalStrict> *curPath = 0;
    uval curPathLen = 0;
    MountComponent *curComp = &top;		// current comp

    if (curComp->isMountPoint()) {
	marshBuf->addEntry(curPath, curPathLen, curComp->getMTOH(),
			   curComp->getMTRelPath(), curComp->getMTRelLen(),
			   curComp->getDesc(), curComp->getDescLen(),
			   curComp->isCoverable(), curComp->getIsBind());
    }
    while (1) {
	MountComponent *chdComp;
	// if another child go down, and save next
	if ((chldTok = curComp->nextChild(chldTok, chdComp)) != NULL) {

	    // save all information about parent
	    History *sv = new History();
	    tassertMsg((sv != NULL), "failed allocation\n");
	    sv->prev = prev;
	    sv->chldTok = chldTok;
	    sv->pth = curPath;
	    sv->pthLen = curPathLen;
	    sv->comp = curComp;

	    // create new path appending child info
	    curPathLen = curPath->appendCompCreate(curPathLen, chdComp->comp,
						   chdComp->compLen-1, curPath);
	    prev = sv;
	    curComp = chdComp;
	    chldTok = NULL;		// start over with children

	    if (curComp->isMountPoint()) {
		marshBuf->addEntry(curPath, curPathLen, curComp->getMTOH(),
				   curComp->getMTRelPath(),
				   curComp->getMTRelLen(),
				   curComp->getDesc(), curComp->getDescLen(),
				   curComp->isCoverable(),
				   curComp->getIsBind());
	    }
	} else { // else pop up and will try sibling
	    History *tmp;
	    if (prev == NULL) {
		// we are done
		return 0;
	    }
	    curPath->destroy(curPathLen);
	    chldTok = prev->chldTok;
	    curPath = prev->pth;
	    curPathLen = prev->pthLen;
	    curComp = prev->comp;
	    tmp = prev;
	    prev = prev->prev;
	    delete tmp;
	}
    }
}

SysStatus
MountPointMgrCommon::lookup(const PathName *name, uval namelen,
			    PathName *&unRes, uval &unResLen,
			    MountComponent *&mc, uval followLink /* = 1 */)
{
    // we track the last mount point we came accross in
    // traversal
    // node, we stop as soon as we hit a symbolic link
    // node, we do not keep track of a coverable mount point if
    //   we already know about a non-coverable one.
    struct MountPointFound {
	MountComponent *mc;
	PathName *unRes;		// unresolved part after this pathlen
	uval unResLen;
	void set(MountComponent *imc, const PathName *iunRes, uval iunResLen) {
	    // if current one saved is coverable, then cover with this one
	    // also, if new element is a symbolic link, save it
	    if ((mc == NULL) || (mc->isCoverable()) || (!imc->isCoverable()) ||
		(imc->isSymLink())) {
		mc = imc;
		unRes = (PathName *)iunRes;
		unResLen = iunResLen;
	    }
	}
	MountPointFound() {
	    mc = NULL; unRes = NULL; unResLen = 0;
	}
    };

    // mount point found
    MountPointFound lastMP;

    PathName *currPth = (PathName *)name; // current path component
    uval remainder = namelen;

    MountComponent *ptrPar;		// current parent
    MountComponent *ptrChld;

    if (top.isMountPoint()) {
	lastMP.set(&top, currPth, remainder);
    }

    tassertMsg(!top.isSymLink(), "being silly");

    ptrPar = &top;	// get top element of tree
    // while there is something to be resolved in the path
    while (!PathName::IsEmpty(currPth, remainder)) {
	ptrChld = ptrPar->findChild(currPth, remainder);
	if (ptrChld == NULL) {
	    break;
	}

	currPth = currPth->getNext(remainder, remainder);
	if (!ptrChld->isEmpty()) {

	    if ((followLink == 0) && (ptrChld->isSymLink()) &&
		(PathName::IsEmpty(currPth, remainder)) ) {
		// for operations like unlink want to operate on
		// symbolic link, not on thing points to
		goto return_result;
	    }

	    lastMP.set(ptrChld, currPth, remainder);
	    if (ptrChld->isSymLink()) goto return_result;
	}
	ptrPar = ptrChld;
    }

 return_result:
    if (lastMP.mc == NULL) {
	return _SERROR(1416, 0, ENOENT);
    }

    unRes = lastMP.unRes;
    unResLen = lastMP.unResLen;
    mc = lastMP.mc;

    return 0;
}

SysStatus
MountPointMgrCommon::lookupMP(PathName *name, uval &nameLen, uval maxLen,
			      PathName *&unRes, uval &unResLen,
			      MountComponent *&mc, uval followLink /* = 1 */)
{
    SysStatus rc;
    PathNameDynamic<AllocGlobal> *newPath; // fake dynamic alloc for type cheat
    uval newPathLen;
    uval iterations = 0;
    PathName *tmpPath;
    char tmpBuf[PATH_MAX];

    while (1) {
	char *to;
	rc = lookup(name, nameLen, unRes, unResLen, mc, followLink);
	if (_FAILURE(rc)) return rc;

	if (mc->isMountPoint()) {
	    // found a mount point, done with sym links
	    break;
	}

	if (iterations++ > 10) {
	    tassertWrn(0, "too many symbolic links\n");
	    return _SERROR(2676, 0, ELOOP);
	}

	tassertMsg((mc->isSymLink()), "better be a symbolic link");

	// found a symbolic link, construct new path and retry
#ifdef DEBUG_SYMLINKS
	{
	    char tmp[PATH_MAX];
	    name->getUPath(nameLen, tmp, sizeof(tmp));
	    err_printf("MountPointMgrCommon: found sym link %s ",
		       tmp);
	    unRes->getUPath(unResLen, tmp, sizeof(tmp));
	    err_printf("unresolved %s ", tmp);
	    err_printf("-> %s \n", mc->getSLData());
	}
#endif

	to = mc->getSLData();

	// create new path with relpath and unresolved appended
	// no real allocation done here... putting in tmpBuf
	newPathLen = PathNameDynamic<AllocGlobal>::Create(to, strlen(to),
							  NULL, 0,
							  newPath,
							  sizeof(tmpBuf),
							  tmpBuf);
	// append on the unresolved part of original path
	rc = newPath->appendPath(newPathLen, sizeof(tmpBuf), unRes, unResLen);
	tassertMsg(_SUCCESS(rc), "woops\n");
	newPathLen = _SGETUVAL(rc);

#ifdef DEBUG_SYMLINKS
	{
	    char tmp[PATH_MAX];
	    newPath->getUPath(newPathLen, tmp, sizeof(tmp));
	    err_printf("\t newPath on lookup is %s \n", tmp);
	}
#endif

	if (*to == '/') {
	    // if absolute path, copy back to to name
	    tmpPath = newPath->dup(newPathLen, name->getBuf(), maxLen);
	    tassertMsg((tmpPath == name), "hu?\n");
	    nameLen = newPathLen;
	} else {
	    // if relpath, append to path before resolved component

	    // get path up to the component that resolved to sym link
	    if (unRes != NULL) {
		tmpPath = name->getPrev(nameLen, unRes);
	    } else {
		tmpPath = NULL;
	    }
	    name = name->getStart(nameLen, tmpPath, nameLen);

	    // Notice that newPath has a representation of symlink value (variable "to"),
	    // but any initial ".." has been lost in the convertion of "to"
	    // to a PathName representation. We have to adjust name to account for
	    // things as "../././../" before appending newPath
	    uval toLen = strlen(to);
	    while (toLen > 1 && *to == '.') {
		if (*(to+1) == '.') { // we have "..", back up on component
		    name = name->getStart(nameLen, NULL, nameLen);
		    if (toLen == 2) { // we have exactly ".."
			break;
		    } else {
			tassertMsg(*(to+2) == '/', "?");
			to += 3; // skip "../"
			toLen -= 3;
		    }
		} else { // it has to be "./", nothing to do
		    tassertMsg(*(to+1) == '/', "?");
		    to += 2; // skip "./"
		    toLen -= 2;
		}
	    }

#ifdef DEBUG_SYMLINKS
	    {
		char tmp[PATH_MAX];
		name->getUPath(nameLen, tmp, sizeof(tmp));
		err_printf("\t After getStart name on lookup is on %s \n", tmp);
	    }
#endif
	    rc = name->appendPath(nameLen, maxLen, newPath, newPathLen);
	    tassertMsg(_SUCCESS(rc), "woops\n");
	    nameLen = _SGETUVAL(rc);
	}

#ifdef DEBUG_SYMLINKS
	{
	    if (*to == '/') {
		err_printf("\t absolute path\n");
	    } else {
		err_printf("\t relative path\n");
	    }
	    char tmp[PATH_MAX];
	    name->getUPath(nameLen, tmp, sizeof(tmp));
	    err_printf("\t new lookup on %s \n", tmp);
	}
#endif
    }

    return 0;
}

SysStatus
MountPointMgrCommon::lookup(PathName *name, uval &nameLen, uval maxLen,
			    PathName *&retUnRes, uval &retUnResLen,
			    ObjectHandle &oh, uval followLink /* = 1 */)
{
    MountComponent *mc;
    SysStatusUval rc;
    char *buf = name->getBuf();
    PathName *unRes;
    uval unResLen;
    char tmpBuf[PATH_MAX];

    rc = lookupMP(name, nameLen, maxLen, unRes, unResLen, mc, followLink);
    if (_FAILURE(rc)) return rc;    

    // found mount point
    tassertMsg(mc->isMountPoint(), "not mp\n");

    // now move remainder of path into a temporary buffer,
    unRes = unRes->dup(unResLen, tmpBuf, sizeof(tmpBuf));
    if (unRes == NULL) {
	tassertMsg((unRes != NULL), "buffer too small\n");
	// buffer was too small for the path
	return _SERROR(1413, 0, ENOMEM);
    }

    // move relative path to start of path
    mc->getMTRelPath()->dup(mc->getMTRelLen(), buf, maxLen);
    nameLen = mc->getMTRelLen();

    // append remainder of path to new path
    rc = name->appendPath(nameLen, maxLen, unRes, unResLen,
			  retUnRes);
    retUnResLen = unResLen;
    nameLen = _SGETUVAL(rc);

#ifdef DEBUG_SYMLINKS
    {
	char tmp[PATH_MAX];
	name->getUPath(nameLen, tmp, sizeof(tmp));
	err_printf("\t leaving lookup with name  %s \n", tmp);
    }
#endif

    oh = *mc->getMTOH();
    return 0;
}

SysStatus
MountPointMgrCommon::bind(const PathName *oldPath, uval oldLen,
			  const PathName *newPath, uval newLen,
			  uval isCoverable)
{
    SysStatus rc;
    char resolvedPath[PATH_MAX+1];
    ObjectHandle oh;
    PathName *unRes, *path;
    uval unResLen, pathLen;

    // need to copy path since comes back from lookup changed
    path = ((PathName *)oldPath)->dup(oldLen, resolvedPath, PATH_MAX+1);
    pathLen = oldLen;

    rc = lookup(path, pathLen, PATH_MAX+1, unRes, unResLen, oh);
    _IF_FAILURE_RET(rc);
    
    // gathering information to describe this mount poing
    char tbuf[PATH_MAX+1+9];
    const char *initial = "bind to ";
    strncpy(tbuf, initial, strlen(initial));
    uval lennewp;
    lennewp = ((PathName*)oldPath)->getUPath(oldLen, &tbuf[strlen(initial)],
					     PATH_MAX+1);
    rc = registerMountPoint(newPath, newLen, oh, path, pathLen,
			    tbuf, strlen(tbuf), isCoverable,
			     /* isBind */ 1);
    return rc;
}

void
MountPointMgrCommon::init()
{
    /*
     * Note, it doesn't really matter what the top element string value is,
     * we always start searching through its children, and anything that mounts
     * with a null pathname will just override this top element (again with
     * no string)
     */
    top.init(".", 1);
}

void
MountPointMgrCommon::reInit()
{
    History *prev = 0, *tmp;
    MountComponent *chdComp;
    void *chldTok;

    PathNameDynamic<AllocLocalStrict> *curPath = 0;
    uval curPathLen = 0;
    MountComponent *curComp = &top;		// current comp

    // if chldTok, have a child
    chldTok = curComp->nextChild(NULL, chdComp);
    while (1) {
	// err_printf("looking %p, %s\n", curComp, curComp->comp);
	// if another child go down, and save next
	if (chldTok != NULL) {
	    // err_printf("new child %p, %s\n", chdComp, chdComp->comp);
	    // save all information about parent
	    History *sv = new History();
	    tassertMsg((sv != NULL), "failed allocation\n");
	    sv->prev = prev;
	    sv->chldTok = chldTok;
	    sv->pth = curPath;
	    sv->pthLen = curPathLen;
	    sv->comp = curComp;

	    // create new path appending child info
	    curPathLen = curPath->appendCompCreate(curPathLen, chdComp->comp,
						   chdComp->compLen-1, curPath);
	    prev = sv;
	    curComp = chdComp;

	    // decend passing in NULL since starting over with children
	    chldTok = curComp->nextChild(NULL, chdComp);
	} else { // else pop up and will try sibling
	    if (prev == NULL) {
		// we are done
		tassertMsg(curComp == &top, "doing something wrong\n");
		top.reInit(".", 1);
		return;
	    }
	    // iterate, since can't delete until have iterated
	    chldTok = prev->comp->nextChild(prev->chldTok, chdComp);
	    curPath->destroy(curPathLen);
	    curPath = prev->pth;
	    curPathLen = prev->pthLen;
	    curComp = prev->comp;
	    tmp = prev;
	    prev = prev->prev;
	    delete tmp;
	}
    }
}

void *
MountPointMgrCommon::MarshBuf::copyFromBuf(void *cur, void *p, uval len)
{
    memcpy(p, cur, len);
    return (void *)(uval(cur) + len);
}

void *
MountPointMgrCommon::MarshBuf::getEntry(void *cur,
					PathName *&pth, uval &pthLen,
					ObjectHandle &oh,
					PathName *&relPath, uval &lenRP,
					char *desc, uval &lenDesc,
					uval &isCoverable,
					uval &isBind)
{
    SysStatus rc;
    void *end = (void *)(uval(data) + used);
    if (cur >= end) return NULL;
    if (cur == NULL) {
	cur = data;
    }
    cur = copyFromBuf(cur, &pthLen, sizeof(uval));
    rc = PathName::PathFromBuf((char *)cur, pthLen, pth);
    tassertMsg(_SUCCESS(rc), "woops\n");
    cur = (void *)(uval(cur) + pthLen);
    cur = copyFromBuf(cur, &lenRP, sizeof(uval));
    rc = PathName::PathFromBuf((char *)cur, lenRP, relPath);
    tassertMsg(_SUCCESS(rc), "woops\n");
    cur = (void *)(uval(cur) + lenRP);
    cur = copyFromBuf(cur, &oh, sizeof(ObjectHandle));
    cur = copyFromBuf(cur, &lenDesc, sizeof(uval));
    if (lenDesc) {
	cur = copyFromBuf(cur, desc, lenDesc);
    }
    cur = copyFromBuf(cur, &isCoverable, sizeof(isCoverable));
    cur = copyFromBuf(cur, &isBind, sizeof(isBind));
    return cur;
}

uval
MountPointMgrCommon::MarshBuf::copyToClientBuf(uval inlen, void *buf,
					       uval &cur, uval &left)
{
    uval n = used - cur;
    if (n > inlen) {
	left = n - inlen;
	n = inlen;
    } else {
	left = 0;
    }

    memcpy(buf, (void*) ((char*)data + cur), n);
    cur += n;
    return n;
};

SysStatus
MountPointMgrCommon::demarshalFromBuf(MarshBuf *marshBuf)
{
    void *cur = NULL;
    PathName *pth, *relPth;
    uval pthLen, lenRP, isCoverable, isBind;
    ObjectHandle oh;
    SysStatus rc;
    char desc[MAX_MP_DESC_LEN];
    uval descLen;

    while (1) {
	cur = marshBuf->getEntry(cur, pth, pthLen, oh, relPth, lenRP,
				 desc, descLen, isCoverable, isBind);
	if (cur != NULL) {
	    rc = registerMountPoint(pth, pthLen, oh, relPth, lenRP, desc,
				    descLen, isCoverable, isBind);
	    tassertMsg(_SUCCESS(rc), "woops\n");
	} else {
	    // print();			// FIXME: remove
	    return 0;
	}
    }
}

void
MountPointMgrCommon::MarshBuf::init()
{
    uval realSize;
    data = allocGlobal(1024, realSize);
    len = realSize;
    used = 0;
}

void
MountPointMgrCommon::MarshBuf::reset()
{
    used = 0;
}

inline void
MountPointMgrCommon::MarshBuf::copyToBuf(void *p, uval pLen)
{
    void *tail;
    if (pLen >= (len-used)) {
	void *oldData = data;
	uval oldLen = len;

	while ((pLen >= (len-used))) {
	    len = len*2;
	};

	data = allocGlobal(len);
	tassertMsg(data != NULL, "alloc of marshal buffer failed\n");
	memcpy(data, oldData, used);
	freeGlobal(oldData, oldLen);
    }
    tail = (void *)(uval(data) + used);
    memcpy(tail, p, pLen);
    used += pLen;
}

inline void
MountPointMgrCommon::MarshBuf::addPath(PathName *pth, uval pthLen)
{
    copyToBuf(&pthLen, sizeof(uval));
    if (pthLen) {
	copyToBuf(pth->getBuf(), pthLen);
    }
}

void
MountPointMgrCommon::MarshBuf::addEntry(PathName *pth, uval pthLen,
					ObjectHandle *oh,
					PathName *relPath, uval relLen,
					char *desc, uval descLen,
					uval isCov, uval isBind)
{
    addPath(pth, pthLen);
    addPath(relPath, relLen);
    copyToBuf(oh, sizeof(ObjectHandle));
    copyToBuf(&descLen, sizeof(uval));
    if (descLen) {
	copyToBuf(desc, descLen);
    }
    copyToBuf(&isCov, sizeof(isCov));
    copyToBuf(&isBind, sizeof(isBind));
}

SysStatusUval
MountPointMgrCommon::getDents(PathName *name, uval namelen, struct direntk42 *buf,
			   uval len)
{
    MountComponent *mntComp;		// current mount point looking at

    mntComp = &top;	// get top element of tree
    // follow path until done, then return children of this
    // while there is something to be resolved in the path
    while (!PathName::IsEmpty(name, namelen)) {
	mntComp = mntComp->findChild(name, namelen);
	if (mntComp == NULL) {
	    return 0;			// didn't find child, nothing under
					// this dir
	}
	name = name->getNext(namelen, namelen);
    }
    return mntComp->getDents(buf, len);
}

/* static */ void
MountPointMgrCommon::Test()
{
    MountPointMgrCommon mpc;
    MarshBuf marshBuf;
    PathNameDynamic<AllocLocalStrict> *tmpp1, *tmpp2;
    uval tmpl1, tmpl2;
    SysStatus rc;
    char *nm1, *nm2;
    ObjectHandle oh;

    mpc.init();
    marshBuf.init();

    nm1 = "/path/path1";
    nm2 = "/relpath";
    tmpl1 = PathNameDynamic<AllocLocalStrict>::Create(nm1, strlen(nm1),
						      0, 0, tmpp1, PATH_MAX+1);
    tmpl2 = PathNameDynamic<AllocLocalStrict>::Create(nm2, strlen(nm2),
						      0, 0, tmpp2, PATH_MAX+1);
    rc = mpc.registerMountPoint(tmpp1, tmpl1, oh, tmpp2, tmpl2, "test", 4, 0);
    tmpp1->destroy(PATH_MAX+1);
    tmpp2->destroy(PATH_MAX+1);

    nm1 = "/path/path2";
    nm2 = "/relpath2";
    tmpl1 = PathNameDynamic<AllocLocalStrict>::Create(nm1, strlen(nm1),
						      0, 0, tmpp1, PATH_MAX+1);
    tmpl2 = PathNameDynamic<AllocLocalStrict>::Create(nm2, strlen(nm2),
						      0, 0, tmpp2, PATH_MAX+1);
    rc = mpc.registerMountPoint(tmpp1, tmpl1, oh, tmpp2, tmpl2, "test", 4, 0);
    tmpp1->destroy(PATH_MAX+1);
    tmpp2->destroy(PATH_MAX+1);

    mpc.print();

    mpc.marshalToBuf(&marshBuf);
    mpc.reInit();
    err_printf("\n--- print after reInit()--- \n");
    mpc.print();
    mpc.demarshalFromBuf(&marshBuf);
    err_printf("\n--- print after deMarshal()--- \n");
    mpc.print();
}

/* type argument:
 * - VISIBLE_MP for "visible" mount points (not covered by another mp)
 * - VISIBLE_BND for "visible" binds (not covered by a mp up in the tree)
 * - UNVISIBLE for "invisible" mount points & binds  (covered by a mp up in
 *   the tree) 
 */
void
MountPointMgrCommon::MountComponent::printMtab(char *basePath,
					       VisibilityType type,
					       uval upMPUncoverable)
{
    void *iter;
    MountComponent *pChld;

    char path[PATH_MAX+1];
    if (basePath) {
	memcpy(path, basePath, strlen(basePath)+1);
	if (strlen(basePath) != 1) { // previous component is not /
	    memcpy(&path[strlen(basePath)], "/", 2);
	}
	memcpy(&path[strlen(path)], comp, strlen(comp)+1);
    } else {
	memcpy(path, "/", 2);
    }
    if (isMountPoint()) {
	if (upMPUncoverable && isCoverable()) {
	    if (type == INVISIBLE){
		err_printf("\tpath %s, %s\n", path, getDesc());
	    }
	} else {
	    if (getIsBind() == 1 && type == VISIBLE_BND ||
		getIsBind() == 0 && type == VISIBLE_MP) {
		err_printf("\tpath %s, %s\n", path, getDesc());
	    }
	}
	if (!isCoverable()) {
	    upMPUncoverable = 1;
	}
    }

    iter = children.next(NULL, pChld);
    while (iter != NULL) {
	char *tpath = (char*) AllocGlobal::alloc(PATH_MAX+1);
	memcpy(tpath, path, strlen(path)+1);
	pChld->printMtab(tpath, type, upMPUncoverable);
	iter = children.next(iter, pChld);
    };

    // free buffer received
    if (basePath) {
	AllocGlobal::free(basePath, PATH_MAX+1);
    }
}

// prints on the console information about file systems currently
// mounted (a separate list for the ones being covered by file
// systems up in the name space tree, and therefore not reachable.
// (File systems can be mounted so that they are never covered)
SysStatus
MountPointMgrCommon::printMtab()
{
    err_printf("\nList of currently accessible mount points \n");
    top.printMtab(NULL, VISIBLE_MP, 0);
    err_printf("\nList of currently accessible binds \n");
    top.printMtab(NULL, VISIBLE_BND, 0);
    err_printf("\nList of currently UNaccessible mount points and "
	       "binds (incomplete for now because we don't track mounts "
	       "over mounts yet)\n");
    top.printMtab(NULL, INVISIBLE, 0);
    return 0;
}

SysStatus
MountPointMgrCommon::getNameTreeList(
    ListSimple<ObjectHandle*, AllocGlobal> *&list)
{
    // Allocate list
    list = new ListSimple<ObjectHandle*, AllocGlobal>;

    return top.getNameTreeList(list);
}

SysStatus
MountPointMgrCommon::MountComponent::getNameTreeList(
    ListSimple<ObjectHandle*, AllocGlobal> *&list)
{
    if (isMountPoint() && getIsBind() == 0) {
	ObjectHandle *oh = getMTOH();
#if 0
	err_printf("Adding oh (%p) to the list for MP with desc %s\n", oh,
		   getDesc());
#endif
	list->add(oh);
    }

    MountComponent *pChld;
    void *iter = children.next(NULL, pChld);
    while (iter != NULL) {
	pChld->getNameTreeList(list);
	iter = children.next(iter, pChld);
    };

    return 0;
}
