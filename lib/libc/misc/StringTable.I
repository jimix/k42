#ifndef __STRING_TABLE_I_
#define __STRING_TABLE_I_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StringTable.I,v 1.27 2003/02/13 14:42:09 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of StringTable template
 * **************************************************************************/

//#define DILMA_DEBUG

template <class SH>
void
STE<SH>::init(uval slen, uval avail)
{
    space = avail;
    sh.strLen = slen;
    tassert((slen>0), err_printf("this is gonna cause problems\n"));
}

template <class SH>
void
STE<SH>::init(SH *newent, uval avail)
{
    space = avail;
    sh    = *newent;
    tassert((newent->strLen>0), err_printf("this is gonna cause problems\n"));
    memcpy(sh.str, newent->str, newent->strLen);
}

template <class SH>
SH *
STE<SH>::allocEntry(uval len)
{
    STE<SH> *nxt;
    uval avail = space - sizeNeed();

    // should really do this as a fast in-line check in header
    if (sizeNeed(len) > avail) {
	return 0;
    }

    space = sizeNeed();
    nxt = getNext();
    nxt->init(len, avail);
    return &nxt->sh;
}


template <class SH>
SH *
STE<SH>::allocEntry(SH *newent)
{
    STE<SH> *nxt;
    uval avail = space - sizeNeed();

    // should really do this as a fast in-line check in header
    if (sizeNeed(newent->strLen) > avail) {
	return 0;
    }

    space = sizeNeed();
    nxt = getNext();
    nxt->init(newent, avail);
    return nxt->getContents();
}


/* static */
template <class SH>
_StrTable<SH> *
_StrTable<SH>::Init(SH *first, void *buf, uval blen)
{
    _StrTable<SH> *ptr = (_StrTable<SH> *)buf;
    ptr->numEntries  = 1;		// always start off with one entry
    ptr->bufLen = blen;
    ptr->se.init(first, blen-sizeHeader());
    return ptr;
}

/* static */
template <class SH>
SH *
_StrTable<SH>::Init(uval strlen, void *buf, uval blen)
{
    _StrTable<SH> *ptr = (_StrTable<SH> *)buf;
    ptr->numEntries  = 1;		// always start off with one entry
    ptr->bufLen = blen;
    ptr->se.init(strlen, blen-sizeHeader());
    return ptr->se.getContents();
}

template <class SH>
SH *
_StrTable<SH>::allocEntry(uval slen)
{
    STE<SH> *ptr = &se;
    SH *newEntry;
    for (uval i=0; i<numEntries ; i++) {
	if ((newEntry = ptr->allocEntry(slen)) != 0) {
	    numEntries++;
	    return newEntry;
	}
	ptr = ptr->getNext();
    }
    return 0;
}

template <class SH>
SH *
_StrTable<SH>::allocEntry(SH *newCont)
{
    STE<SH> *ptr = &se;
    SH *newEntry;
    for (uval i=0; i<numEntries ; i++) {
	if ((newEntry = ptr->allocEntry(newCont)) != 0) {
	    numEntries++;
	    return newEntry;
	}
	ptr = ptr->getNext();
    }
    return 0;
}

template <class SH>
SysStatus
_StrTable<SH>::deleteEntryWithPtr(SH *old)
{
#ifdef DILMA_DEBUG
    err_printf("deleteEntryWithPtr %p\n", old);
#endif // ifdef DILMA_DEBUG

    // quick check to see if it is in buffer
    if (((uval)old < (uval)this) ||
	((uval)old >= ((uval)this+bufLen)))
	return _SERROR(1006, 0, ENOENT);

    // is in correct range, make sure not deleting first
    tassert((numEntries>1), err_printf("attempt to delete 0 entry\n"));

    STE<SH> *prev = &se;
    STE<SH> *cur  = se.getNext();

    for (uval i=1; i<numEntries ; i++) {
	if (cur->getContents() == old) {
	    prev->freeNext();
	    numEntries--;
	    return 0;
	}
	prev = cur;
	cur = cur->getNext();
    }
    return _SERROR(1498, 0, ENOENT);
}

template <class SH>
SysStatus
_StrTable<SH>::deleteEntryWithContents(SH *old)
{
#ifdef DILMA_DEBUG
    err_printf("deleteEntryWithContents\n");
#endif // ifdef DILMA_DEBUG

    STE<SH> *prev = &se;
    STE<SH> *cur  = se.getNext();
    if (numEntries < 2)
	return _SERROR(1008, 0, ENOENT);

    for (uval i=1; i<numEntries ; i++) {
	if (((cur->getContents()->strLen) == old->strLen) &&
	    (strncmp(old->str, cur->getContents()->str, old->strLen)==0)) {
	    prev->freeNext();
	    numEntries--;
	    return 0;
	}
	prev = cur;
	cur = cur->getNext();
    }
    return _SERROR(1007, 0, ENOENT);
}

template <class SH, class ALLOC>
_StrTable<SH> *
_DynamicStrTable<SH,ALLOC>::copyEntries(void *to, uval blen)
{
#ifdef DILMA_DEBUG
    err_printf("In copyEntries with blen %ld\n", blen);
    breakpoint();
#endif // ifdef DILMA_DEBUG

    STE<SH> *ptr = table->getHead();
    uval i=1;

    _StrTable<SH> *newtab =
	_StrTable<SH>::Init(ptr->getContents(), to, blen);
    uval numEntries = table->getNumEntries();
    while (i++<numEntries) {
	ptr = ptr->getNext();
	SH *newent = newtab->allocEntry(ptr->getContents());
	tassert((newent != 0), err_printf("not sufficient room\n"));
    }
    return newtab;
}


template <class SH, class ALLOC>
void
_DynamicStrTable<SH,ALLOC>::doubleSize()
{
#ifdef DILMA_DEBUG
    err_printf("*********************** In doubleSize()\n");
#endif // #ifdef DILMA_DEBUG
    uval oldsize = table->getBufLen();
    uval newsize = oldsize*2;
    void *buf = ALLOC::alloc(newsize);
#ifdef DILMA_DEBUG
    err_printf("doubled string buffer size from %ld<%p> to %ld %p->0x%lx\n",
	       oldsize, table, newsize, buf, (uval)buf+newsize);
#endif // #ifdef DILMA_DEBUG
    _StrTable<SH> *newtab = copyEntries(buf,newsize);
    ALLOC::free(table, oldsize);
    table = newtab;
}

template <class SH, class ALLOC>
SH *
_DynamicStrTable<SH,ALLOC>::allocEntry(uval len)
{
#ifdef DILMA_DEBUG
    err_printf("Allocating entry of size %ld\n", (uval) len);
#endif // ifdef DILMA_DEBUG

    SH *p = table->allocEntry(len);
    while (!p) {
	doubleSize();
	p = table->allocEntry(len);
    }
    return p;
}

template <class SH, class ALLOC>
SH *
_DynamicStrTable<SH,ALLOC>::allocEntry(SH *newCont)
{
#ifdef DILMA_DEBUG
    err_printf("Allocating entry with newCont %ld\n");
#endif // ifdef DILMA_DEBUG

    SH *p = table->allocEntry(newCont);
    while (!p) {
	doubleSize();
	p = table->allocEntry(newCont);
    }
    tassert(p, err_printf("woops\n"));
    return p;
}

template <class SH, class ALLOC>
void
_DynamicStrTable<SH,ALLOC>::init(SH *first)
{
    uval required = STE<SH>::sizeNeed(first->strLen)+sizeof(_StrTable<SH>);
    uval amount = required;
    void *buf = ALLOC::alloc(amount);
    table = _StrTable<SH>::Init(first, buf, amount);
}

template <class SH, class ALLOC>
SH *
_DynamicStrTable<SH,ALLOC>::init(uval strLen)
{
    uval required = STE<SH>::sizeNeed(strLen)+sizeof(_StrTable<SH>);
    uval amount = required;
    void *buf = ALLOC::alloc(amount);
    table = (_StrTable<SH> *)buf;
    return _StrTable<SH>::Init(strLen, buf, amount);
}


#endif /* #ifndef __STRING_TABLE_I_ */
