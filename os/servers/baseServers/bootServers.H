#ifndef __BOOT_SERVERS_H_
#define __BOOT_SERVERS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bootServers.H,v 1.8 2001/10/05 21:51:38 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Mechanisms needed in booting an initial set of
 * servers.
 * **************************************************************************/

struct bootServerHeader {
private:
    enum {MAX_NAME_LENGTH = 64};
    typedef char asciiInt[24];

    char theName[MAX_NAME_LENGTH + 1];	// leave room for null-terminator
    asciiInt theOffset;
    asciiInt theSize;

public:
    void name(char *s) {strncpy(theName, s, MAX_NAME_LENGTH);
			    theName[MAX_NAME_LENGTH] = '\0'; }
    char *name(void) { return theName; }

    void offset(int value) { sprintf(theOffset, "%u", value); }
    int offset(void) { return atoi(theOffset); }

    void size(int value) { sprintf(theSize, "%u", value); }
    int size(void) { return atoi(theSize); }
};

#endif /* #ifndef __BOOT_SERVERS_H_ */
