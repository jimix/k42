#ifndef __PPCBAK_H_
#define __PPCBAK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppcbak.H,v 1.5 2001/10/05 21:49:41 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/


// need a structure for each Processor.
class PPCBakPagePool {
    static const uval num_bak_pages;
    uval nextPage;
public:
    void init();
    inline uval pop() {
	uval p = nextPage;
	nextPage = *(uval*)nextPage;
	return p;
    }
    inline void push(uval p) {
	*(uval*)p = nextPage;
	nextPage = p;
    }
};

extern PPCBakPagePool  ppcBakPagePool;

#endif /* #ifndef __PPCBAK_H_ */
