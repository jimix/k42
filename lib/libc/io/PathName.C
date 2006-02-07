/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PathName.C,v 1.57 2005/08/22 22:35:09 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Note, for now all the implementations are
 * trivial, since coincidentally dealing with a null terminated
 * string.  With multi-byte characters these will get more complex.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "PathName.H"

#if 0
#define paranoid tassert
#else
#define paranoid(EX,STMT)
#endif

uval
PathName::matchComp(uval thisLen, char *str, uval len)
{
    if (thisLen) {
	if (len == nmLen) {
	    if (memcmp(str, nm, len) != 0)
		return 0;
	    return 1;
	}
    }
    // empty string
    return len == 0;
}

template<class ALLOC>
void
PathNameDynamic<ALLOC>::splitCreate(uval thisLen, const PathName *ptr,
				    PathNameDynamic<ALLOC> * &part1,
				    uval &len1,
				    PathNameDynamic<ALLOC> * &part2,
				    uval &len2)
{
    if (thisLen) {
	// partial sanity check
	tassert((this <= ptr && ptr < this + thisLen),
		err_printf("splitCreated invoked with this==%px,ptr==%px\n",
			   this, ptr));

	len1 = uval(ptr)-uval(this);
	len2 = thisLen-len1;

	if (len1) {
	    part1 = (PathNameDynamic<ALLOC> *)ALLOC::alloc(len1);
	    memcpy(part1, this, len1);
	} else {
	    part1 = 0;
	    len1 = 0;
	}

	if (len2) {
	    part2 = (PathNameDynamic<ALLOC> *)ALLOC::alloc(len2);
	    memcpy(part2, ptr, len2);
	} else {
	    part2 = 0;
	    len2 = 0;
	}
    } else {
	tassertWrn(0, "splitCreate with thisLen==0\n");
	part1 = part2 = 0;
	len1 = len2 = 0;
    }
}

template<class ALLOC>
uval
PathNameDynamic<ALLOC>::splitCreate(uval thisLen, const PathName *ptr,
				    PathNameDynamic<ALLOC> * &part1)
{
    if (thisLen) {
	// partial sanity check
	tassert((this <= ptr && ptr < this + thisLen),
		err_printf("splitCreated invoked with this==%px,ptr==%px\n",
			   this, ptr));
	uval len1 = uval(ptr)-uval(this);
	if (len1) {
	    part1 = (PathNameDynamic<ALLOC> *)ALLOC::alloc(len1);
	    memcpy(part1, this, len1);
	    return len1;
	}
    }

    part1 = 0;
    return 0;
}

template<class ALLOC>
uval
PathNameDynamic<ALLOC>::dupCreate(uval thisLen, PathNameDynamic<ALLOC> * &newp,
				  uval bufLen)
{
    if (thisLen) {
	if (bufLen) {
	    newp = (PathNameDynamic<ALLOC> *)ALLOC::alloc(bufLen);
	} else {
	    newp = (PathNameDynamic<ALLOC> *)ALLOC::alloc(thisLen);
	}
	memcpy(newp, this, thisLen);
	return thisLen;
    } else {
	if (bufLen) {
	    // even for empty, we need to allocate the specified buffer size
	    newp = (PathNameDynamic<ALLOC> *)ALLOC::alloc(bufLen);
	} else {
	    newp = 0;
	}
	return 0;
    }
}

template<class ALLOC>
uval
PathNameDynamic<ALLOC>::createFromComp(uval thisLen,
				       PathNameDynamic<ALLOC> * &np)
{
    if (thisLen) {
	uval len = nmLen+1;
	np = (PathNameDynamic<ALLOC> *)ALLOC::alloc(len);
	memcpy(np, this, len);
	return len;
    } else {
	np = 0;
	return 0;
    }
}


PathName *
PathName::getPrev(uval thisLen, PathName *p)
{
    tassert(this != 0, err_printf("getPrev called for NULL path\n"));

    PathName *tmp = this;

    // if try to back up past beginning, return beginning
    if (tmp == p) return tmp;

    // if empty path, return beginning
    if (!thisLen) return tmp;

    while (thisLen != 0 && tmp->getNext(thisLen) != p) {
	tmp = tmp->getNext(thisLen, thisLen);
    }

    if (thisLen) {
	return tmp;
    } else {
	// could not find p
	tassertMsg(0, "getPrev failed\n");
	return 0;
    }
}

// This returns np == 0 if there is not space for this
template<class ALLOC>
uval
PathNameDynamic<ALLOC>::Create(const char *unixPath, uval upLen,
			       PathNameDynamic<ALLOC> *start, uval startLen,
			       PathNameDynamic<ALLOC> * &np,
			       uval bufLen /* = 0 */, 
			       char *useBuf /* = NULL */)
{
    /* if bufLen is 0 we are expected to compute
     * the necessary buffer length; if not it indicates the buffer
     * size to be allocated.
     */

    uval expectedSize;     /* expectedSize is the largest it can
			      need, given the input.*/


    uval realSize;

    if (bufLen) {
	if (startLen > bufLen || upLen >= bufLen) {
	    np = NULL;
	    return 0;
	}
	expectedSize = bufLen;
    } else {
	expectedSize = upLen + 1 + startLen;
    }

    if (useBuf == NULL) {
	np = (PathNameDynamic<ALLOC> *)ALLOC::alloc(expectedSize);
    } else {
	np = (PathNameDynamic<ALLOC> *)useBuf;
	tassertMsg( (bufLen > 0), "with useBuf, need to specify lengh\n");
    }

    uval npLen = 0;       // stores current length of np
    PathName *ptr = np;	  // variable points to current location
    ptr->nmLen = 0;       // np gets prepared for returning an empty path if
                          // needed
    uval maxptr = uval(np) + expectedSize;

    // copy over last string
    if (startLen) {
	memcpy(np, start, startLen);
	npLen = startLen;
	ptr = (PathName *)(uval(np) + startLen);
    }

    const char *frp = unixPath;
    uval len = upLen;

    // now grab each component, from unix path and process
    while (len) {
	uval prevLen;
	do {
	    prevLen = len;
	    if (*frp == '.') {
		if (*(frp+1) == '\0') {
		    ++frp;
		    --len;
		} else if (*(frp+1) == '/') {
		    frp += 2;
		    len -= 2;
		} else if (*(frp+1) == '.' &&
			   (*(frp+2) == '\0') || (*(frp+2) == '/')) {
		    frp += 2;
		    len -= 2;
		    // pop off an element
		    ptr = np->getPrev(npLen, ptr);
		    tassert(ptr != 0,
			    err_printf("not dealing with errors yet!\n"));
		    npLen -= ptr->nmLen - sizeof(uval8);
		    tassert(npLen>=0, err_printf("bug in implementaion\n"));
		}
	    }
	    if ((len) && (*frp == '/')) {
		frp++ ; len--;
	    }
	} while ((len) && (prevLen!=len));

	if (len == 0) break;			// done

	// okay done with noise copy over the good stuff
	if (uval(ptr) >= maxptr) goto overrun;

	char *top = ptr->nm;
	ptr->nmLen = 0;
	while ((len) && (*frp != '/')) {
	    if (ptr->nmLen > 254) {
		tassertWrn(0, "component in path name too long (max is 255)\n");
		np->destroy(expectedSize);
		np = NULL;
		return 0;
	    }
	    len--;
	    if (uval(top) >= maxptr) goto overrun;
	    *top++ = *frp++;
	    ptr->nmLen++;
	}
	if (ptr->nmLen) { // done something, get prepared for the next one
	    npLen = npLen + ptr->nmLen + sizeof(uval8);
	    ptr = ptr->getNext(ptr->nmLen);
	    if (len != 0) { // there is another component to be added
		if (uval(ptr) >= maxptr) goto overrun;
		ptr->nmLen = 0;
	    }
	}
    }
    realSize = uval(ptr)-uval(np);

    // check if we really wanted a smaller buffer
    // N.B. if user specified a size accept it
    if (!bufLen && (realSize < expectedSize)) {
	if (realSize) {
	    PathNameDynamic<ALLOC> *tmp = (PathNameDynamic<ALLOC> *)
		ALLOC::alloc(realSize);
	    memcpy(tmp, np, realSize);
	    np->destroy(expectedSize);
	    np = tmp;
	} else {
	    // empty path
	    np->destroy(expectedSize);
	    np = NULL;
	}
	expectedSize = realSize;
    }
    tassert((realSize<=expectedSize), err_printf("internal error\n"));
    return realSize;

overrun:
    tassert(0, err_printf("path does not fit in provided buffer-"
	       "bufLen %ld, upLen %ld, startLen %ld\n",
	       bufLen, upLen, startLen));
    np->destroy(expectedSize);
    np = NULL;				// hopefully null will break something
    return 0;
}

SysStatus
PathName::PathFromBuf(char *buf, uval len, PathName *&np)
{
    np = (PathName *)buf;

    /* servers depend on validate to avoid disaster
     * if clients pass in a bogus path buffer
     */
    PathName *ptr = np;
    while (1) {
	if ((uval(ptr)-uval(np))>len) {
	    tassertMsg(0, "bad path name, returning EINVAL");
	    return _SERROR(1160, 0, EINVAL);
	}
	if ((uval(ptr)-uval(np))==len) {
		return 0;
	}
	// len is not zero, so we can get current length at ptr->nmLen
	ptr = ptr->getNext(ptr->nmLen);
    }
}

SysStatusUval
PathName::getUPath(uval thisLen, char *buf, uval bufLen, PathName *to)
{
    sval bLenlft=bufLen;
    PathName *pnp = this;
    PathName *end = (PathName*)(uval(pnp)+thisLen);
    char *ptr = buf;

    if (pnp == end) {
	*ptr++ = '/';
    } else {
	while ((pnp<end) && (pnp!=to)) {
	    *ptr++ = '/';
	    bLenlft -= (pnp->nmLen+1);
	    if (bLenlft < 0)
		return _SERROR(1049, 0, ERANGE);
	    memcpy(ptr,pnp->nm,pnp->nmLen);
	    ptr += pnp->nmLen;
	    pnp = pnp->getNext(pnp->nmLen);
	}
    }
    *ptr++ = '\0';
    return (uval)(ptr-buf);
}

template<class ALLOC>
uval
PathNameDynamic<ALLOC>::create(uval thisLen, PathName *pth, uval pthLen,
			       PathNameDynamic<ALLOC> * &np,
			       uval bufLen)
{
    uval reqLen=thisLen+pthLen;
    if (bufLen) {
	/* this shouldn't happen so just return values that will break
	 * the caller quickly
	 */
	if (bufLen < (thisLen+pthLen)) {
	    np = NULL;
	    return 0;
	}
    } else {
	bufLen = reqLen;
    }

    // don't want to change length, so use alloc

    np = (PathNameDynamic<ALLOC> *)ALLOC::alloc(bufLen);
    memcpy(np, this, thisLen);
    memcpy((void *)(uval(np)+thisLen),pth,pthLen);

    return reqLen;
}

template<class ALLOC>
uval
PathNameDynamic<ALLOC>::appendCompCreate(uval thisLen, char *str, uval strLen,
					 PathNameDynamic<ALLOC> * &np,
					 uval bufLen)
{
    // add strLen of component plus room for len
    uval reqLen = thisLen + strLen + 1;

    if (bufLen) {
	if (bufLen < reqLen) {
	    /* this shouldn't happen so just return values that will break
	     * the caller quickly
	     */
	    np = NULL;
	    return 0;
	}
    } else {
	bufLen = reqLen;
    }

    // don't want to change length, so use alloc
    np = (PathNameDynamic<ALLOC> *)ALLOC::alloc(bufLen);
    if (thisLen) {
	memcpy(np, this, thisLen);
    }
    memcpy((void *)(uval(np)+thisLen+1),str,strLen);
    PathName *tmp = (PathName *)(uval(np)+thisLen);
    tmp->nmLen = strLen;

    return reqLen;
}


template<class ALLOC>
uval
PathNameDynamic<ALLOC>::appendCompCreate(uval thisLen, PathName *pth,
					 PathNameDynamic<ALLOC> * &np,
					 uval bufLen)
{
    return appendCompCreate(thisLen, pth->nm, pth->nmLen, np, bufLen);
}

PathName *
PathName::getStart(uval thisLen, PathName *ptr, uval &resLen) 
{
    if (unlikely(thisLen == 0)) {
	resLen = 0;
	return this;
    }

    if (ptr == NULL) {
	PathName *tmp, *next;
	uval remlen, remnext;
	
	tmp = this;
	remlen =  thisLen;
	next = tmp->getNext(remlen, remnext);
	while (next != NULL) {
	    tmp = next;
	    remlen = remnext;
	    next = tmp->getNext(remlen, remnext);
	}
	resLen = (uval)tmp - (uval)this;
	return this;
    }

    resLen = (uval)ptr - (uval)this;
    return this;
}


SysStatusUval
PathName::appendComp(uval thisLen, uval bufLen, char *str, uval strLn)
{
    // add strLen of component plus room for nmLen
    uval reqLen = thisLen + strLn + 1;

    if (bufLen < reqLen) {
	return _SERROR(1503, 0, ERANGE);
    }

    memcpy((void *)(uval(this)+thisLen+1),str,strLn);

    PathName *pth = (PathName *)(uval(this)+thisLen);
    pth->nmLen = strLn;

    return reqLen;
}

SysStatusUval
PathName::appendPath(uval thisLen, uval thisBufLen, PathName *pth, uval pthLen,
		     PathName *&np)
{
    // add strLen of component plus room for nmLen
    uval reqLen = thisLen + pthLen;

    if (thisBufLen < reqLen)
	return _SERROR(1504, 0, ERANGE);

    memcpy((void *)(uval(this)+thisLen), pth, pthLen);
    np = (PathName *)(uval(this)+thisLen);

    return reqLen;
}

SysStatusUval
PathName::appendPath(uval thisLen, uval thisBufLen, PathName *pth, uval pthLen)
{
    // add strLen of component plus room for nmLen
    uval reqLen = thisLen + pthLen;

    if (unlikely(thisBufLen < reqLen)) {
	return _SERROR(1052, 0, ERANGE);
    }

    memcpy((void *)(uval(this)+thisLen),pth,pthLen);

    return reqLen;
}

SysStatusUval
PathName::prependPath(uval thisLen, uval bufLen,
		      PathName *pth, uval pthLen, PathName *&np)
{
    if (bufLen < (thisLen+pthLen)) {
	return _SERROR(1155, 0, ERANGE);
    }

    // figure out where current path will wind up - past end
    // of prepended path whose length is pthLen
    np = (PathName*)(uval(this)+pthLen);
    //memmove works even if overlap
    memmove(np,this,thisLen);
    memcpy(this,pth,pthLen);

    return thisLen+pthLen;
}

SysStatusUval
PathName::prependPath(uval thisLen, uval bufLen, PathName *pth, uval pthLen)
{
    if (bufLen < (thisLen+pthLen)) {
	return _SERROR(1508, 0, ERANGE);
    }
    PathName* np = (PathName*)(uval(this)+pthLen);
    //memmove works even if overlap
    memmove(np,this,thisLen);
    memcpy(this,pth,pthLen);
    return thisLen+pthLen;
}

PathName *
PathName::dup(uval thisLen, char *buf, uval bufLen)
{
    PathName *pth = (PathName *)buf;
    tassertWrn((thisLen <= bufLen), "buffer too small\n");
    if (thisLen > bufLen) return NULL;

    memcpy(buf, (void *)(uval(this)), thisLen);

    return pth;
}

SysStatusUval
PathName::appendComp(uval thisLen, uval bufLen, PathName *pth)
{
    return appendComp(thisLen, bufLen, pth->nm, pth->nmLen);
}

template<class ALLOC>
void
PathNameDynamic<ALLOC>::destroy(uval thisLen)
{
    if (thisLen != 0) {
	ALLOC::free(this, thisLen);
    }
}

// specific instantiations
template class PathNameDynamic<AllocLocalStrict>;
template class PathNameDynamic<AllocGlobal>;
template class PathNameDynamic<AllocGlobalPadded>;
template class PathNameDynamic<AllocPinnedLocalStrict>;
template class PathNameDynamic<AllocPinnedGlobal>;
template class PathNameDynamic<AllocPinnedGlobalPadded>;
