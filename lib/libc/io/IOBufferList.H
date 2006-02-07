#ifndef __IOBUFFER_LIST_H_
#define __IOBUFFER_LIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IOBufferList.H,v 1.17 2001/10/05 21:47:40 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: a trivial list of buffers used by most stream
 * classes.  A Mapped buffer can have very large amounts of data, and
 * the data describing it cannot be embeded in it.  A regular buffer is
 * much smaller (limited to 64K in our implementation, and the
 * describing data can be embeded in it.
 *
 * Note, the MapBufInfo class, and the RegBufInfo class are never
 * constructed. They are initialized through explicit create calls. I
 * chose this strategy to simplify the allocation of the larger
 * templated object, where the intialization is itself specific to the
 * template.
 * **************************************************************************/


class MapBufInfo {
    uval fileOffset;			// offset of buffer in file
    uval    refs;			// number of references to region
    uval    len;			// length of region
    uval    cnt;			// valid bytes remaining
    uval    off;			// offset to next byte to access
    char   *base;			// pointer to beginning of region
public:
    void  set(MapBufInfo *init)     { *this = *init; }
    void  setFileOffset(uval fo) {fileOffset = fo; }
    void  setRefs(uval r)	    { refs = r; }
    uval  incRefs()	            { return ++refs; }
    uval  decRefs()	            { return --refs; }
    uval  getRefs()	            { return refs; }
    uval  getLen()	            { return len; }
    uval  getCnt()	            { return cnt; }
    char *getPtr()	            { return &base[off]; }
    void  incPtr(sval amt)          { off+=amt; }
    void  decCnt(sval amt)          { cnt-=amt; }
    uval  getOff()	            { return off; }
    void  setOff(uval boff)         { off=boff; }
    void  setLen(uval l)            { len=l; }
    void  setCnt(uval b)	    { cnt=b; }
    void  setBase(char * b)	    { base=b; }
    char *getBase()	            { return base; }
};

class RegBufInfo {
    uval16 len;				// length of region
    uval16 cnt;				// valid bytes remaining
    uval16 off;				// pointer from last op to reg
    uval8  refs;			// number of references to region
    char   buf[1];			// place holder for buffer
public:
    uval   incRefs()	        { return ++refs; }
    uval   decRefs()	        { return --refs; }
    uval   getRefs()	        { return refs; }
    void   setRefs(uval amt)	{ refs = amt; }
    uval   getLen()	        { return len; }
    uval   getCnt()	        { return cnt; }
    void   incPtr(sval amt)     { off+=amt; }
    void   decCnt(sval amt)     { cnt-=amt; }
    char  *getPtr()	        { return &buf[off]; }
    uval   getOff()	        { return off; }
    char  *getBase()	        { return buf; }

    void   setLen(uval l)       { len=l; }
    void   setCnt(uval b)       { cnt=b; }
    void   setOff(uval boff)    { off=boff; }
};


template <class B>
class _IOBufElem {
    _IOBufElem<B> *prev;		// previous region in region list
    _IOBufElem<B> *next;		// next region in region list
    B bf;				// actual buffer

public:
    uval   getCnt()	        { return bf.getCnt(); }
    uval   getLen()	        { return bf.getLen(); }
    char  *getPtr()	        { return bf.getPtr(); }
    uval   incRefs()            { return bf.incRefs(); }
    uval   decRefs()            { return bf.decRefs(); }
    uval   getRefs()            { return bf.getRefs(); }
    char  *getBase()	        { return bf.getBase(); }

    // increases pointer and decreases cnt by amt
    // can be used with negative values to adjust back
    void   adjPtrCnt(sval amt)  { bf.incPtr(amt); bf.decCnt(amt); }

    B             *getInfo()    { return &bf; }

    _IOBufElem<B> *getNext()    { return next; }

    _IOBufElem<B> *getPrev()    { return prev; }

    _IOBufElem<B>* insert(_IOBufElem<B> *list) {
	if (list == 0) {
	    next = prev = this;
	} else {
	    next = list; prev = list->prev;
	    list->prev->next = this; list->prev = this;
	}
	return this;
    }

    _IOBufElem<B>* append(_IOBufElem<B> *list) {
	if (list == 0) {
	    next = prev = this; list = this;
	} else {
	    next = list; prev = list->prev;
	    list->prev->next = this; list->prev = this;
	}
	return list;
    }


    _IOBufElem<B>* remove(_IOBufElem<B> *list) {
	if (next == this) {
	    list = 0;
	} else {
	    next->prev = prev; prev->next = next;
	    if (this == list) list = next;
	}
	return list;
    }


    _IOBufElem<B>* tstPtr(char *p) {
	if ((getBase() <= p) && ((bf.getBase() + bf.getLen()) > p))
	    return this;
	return 0;
    }
};

template <class BE>
class _IOBufferList {
    _IOBufElem<BE> *list;
public:
    _IOBufferList() {list = 0;}

    _IOBufElem<BE> *getHead()			{ return list; }

    _IOBufElem<BE> *getTail() { if (list) return list->getPrev(); return 0; }

    _IOBufElem<BE> *search(char *p) {
	register _IOBufElem<BE> *ptr = list;
	if (ptr) {
	    do {
		if (ptr->tstPtr(p))
		    return ptr;
		ptr = ptr->getNext();
	    } while (ptr != list);
	}
	return 0;
    }

    void append(_IOBufElem<BE> *b) { list = b->append(list); }

    void insert(_IOBufElem<BE> *b) { list = b->insert(list); }

    void remove(_IOBufElem<BE> *b) { list = b->remove(list); }
};



typedef _IOBufElem<RegBufInfo>     IOBufReg;
typedef _IOBufElem<MapBufInfo>     IOBufMap;
typedef _IOBufferList<RegBufInfo>  IOBufListReg;
typedef _IOBufferList<MapBufInfo>  IOBufListMap;

// create a new IOBufReg
inline IOBufReg *
createIOBufReg(uval size)
{
    IOBufReg *buf;
    uval sz = size;
    sz += sizeof(IOBufReg)-1;

    buf = (IOBufReg *)allocGlobal(sz,sz);

    // Note: size returned by allocGlobal may be larger than sz passed
    (buf->getInfo())->setLen(sz-sizeof(IOBufReg)+1);

    // no other initialization is done, since can be avoided often in
    // stream specific fashion
    return buf;
}

inline void
deleteIOBufReg(IOBufReg *bf)
{
    uval size = (bf->getInfo())->getLen();
    size += sizeof(IOBufReg)-1;
    freeGlobal(bf, size);
}

// create a new IOBufMap
inline IOBufMap *
createIOBufMap(MapBufInfo *info)
{
    IOBufMap *buf;
    buf = (IOBufMap *)allocGlobal(sizeof(IOBufMap));

    (buf->getInfo())->set(info);

    // no other initialization, since is object specific
    return buf;
}

inline void
deleteIOBufMap(IOBufMap *bf)
{
    freeGlobal(bf, sizeof(IOBufReg));
}

#endif /* #ifndef __IOBUFFER_LIST_H_ */
