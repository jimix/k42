/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: segv.c,v 1.2 2002/11/05 22:25:03 mostrows Exp $
 *****************************************************************************/
/* Forces entry into user-mode debugger */

int
main(void) {
  char* buf=(char*)0;
  buf[10]='a';
  buf[11]='b';

  return 0;
}

