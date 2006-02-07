#ifndef __STRING_SHORT_H_
#define __STRING_SHORT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StringShort.H,v 1.6 2001/10/05 21:47:50 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple 7 char plus null byte string class
 * **************************************************************************/

class StringShort {
    enum { LEN=8 };			// length of string == 1 reg == 64bits
    char string[LEN];			// string length (includes null)

public:
    char *getString()		{ return string; }
    void  setString(const char *s)
    {
	int i = 0;
	while((i < LEN) && ((string[i] = s[i]) != 0)) {
	    i++;
	}
	string[LEN-1] = '\0';		// simple and safe
    }
    operator char*() {return getString();}
    StringShort(const char *s)     {setString(s);}
    StringShort() { }
};
#endif /* #ifndef __STRING_SHORT_H_ */
