#ifndef __PATH_NAME_H_
#define __PATH_NAME_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PathName.H,v 1.40 2003/06/20 19:47:23 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Class used to parse pathnames, to make for easy
 * parsing, a component of a pathname is itself a pathname. The length (in
 * bytes) of the PathName is maintained outside of the
 * PathName itself, and it is passed in as a paramenter on
 * many calls (for efficienty, and also to deal properly with empty paths).
 * This "external" length is the only way to determine the end of a path.
 *
 * Methods that allocate memory for the pathname have bufLen as an
 * argument. Its default value is 0, meaning that the method should
 * allocate just enough memory to hold the data it wants to
 * store. Use of this methods that require a maximum size buffer (currently
 * PATH_MAX+1 - large enough to hold a maximal path
 * since each component but first must be preceeded by "/".  The "/"
 * is not represented, but is replaced by one byte representing
 * length) should explicitly provide this bufLen to the call.
 *
 * N.B. an empty path is represented by a pathLen of 0.  If the maxLen is
 * also 0, the PathName* can be NULL
 * **************************************************************************/

#include <limits.h>


class PathName {
public:
    uval8 nmLen;			// length of first component
    char  nm[1];			// string name first component

    // minimum path name is 1 character len, 1 character contents
    // and a null character
    static uval IsEmpty(PathName *, uval thisLen) {return (thisLen == 0);}

    // returns the length of a single component
    uval getCompLen(uval thisLen) {
	if (thisLen) {
	    return nmLen;
	}
	return 0;
    }

    // get pointer to component name (don't use this much, since interface
    // will change with wide char)
    char *getCompName(uval thisLen) {
	if (thisLen) {
	    return &nm[0];
	}
	return 0;
    }

    // returns 1 if character string matches component
    uval matchComp(uval thisLen, char *str, uval len);

    // gets next part of PathName
    PathName *getNext(uval thisLen) {
	if (thisLen) {
	    return (PathName *)((uval)this + nmLen + sizeof(uval8));
	}
	return 0;
    }

    // true if is last component of pathname
    uval isLastComp(uval thisLen) { return (thisLen==((uval)nmLen+1)); }

    // true if comp is last comp of this path
    uval isLastComp(uval thisLen, PathName* comp) {
	return ((uval(comp)-uval(this)+comp->nmLen+1)==thisLen); }

    // true if this is a component
    // (does NOT parse to make sure it's a valid component)
    uval isComp(uval thisLen, PathName* comp) {
	return ((comp >= this) && ((uval(this)+thisLen)>uval(comp))); }

    // returns length of path starting at component - no error checks!X
    uval compPathLen(uval thisLen, PathName* comp) {
	return(uval(this)+thisLen-uval(comp)); }

    // gets next part of PathName, and remaining length
    PathName *getNext(uval thisLen, uval &remLen) {
	if (thisLen) {
	    PathName *nxt = getNext(thisLen);
	    remLen = thisLen - ((uval)nxt - (uval)this);
	    if (remLen== 0) {
		return 0;
	    }
	    return nxt;
	}
	return 0;
    }

    // in the pathname, get the component previous to the one
    // ``ptr'' points to
    // if ptr does not point to a component of the path, returns 0
    PathName *getPrev(uval thisLen, PathName *ptr);

    // get starting pathname, i.e. the pathname up to a ptr in path
    // a null ptr is assumed to mean that you want all but the 
    // last componenet
    PathName *getStart(uval thisLen, PathName *ptr, uval &resLen);

    // appends first component of passed path returns new pathLen
    SysStatusUval appendComp(uval thisLen, uval bufLen, PathName *pth);

    // appends string to path
    SysStatusUval appendComp(uval thisLen, uval bufLen, char *str,
			     uval strLen);

    // appends path, returns length, and start of appended portion
    SysStatusUval appendPath(uval thisLen, uval thisBufLen, PathName *pth,
			     uval pthLen, PathName *&np);

    // appends path, returns length
    SysStatusUval appendPath(uval thisLen, uval thisBufLen,
			     PathName *pth, uval pthLen);

    // prepends path, moving current stuff right to make room, returns length
    SysStatusUval prependPath(uval thisLen, uval bufLen,
			      PathName *pth, uval pthLn);

    // prepends path, moving current stuff right to make room, returns length
    // and offset of original in its new pushed right location
    SysStatusUval prependPath(uval thisLen, uval bufLen,
			      PathName *pth, uval pthLn, PathName*&np);

    // duplicates PathName into buffer, typecheats buffer and returns
    PathName *dup(uval thisLen, char *buf, uval bufLen);

    // creates a unix path in a buffer, returns len (counting the \0
    // at the end of string) or error if buf
    // too small, grab up to (not including to), or all if to == 0
    SysStatusUval getUPath(uval thisLen, char *buf, uval bufLen,
			   PathName *to=0);

    // typecheats a buffer that we know contains a PathName
    // also does sanity checking on the path, including that len matches
    static SysStatus PathFromBuf(char *buf, uval len, PathName *&np);

    // typecheats to a buffer that can be passed around.
    // (There are places where this method may be invoked through a NULL
    // pointer.)
    char *getBuf()   { return (char *)this; }
};


template<class ALLOC>
class PathNameDynamic : public PathName {
public:
    // gets next part of PathName
    PathNameDynamic<ALLOC> *getNext(uval thisLen) {
	return (PathNameDynamic<ALLOC> *)getNext(thisLen);
    }

    // split PathName at ptr, returns two new pathnames.
    // The creation will allocate a buffer with the exact size needed
    void splitCreate(uval thisLen, const PathName *ptr,
		     PathNameDynamic<ALLOC> * &part1, uval &len1,
		     PathNameDynamic<ALLOC> * &part2, uval &len2);


    // split PathName at ptr, returns starting pathname, returns len of
    // new starting point. The creation will allocate a buffer with
    // the exact size needed
    uval splitCreate(uval thisLen, const PathName *ptr,
		     PathNameDynamic<ALLOC> * &part1);

    // returns length of new PathName.
    // bufLen == 0 means that memory should be allocated with the
    // exact size needed for the pathname being duped
    uval dupCreate(uval thisLen, PathNameDynamic<ALLOC> * &np, uval bufLen=0);


    // creates a new PathName from first component. The new PathName
    // buffer is allocated with the exact size to fit the component
    uval createFromComp(uval thisLen, PathNameDynamic<ALLOC> * &np);

    // creates a PathName, appending unix pathname onto a start path,
    // returns length of created PathName (startLen may be 0).
    // upLen is the strlen of the unix path
    // bufLen == 0 implies that the buffer the pathName is placed in
    // is created  with exactly the necessary size. If a bufLen != 0
    // is provided, the specified size is used to create the
    // buffer; in this the user of the pathName is supposed keep
    // track of the provided bufLen and use it on destruction
    // if useBuf is provided, that buffer is used and returned
    static uval Create(const char *unixPath, uval upLen,
		       PathNameDynamic<ALLOC> *start, uval startLen,
		       PathNameDynamic<ALLOC> * &np,
		       uval bufLen=0, char *useBuf = NULL);

    // creates a new PathName by appending passed path to this path,
    // returns length
    // len is length of original path (N.B. create is a method on a path)
    // buffer size of new path will be bufLen.  bufLen=0 implies
    // minimum buffer needed to hold result.
    uval create(uval thisLen, PathName *pth, uval pthLen,
		PathNameDynamic<ALLOC> * &np, uval bufLen=0);

    // creates a new PathName by appeneding first component of passed path
    // returns length
    uval appendCompCreate(uval thisLen, PathName *pth,
			  PathNameDynamic<ALLOC> * &np, uval bufLen=0);

    // creates a new PathName by appending component, returns length
    uval appendCompCreate(uval thisLen, char *str, uval strLen,
			  PathNameDynamic<ALLOC> * &np, uval bufLen=0);

    void destroy(uval thisLen);
};

#endif /* #ifndef __PATH_NAME_H_ */
