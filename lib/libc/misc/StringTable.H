#ifndef __STRING_TABLE_H_
#define __STRING_TABLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StringTable.H,v 1.22 2003/02/13 14:42:09 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple class for managing storage of structures
 * containing strings.
 * **************************************************************************/

// definition for class that normally holds variable length structures
// with a string as their last element, see defintion at end
template <class SH>
class STE {
    uval       space __attribute__((__packed__)); // space allocated to entry
    SH sh;				// actual string holder

public:
    // returns the required size for a str len, hack to remove 1 byte
    // space used as a space holder for string
    // note, align up so always allocate on 64 bit boundary
    static uval sizeNeed(uval len) {
	return ALIGN_UP(sizeof(STE<SH>)-sizeof(char)+len, sizeof(uval64)); }

private:
    // returns the required size of this entry
    uval sizeNeed() { return sizeNeed(sh.strLen); }

public:

    void init(uval slen, uval bufsize);

    void init(SH *cont, uval bufsize);

    STE<SH> *getNext()        { return (STE<SH> *)(((uval)this) + space); }

    uval getSpace()           { return space; }

    void freeNext()           { space += getNext()->space; }

    SH *getContents()         { return &sh; }

    SH *allocEntry(uval len);

    SH *allocEntry(SH *newent);
};


template <class SH>
class _StrTable {
    uval       bufLen;		// length of this buffer
    uval       numEntries;		// number of valid entries
    // note, chose uval above so that aligment of all
    // entries will be 64 bytes
    static uval sizeHeader() {return 2 * sizeof(uval);}
    STE<SH>      se;			// first entry in buffer (never 0 len)
public:
    // have to provide a first entry that can never be deleted
    static _StrTable<SH> *Init(SH *first, void *buf, uval blen);
    static SH *Init(uval strlen, void *buf, uval blen);

    STE<SH> *getHead() { return &se; }

    SH *getHeadData() { return se.getContents(); }

    uval getBufLen() {return bufLen;}

    uval getNumEntries() {return numEntries;}

    // allocate a new entry with the specified length,
    SH *allocEntry(uval len);

    // allocate a new entry with the contents passed
    SH *allocEntry(SH *newCont);

    // delete entry with pointer passed, 0 on success
    SysStatus deleteEntryWithPtr(SH *old);

    // delete entry with name and len passed (rest of contents ignored)
    SysStatus deleteEntryWithContents(SH *old);
};

template <class SH, class ALLOC>
class _DynamicStrTable {
    _StrTable<SH> *table;

    void doubleSize();

public:
    // have to provide a first entry that can never be deleted
    void init(SH *first);
    SH *init(uval strlen);

    _StrTable<SH> *copyEntries(void *to, uval blen);

    uval getBufLen() {return table->getBufLen();}

    uval getNumEntries() {return table->getNumEntries();}

    STE<SH> *getHead() { return table->getHead(); }

    SH *getHeadData() { return table->getHeadData(); }

    // allocate a new entry with the specfified length
    SH *allocEntry(uval len);

    // allocate a new entry with the contents passed
    SH *allocEntry(SH *newCont);

    // delete entry with pointer passed, 0 on success
    SysStatus deleteEntryWithPtr(SH *old)
    { return table->deleteEntryWithPtr(old); }

    // delete entry with name and len passed (rest of contents ignored)
    SysStatus deleteEntryWithContents(SH *old)
    { return table->deleteEntryWithContents(old); }
};

#endif /* #ifndef __STRING_TABLE_H_ */
